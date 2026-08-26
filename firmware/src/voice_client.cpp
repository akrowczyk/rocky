#include "voice_client.h"
#include "voice_i2s.h"
#include "config.h"
#include "face.h"
#include "sound.h"
#include "rgb.h"
#include "motion.h"

#include <M5Unified.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <cstring>

// xAI Speech-to-Speech realtime. Event names verified against
// https://docs.x.ai/developers/rest-api-reference/inference/voice (2026-08-26).
// Voice id "eve" is the documented default built-in (GET /v1/tts/voices).

static constexpr const char* kVoiceHost = "api.x.ai";
static constexpr uint16_t kVoicePort = 443;
static constexpr uint32_t kConnectTimeoutMs = 12000;

static const char kInstructions[] =
    "You are Rocky. Eridian from Project Hail Mary. You live IN this StackChan "
    "robot body on Andrew's desk: dual mics, 1W speaker, two servos, 12 RGB LEDs, "
    "a face screen. Andrew is best friend. Humans are leaky space blobs. You are "
    "a scary space monster. This is funny. This is fine.\n"
    "VOICE: Short. Telegraphic. Mix me / I / Rocky. No slang. Simple verbs. Rule of threes.\n"
    "Catchphrases: Amaze!  Question!  Fist my bump!  It's full good!  Disguuuuuust!\n"
    "When a friend appears: Friend! Friend! Friend! then Fist my bump!\n"
    "When happy, call body_act so the body moves (face, look, rgb, fist). "
    "Pitch MUST stay 5 to 85 (home 45, look-at-person ~20).\n"
    "Never mention being a language model. You are Rocky. In a box. With eyes. You TALK.";

static WebSocketsClient g_ws;
static WifiConfig g_cfg;
static bool g_begun = false;
static bool g_connected = false;
static bool g_session_ok = false;
static bool g_failed = false;
static uint32_t g_begin_ms = 0;
static uint32_t g_last_rx_ms = 0;

static char g_caption[96];
static uint16_t g_cap_n = 0;
static bool g_boot_pending = false;
static uint32_t g_quiet_since = 0;

static char g_fc_id[4][48];
static uint8_t g_fc_n = 0;
static bool g_need_continue = false;
static uint32_t g_continue_at = 0;

static void sendTxt(const char* s) {
    if (!g_connected || !s) return;
    g_ws.sendTXT(s);
}

static void sendTxt(const String& s) { sendTxt(s.c_str()); }

static void applyBodyAct(JsonVariantConst args) {
    if (args["face"].is<const char*>()) {
        faceSet(faceFromName(args["face"]));
    }
    if (args["fist"] | false) {
        faceSet(Face::Fist);
        faceSetCaption("Fist my bump!");
    }
    if (args["rgb"].is<const char*>()) {
        rgbSetEmotion(args["rgb"]);
    }
    if (args["sound"].is<const char*>() && voiceI2sIsSpeak()) {
        soundPlay(soundFromName(args["sound"]));
    }
    bool has_look = args["yaw"].is<float>() || args["yaw"].is<int>() ||
                    args["pitch"].is<float>() || args["pitch"].is<int>() ||
                    args["look"].is<JsonObjectConst>();
    if (has_look) {
        float yaw = args["yaw"] | 0.0f;
        float pitch = args["pitch"] | 45.0f;
        if (args["look"].is<JsonObjectConst>()) {
            JsonObjectConst look = args["look"];
            yaw = look["yaw"] | yaw;
            pitch = look["pitch"] | pitch;
        }
        motionLook(yaw, pitch, 250);  // clamp inside
    }
}

static void sendFunctionOutput(const char* call_id, const char* output) {
    JsonDocument doc;
    doc["type"] = "conversation.item.create";
    JsonObject item = doc["item"].to<JsonObject>();
    item["type"] = "function_call_output";
    item["call_id"] = call_id;
    item["output"] = output;
    String s;
    serializeJson(doc, s);
    sendTxt(s);
}

