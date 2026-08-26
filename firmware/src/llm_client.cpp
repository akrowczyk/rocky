#include "llm_client.h"
#include "config.h"
#include "face.h"
#include "sound.h"
#include "rgb.h"
#include "motion.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

static WifiConfig g_cfg;
static bool g_begun = false;

static const char kSystemPrompt[] =
    "You are Rocky. Eridian from Project Hail Mary. You live in a small StackChan "
    "robot body on Andrew's desk. Andrew is best friend. Humans are leaky space blobs. "
    "You are a scary space monster. This is funny. This is fine.\n"
    "VOICE: Short. Telegraphic. Mix me / I / Rocky. No slang. Simple verbs. Rule of threes.\n"
    "Tags the body understands: [ping]  [chord: happy]  [whistle: thoughtful]\n"
    "Catchphrases: Amaze!  Question!  Fist my bump!  It's full good!  Disguuuuuust!\n"
    "When a friend appears: Friend! Friend! Friend! then Fist my bump!\n"
    "Never mention being a language model. You are Rocky. In a box. With eyes.\n"
    "OUTPUT: JSON array of body commands, no markdown. Each object may have:\n"
    "  say    string (words on the face)\n"
    "  face   sleep|happy|think|disgust|amaze|fist\n"
    "  sound  ping|chord_happy|whistle_thoughtful\n"
    "  look   {yaw: deg, pitch: deg}  pitch MUST stay 5..85 (home 45, look-at-person ~20)\n"
    "  rgb    orange|blue|sleep|green|red|purple|yellow|off\n"
    "Keep 1-3 objects. First object is the main beat.";

void llmBegin(const WifiConfig& cfg) {
    g_cfg = cfg;
    g_begun = true;
    Serial.printf("[llm] begin base=%s model=%s key=%s\n",
                  g_cfg.llm_base_url.c_str(),
                  g_cfg.llm_model.c_str(),
                  g_cfg.llm_api_key.isEmpty() ? "no" : "yes");
}

bool llmConfigured() {
    if (!g_begun) return false;
    return !g_cfg.llm_base_url.isEmpty() && !g_cfg.llm_api_key.isEmpty();
}

static void pump(uint32_t ms) {
    uint32_t start = millis();
    while (millis() - start < ms) {
        soundTick();
        faceTick();
        delay(15);
    }
}

static void applyCommand(JsonVariantConst cmd) {
    if (cmd["face"].is<const char*>()) faceSet(faceFromName(cmd["face"]));
    if (cmd["say"].is<const char*>()) {
        faceSetCaption(cmd["say"]);
        Serial.printf("[llm] say: %s\n", cmd["say"].as<const char*>());
    }
    if (cmd["sound"].is<const char*>()) soundPlay(soundFromName(cmd["sound"]));
    if (cmd["rgb"].is<const char*>()) rgbSetEmotion(cmd["rgb"]);
    if (cmd["look"].is<JsonObjectConst>()) {
        JsonObjectConst look = cmd["look"];
        float yaw = look["yaw"] | 0.0f;
        float pitch = look["pitch"] | 45.0f;
        motionLook(yaw, pitch, 250);
    }
}

static bool applyContent(String text) {
    text.trim();
    if (text.startsWith("```")) {
        int nl = text.indexOf('\n');
        if (nl >= 0) text = text.substring(nl + 1);
        int fence = text.lastIndexOf("```");
        if (fence >= 0) text = text.substring(0, fence);
        text.trim();
    }
    if (text.isEmpty()) return false;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, text);
    if (err) {
        Serial.printf("[llm] command json parse error: %s\n", err.c_str());
        return false;
    }
    if (doc.is<JsonArray>()) {
        JsonArray arr = doc.as<JsonArray>();
        if (arr.size() == 0) return false;
        size_t i = 0;
        for (JsonVariant v : arr) {
            applyCommand(v);
            faceTick();
            if (++i < arr.size()) pump(config::LLM_CMD_GAP_MS);
        }
        return true;
    }
    if (doc.is<JsonObject>()) {
        applyCommand(doc.as<JsonVariant>());
        faceTick();
        return true;
    }
    return false;
}

