#include "voice_client.h"
#include "config.h"
#include "face.h"
#include "sound.h"
#include "rgb.h"
#include "motion.h"

#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <cstring>

// xAI Speech-to-Speech realtime. Event names verified against
// https://docs.x.ai/developers/rest-api-reference/inference/voice (2026-08-26).
// Voice id "eve" is the documented default built-in (GET /v1/tts/voices).

static constexpr const char* kVoiceHost = "api.x.ai";
static constexpr uint16_t kVoicePort = 443;
static constexpr uint32_t kSampleRate = 16000;
static constexpr int kMicSamples = 1600;    // 100 ms
static constexpr int kPlaySamples = 2048;   // ~128 ms
static constexpr int kPlaySlots = 4;
static constexpr uint32_t kI2sSettleMs = 80;
static constexpr uint32_t kConnectTimeoutMs = 12000;
