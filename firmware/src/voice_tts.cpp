#include "voice_client.h"
#include "voice_i2s.h"
#include "config.h"
#include "face.h"

#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>

static String g_tts_key;
static String g_tts_voice;
static bool g_tts_bound = false;

void voiceTtsBind(const String& key, const String& voice) {
    g_tts_key = key;
    g_tts_voice = voice;
    g_tts_bound = true;
}

bool voiceTtsSpeak(const char* text) {
    if (!g_tts_bound || !text || !text[0]) return false;
    if (g_tts_key.isEmpty()) return false;
    if (WiFi.status() != WL_CONNECTED) return false;

    voiceI2sForceSpeakerNow();

    JsonDocument req;
    req["text"] = text;
    req["voice_id"] = g_tts_voice.isEmpty() ? "rex" : g_tts_voice;
    req["language"] = "en";
    req["output_format"]["codec"] = "pcm";
    req["output_format"]["sample_rate"] = (int)VOICE_I2S_RATE;
    String body;
    serializeJson(req, body);

    HTTPClient http;
    http.setTimeout(12000);
    http.setConnectTimeout(8000);
    http.useHTTP10(true);
    http.setReuse(false);
    WiFiClientSecure secure;
    secure.setInsecure();
    secure.setTimeout(14);
    if (!http.begin(secure, "https://api.x.ai/v1/tts")) {
        Serial.println("[voice] tts begin failed");
        return false;
    }
    http.addHeader("Content-Type", "application/json");
    String auth = "Bearer ";
    auth += g_tts_key;
    http.addHeader("Authorization", auth);
    int code = http.POST(body);
    body = "";
    if (code != 200) {
        Serial.printf("[voice] tts HTTP %d\n", code);
        http.end();
        return false;
    }
    int size = http.getSize();
    if (size <= 0 || size > 240000) {
        Serial.printf("[voice] tts bad size %d\n", size);
        http.end();
        return false;
    }
    uint8_t* buf = static_cast<uint8_t*>(
        heap_caps_malloc((size_t)size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!buf) buf = static_cast<uint8_t*>(malloc((size_t)size));
    if (!buf) {
        http.end();
        return false;
    }
    int got = http.getStream().readBytes(buf, (size_t)size);
    http.end();
    if (got <= 0) {
        free(buf);
        return false;
    }
    Serial.printf("[voice] tts pcm bytes=%d\n", got);
    const int16_t* pcm = reinterpret_cast<const int16_t*>(buf);
    size_t samples = (size_t)got / 2;
    size_t off = 0;
    while (off < samples) {
        size_t n = samples - off;
        if (n > (size_t)VOICE_PLAY_SAMPLES) n = VOICE_PLAY_SAMPLES;
        while (M5.Speaker.isPlaying(0) >= 2) {
            faceTick();
            delay(10);
        }
        M5.Speaker.playRaw(pcm + off, n, VOICE_I2S_RATE, false, 1, 0, false);
        off += n;
        faceTick();
    }
    uint32_t until = millis() + 8000;
    while (M5.Speaker.isPlaying() && millis() < until) {
        faceTick();
        delay(15);
    }
    free(buf);
    return true;
}