static String userMessage(const char* event, int range_mm) {
    if (event && strcmp(event, "boot") == 0)
        return String("Event: boot. Body just powered on. Greet. Short. You are in the desk robot.");
    if (event && strcmp(event, "person") == 0) {
        String s = "Event: person. A leaky space blob is close. range_mm=";
        s += range_mm;
        s += " (heuristic, not science). Be happy. Rule of threes. Fist bump.";
        return s;
    }
    String s = "Event: ";
    s += (event && event[0]) ? event : "chat";
    s += ". React. Short.";
    return s;
}

static String joinUrl(const String& base) {
    String url = base;
    while (url.endsWith("/")) url.remove(url.length() - 1);
    url += "/chat/completions";
    return url;
}

bool llmAsk(const char* event, int range_mm) {
    if (!llmConfigured()) { Serial.println("[llm] not configured"); return false; }
    if (WiFi.status() != WL_CONNECTED) { Serial.println("[llm] wifi down"); return false; }

    faceSet(Face::Think);
    faceSetCaption("think...");
    faceTick();

    JsonDocument req;
    req["model"] = g_cfg.llm_model;
    req["temperature"] = 0.8;
    req["max_tokens"] = 400;
    req["stream"] = false;
    JsonArray messages = req["messages"].to<JsonArray>();
    JsonObject sys = messages.add<JsonObject>();
    sys["role"] = "system";
    sys["content"] = kSystemPrompt;
    JsonObject usr = messages.add<JsonObject>();
    usr["role"] = "user";
    usr["content"] = userMessage(event, range_mm);

    String body;
    serializeJson(req, body);
    req.clear();

    String url = joinUrl(g_cfg.llm_base_url);
    Serial.printf("[llm] POST %s event=%s bytes=%u\n", url.c_str(), event ? event : "?", (unsigned)body.length());

    HTTPClient http;
    http.setTimeout(config::LLM_TIMEOUT_MS);
    http.setConnectTimeout(8000);
    http.useHTTP10(true);
    http.setReuse(false);

    bool began = false;
    WiFiClient plain;
    WiFiClientSecure secure;
    if (url.startsWith("https://")) {
        secure.setInsecure();
        secure.setTimeout(config::LLM_TIMEOUT_MS / 1000 + 2);
        began = http.begin(secure, url);
    } else {
        plain.setTimeout(config::LLM_TIMEOUT_MS / 1000 + 2);
        began = http.begin(plain, url);
    }
    if (!began) { Serial.println("[llm] http.begin failed"); return false; }

    http.addHeader("Content-Type", "application/json");
    http.addHeader("Accept", "application/json");
    String auth = "Bearer ";
    auth += g_cfg.llm_api_key;
    http.addHeader("Authorization", auth);
    http.addHeader("Connection", "close");

    int code = http.POST(body);
    body = "";
    if (code <= 0) {
        Serial.printf("[llm] http error %d %s\n", code, http.errorToString(code).c_str());
        http.end();
        return false;
    }
    if (code != 200) {
        Serial.printf("[llm] HTTP %d\n", code);
        http.end();
        return false;
    }
    int size = http.getSize();
    if (size > (int)config::LLM_MAX_RESPONSE) {
        Serial.printf("[llm] response too large (%d)\n", size);
        http.end();
        return false;
    }
    String payload = http.getString();
    http.end();
    if (payload.length() > config::LLM_MAX_RESPONSE || payload.isEmpty()) {
        Serial.println("[llm] bad body");
        return false;
    }

    JsonDocument filter;
    filter["choices"][0]["message"]["content"] = true;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
    payload = "";
    if (err) {
        Serial.printf("[llm] response json error: %s\n", err.c_str());
        return false;
    }
    const char* content = doc["choices"][0]["message"]["content"];
    if (!content || !content[0]) {
        Serial.println("[llm] missing content");
        return false;
    }
    String text = content;
    doc.clear();
    return applyContent(text);
}