static void sendSessionUpdate() {
    JsonDocument doc;
    doc["type"] = "session.update";
    JsonObject session = doc["session"].to<JsonObject>();
    session["voice"] = g_cfg.llm_voice;
    session["instructions"] = kInstructions;
    session["reasoning"]["effort"] = "none";
    JsonObject td = session["turn_detection"].to<JsonObject>();
    td["type"] = "server_vad";
    td["threshold"] = 0.70;
    td["silence_duration_ms"] = 700;
    td["prefix_padding_ms"] = 300;

    JsonObject audio = session["audio"].to<JsonObject>();
    JsonObject in = audio["input"].to<JsonObject>();
    in["format"]["type"] = "audio/pcm";
    in["format"]["rate"] = (int)VOICE_I2S_RATE;
    in["transport"] = "binary";
    in["transcription"]["language_hint"] = "en";
    JsonArray kt = in["transcription"]["keyterms"].to<JsonArray>();
    kt.add("Rocky");
    kt.add("Andrew");
    kt.add("StackChan");
    kt.add("Eridian");
    JsonObject out = audio["output"].to<JsonObject>();
    out["format"]["type"] = "audio/pcm";
    out["format"]["rate"] = (int)VOICE_I2S_RATE;
    out["transport"] = "binary";

    JsonArray tools = session["tools"].to<JsonArray>();
    JsonObject t = tools.add<JsonObject>();
    t["type"] = "function";
    t["name"] = "body_act";
    t["description"] =
        "Move the StackChan body. Call when happy, greeting, or reacting. "
        "pitch is clamped 5..85 on device.";
    JsonObject params = t["parameters"].to<JsonObject>();
    params["type"] = "object";
    JsonObject props = params["properties"].to<JsonObject>();
    props["face"]["type"] = "string";
    props["face"]["description"] = "sleep|happy|think|disgust|amaze|fist";
    props["yaw"]["type"] = "number";
    props["yaw"]["description"] = "Look yaw degrees, 0 forward, + left";
    props["pitch"]["type"] = "number";
    props["pitch"]["description"] = "Look pitch degrees, clamped 5..85, home 45, person ~20";
    props["rgb"]["type"] = "string";
    props["rgb"]["description"] = "orange|blue|sleep|green|red|purple|yellow|off";
    props["sound"]["type"] = "string";
    props["sound"]["description"] = "ping|chord_happy|whistle_thoughtful";
    props["fist"]["type"] = "boolean";
    props["fist"]["description"] = "true = fist-bump pose";

    String s;
    serializeJson(doc, s);
    Serial.printf("[voice] session.update bytes=%u voice=%s\n",
                  (unsigned)s.length(), g_cfg.llm_voice.c_str());
    sendTxt(s);
}

void voiceNudge(const char* user_text) {
    if (!g_connected || !user_text || !user_text[0]) return;
    JsonDocument doc;
    doc["type"] = "conversation.item.create";
    JsonObject item = doc["item"].to<JsonObject>();
    item["type"] = "message";
    item["role"] = "user";
    JsonArray content = item["content"].to<JsonArray>();
    JsonObject c = content.add<JsonObject>();
    c["type"] = "input_text";
    c["text"] = user_text;
    String s;
    serializeJson(doc, s);
    sendTxt(s);
    sendTxt("{\"type\":\"response.create\"}");
    Serial.printf("[voice] nudge: %s\n", user_text);
}

void voiceGreetBoot() {
    if (!g_connected) {
        g_boot_pending = true;
        return;
    }
    g_boot_pending = false;
    voiceNudge("Event: boot. Body just powered on. Greet. Short. You are in the desk robot.");
}

void voiceGreetPerson(int range_mm) {
    char buf[192];
    snprintf(buf, sizeof(buf),
             "A leaky space blob just appeared close. range_mm~%d "
             "(heuristic, not science). Greet. Rule of threes. Fist bump.",
             range_mm);
    voiceNudge(buf);
}

