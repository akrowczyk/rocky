#include "brain_client.h"
#include "face.h"
#include "sound.h"
#include "rgb.h"
#include "motion.h"
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

static WebSocketsClient g_ws;
static WifiConfig g_cfg;
static bool g_connected = false;
static bool g_started = false;

static void applyCommand(JsonVariantConst cmd) {
    if (cmd["face"].is<const char*>()) {
        faceSet(faceFromName(cmd["face"]));
    }
    if (cmd["say"].is<const char*>()) {
        faceSetCaption(cmd["say"]);
        Serial.printf("[brain] say: %s\n", cmd["say"].as<const char*>());
    }
    if (cmd["sound"].is<const char*>()) {
        soundPlay(soundFromName(cmd["sound"]));
    }
    if (cmd["rgb"].is<const char*>()) {
        rgbSetEmotion(cmd["rgb"]);
    }
    if (cmd["look"].is<JsonObjectConst>()) {
        JsonObjectConst look = cmd["look"];
        float yaw = look["yaw"] | 0.0f;
        float pitch = look["pitch"] | 45.0f;
        motionLook(yaw, pitch, 250);
    }
}

static void handlePayload(uint8_t* payload, size_t length) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload, length);
    if (err) {
        Serial.printf("[brain] bad json: %s\n", err.c_str());
        return;
    }
    if (doc.is<JsonArray>()) {
        for (JsonVariant v : doc.as<JsonArray>()) applyCommand(v);
    } else {
        applyCommand(doc.as<JsonVariant>());
    }
}

static void onWsEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED:
            g_connected = true;
            Serial.println("[brain] websocket connected");
            brainSendBoot();
            break;
        case WStype_DISCONNECTED:
            g_connected = false;
            Serial.println("[brain] websocket disconnected");
            break;
        case WStype_TEXT:
            handlePayload(payload, length);
            break;
        default:
            break;
    }
}

void brainBegin(const WifiConfig& cfg) {
    g_cfg = cfg;
    g_started = true;
    g_ws.onEvent(onWsEvent);
    g_ws.setReconnectInterval(4000);
    g_ws.begin(cfg.brain_host.c_str(), cfg.brain_port, cfg.brain_path.c_str());
    Serial.printf("[brain] connecting ws://%s:%u%s\n",
                  cfg.brain_host.c_str(), cfg.brain_port, cfg.brain_path.c_str());
}

void brainTick() {
    if (g_started) g_ws.loop();
}

bool brainConnected() { return g_connected; }

void brainSendJson(const char* json) {
    if (!g_connected) return;
    g_ws.sendTXT(json);
}

void brainSendBoot() { brainSendJson("{\"event\":\"boot\"}"); }

void brainSendPerson(int range_mm) {
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"event\":\"person\",\"range_mm\":%d}", range_mm);
    brainSendJson(buf);
}