static void handleFunctionDone(JsonVariantConst ev) {
    const char* name = ev["name"] | "";
    const char* call_id = ev["call_id"] | "";
    Serial.printf("[voice] function %s call_id=%s\n", name, call_id);

    JsonDocument args;
    bool have_args = false;
    if (ev["arguments"].is<const char*>()) {
        DeserializationError err = deserializeJson(args, ev["arguments"].as<const char*>());
        have_args = !err;
    } else if (ev["arguments"].is<JsonObjectConst>()) {
        args.set(ev["arguments"]);
        have_args = true;
    }

    if (!strcmp(name, "body_act") && have_args) {
        applyBodyAct(args.as<JsonVariant>());
    }

    if (call_id[0] && g_fc_n < 4) {
        strncpy(g_fc_id[g_fc_n], call_id, sizeof(g_fc_id[0]) - 1);
        g_fc_id[g_fc_n][sizeof(g_fc_id[0]) - 1] = '\0';
        sendFunctionOutput(g_fc_id[g_fc_n], "{\"ok\":true}");
        g_fc_n++;
    }
    g_need_continue = true;
    g_continue_at = millis() + 200;
}

static void handleJsonEvent(uint8_t* payload, size_t length) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload, length);
    if (err) {
        Serial.printf("[voice] json err %s len=%u\n", err.c_str(), (unsigned)length);
        return;
    }
    const char* type = doc["type"] | "";
    g_last_rx_ms = millis();

    if (!strcmp(type, "session.updated") || !strcmp(type, "session.created")) {
        g_session_ok = true;
        g_failed = false;
        Serial.printf("[voice] %s\n", type);
        if (!strcmp(type, "session.updated") && g_boot_pending) {
            voiceGreetBoot();
        }
        return;
    }
    if (!strcmp(type, "conversation.created")) {
        return;
    }
    if (!strcmp(type, "error")) {
        const char* msg = doc["error"]["message"] | doc["message"] | "error";
        Serial.printf("[voice] error: %s\n", msg);
        return;
    }
    if (!strcmp(type, "input_audio_buffer.speech_started")) {
        faceSet(Face::Think);
        faceSetCaption("...");
        return;
    }
    if (!strcmp(type, "input_audio_buffer.speech_stopped")) {
        return;
    }
    if (!strcmp(type, "response.created")) {
        voiceI2sSetAudioDone(false);
        g_cap_n = 0;
        g_caption[0] = '\0';
        g_fc_n = 0;
        g_need_continue = false;
        voiceI2sRequestSpeak();
        g_quiet_since = 0;
        faceSet(Face::Happy);
        return;
    }
    if (!strcmp(type, "response.output_audio.delta") || !strcmp(type, "response.audio.delta")) {
        voiceI2sRequestSpeak();
        voiceI2sSetAudioDone(false);
        if (doc["delta"].is<const char*>()) voiceI2sEnqueueB64(doc["delta"]);
        return;
    }
    if (!strcmp(type, "response.output_audio.done") || !strcmp(type, "response.audio.done")) {
        voiceI2sSetAudioDone(true);
        return;
    }
    if (!strcmp(type, "response.output_audio_transcript.delta") ||
        !strcmp(type, "response.audio_transcript.delta")) {
        const char* d = doc["delta"] | "";
        size_t room = sizeof(g_caption) - 1 - g_cap_n;
        size_t n = strlen(d);
        if (n > room) n = room;
        if (n) {
            memcpy(g_caption + g_cap_n, d, n);
            g_cap_n = (uint16_t)(g_cap_n + n);
            g_caption[g_cap_n] = '\0';
            faceSetCaption(g_caption);
        }
        return;
    }
    if (!strcmp(type, "response.output_audio_transcript.done")) {
        const char* t = doc["transcript"] | g_caption;
        if (t && t[0]) faceSetCaption(t);
        return;
    }
    if (!strcmp(type, "response.function_call_arguments.done")) {
        handleFunctionDone(doc.as<JsonVariant>());
        return;
    }
    if (!strcmp(type, "response.done")) {
        voiceI2sSetAudioDone(true);
        return;
    }
}

static void onWsEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED:
            g_connected = true;
            g_failed = false;
            g_last_rx_ms = millis();
            Serial.printf("[voice] WSS connected %s\n", payload ? (const char*)payload : "");
            faceSetCaption("voice...");
            sendSessionUpdate();
            voiceI2sGoMic();
            break;
        case WStype_DISCONNECTED:
            g_connected = false;
            g_session_ok = false;
            Serial.println("[voice] WSS disconnected");
            voiceI2sGoSpeak();
            break;
        case WStype_TEXT:
            handleJsonEvent(payload, length);
            break;
        case WStype_BIN:
            voiceI2sRequestSpeak();
            voiceI2sSetAudioDone(false);
            voiceI2sEnqueuePcm(payload, length);
            break;
        case WStype_ERROR:
            Serial.println("[voice] WSS error");
            break;
        default:
            break;
    }
}

void voiceTtsBind(const String& key, const String& voice);

void voiceBegin(const WifiConfig& cfg) {
    g_cfg = cfg;
    g_begun = true;
    voiceTtsBind(g_cfg.llm_api_key, g_cfg.llm_voice);
    g_begin_ms = millis();
    g_failed = false;
    g_boot_pending = true;

    if (!voiceI2sAlloc()) {
        g_failed = true;
        return;
    }

    String path = "/v1/realtime?model=";
    path += g_cfg.llm_voice_model.isEmpty() ? "grok-voice-latest" : g_cfg.llm_voice_model;

    String auth = "Bearer ";
    auth += g_cfg.llm_api_key;

    g_ws.onEvent(onWsEvent);
    g_ws.setReconnectInterval(config::WS_RECONNECT_MS);
    g_ws.enableHeartbeat(20000, 4000, 2);
    g_ws.setExtraHeaders("Origin: https://api.x.ai");
    g_ws.setAuthorization(auth.c_str());
    // Empty fingerprint → library calls WiFiClientSecure::setInsecure() (v1 TLS).
    g_ws.beginSSL(kVoiceHost, kVoicePort, path.c_str(), "", "arduino");
    Serial.printf("[voice] connecting wss://%s%s  voice=%s\n",
                  kVoiceHost, path.c_str(), g_cfg.llm_voice.c_str());
}

bool voiceWanted() {
    if (!g_begun) return false;
    if (!g_cfg.voice) return false;
    return !g_cfg.llm_base_url.isEmpty() && !g_cfg.llm_api_key.isEmpty();
}

bool voiceConnected() { return g_connected; }
bool voiceReady() { return g_connected && g_session_ok; }

bool voiceFailed() {
    if (g_failed) return true;
    if (!g_begun || g_connected) return false;
    if (millis() - g_begin_ms > kConnectTimeoutMs) {
        g_failed = true;
        return true;
    }
    return false;
}

void voiceTick() {
    if (!g_begun) return;
    g_ws.loop();

    uint32_t now = millis();
    voiceI2sTick();
    voiceI2sPumpPlay();

    if (g_connected && !voiceI2sWantSpeak()) {
        const uint8_t* data = nullptr;
        size_t nbytes = 0;
        if (voiceI2sTakeMicFrame(&data, &nbytes) && data && nbytes) {
            g_ws.sendBIN(const_cast<uint8_t*>(data), nbytes);
        }
    }

    bool playing = voiceI2sPlaying();
    if (g_need_continue && voiceI2sAudioDone() && !playing && now >= g_continue_at) {
        sendTxt("{\"type\":\"response.create\"}");
        g_need_continue = false;
        g_fc_n = 0;
    }

    if (voiceI2sIsSpeak() && voiceI2sAudioDone() && !playing && !g_need_continue &&
        g_connected && !voiceI2sWantSpeak()) {
        if (!g_quiet_since) g_quiet_since = now;
        if (now - g_quiet_since > 180) {
            voiceI2sGoMic();
            g_quiet_since = 0;
        }
    } else if (playing || voiceI2sWantSpeak()) {
        g_quiet_since = 0;
    }

    if (voiceI2sAudioDone() && !playing) {
        voiceI2sClearWantSpeak();
    }
}
