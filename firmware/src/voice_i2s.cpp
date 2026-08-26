#include "voice_i2s.h"
#include "config.h"
#include "sound.h"

#include <M5Unified.h>
#include <esp_heap_caps.h>
#include <cstring>

// Play ring: 4 slots x 2048 samples. Buffers live in PSRAM when available.

static constexpr int kPlaySlots = 4;

enum class Bus : uint8_t { Idle, Mic, Speak, WaitSpeak, WaitMic };

static Bus g_bus = Bus::Idle;
static uint32_t g_bus_at = 0;
static bool g_want_speak = false;
static bool g_audio_done = true;

static int16_t* g_mic[2] = {nullptr, nullptr};
static uint8_t g_mic_i = 0;
static bool g_mic_have = false;

static int16_t* g_play[kPlaySlots] = {};
static size_t g_play_len[kPlaySlots] = {};
static uint8_t g_play_w = 0, g_play_r = 0, g_play_n = 0;

static int16_t* allocI16(size_t n) {
    void* p = heap_caps_malloc(n * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) p = malloc(n * sizeof(int16_t));
    if (p) memset(p, 0, n * sizeof(int16_t));
    return static_cast<int16_t*>(p);
}

bool voiceI2sAlloc() {
    if (!g_mic[0]) g_mic[0] = allocI16(VOICE_MIC_SAMPLES);
    if (!g_mic[1]) g_mic[1] = allocI16(VOICE_MIC_SAMPLES);
    for (int i = 0; i < kPlaySlots; i++) {
        if (!g_play[i]) g_play[i] = allocI16(VOICE_PLAY_SAMPLES);
    }
    if (!g_mic[0] || !g_play[0]) {
        Serial.println("[voice] audio buffer alloc failed");
        return false;
    }
    return true;
}

void voiceI2sRequestSpeak() { g_want_speak = true; }
void voiceI2sClearWantSpeak() { g_want_speak = false; }
bool voiceI2sWantSpeak() { return g_want_speak; }
void voiceI2sSetAudioDone(bool done) { g_audio_done = done; }
bool voiceI2sAudioDone() { return g_audio_done; }
bool voiceI2sIsSpeak() { return g_bus == Bus::Speak; }
bool voiceI2sIsMic() { return g_bus == Bus::Mic; }

bool voiceI2sPlaying() {
    return (g_bus == Bus::Speak) && (M5.Speaker.isPlaying() || g_play_n > 0);
}

void voiceI2sGoSpeak() {
    soundStop();
    if (M5.Mic.isRunning()) M5.Mic.end();
    if (M5.Speaker.isRunning()) {
        M5.Speaker.stop();
        M5.Speaker.end();
    }
    g_bus = Bus::WaitSpeak;
    g_bus_at = millis();
    g_mic_have = false;
}

void voiceI2sGoMic() {
    soundStop();
    if (M5.Speaker.isRunning()) {
        M5.Speaker.stop();
        M5.Speaker.end();
    }
    g_bus = Bus::WaitMic;
    g_bus_at = millis();
    g_play_n = 0;
    g_play_w = g_play_r = 0;
}

static void finishSpeakBegin() {
    M5.Speaker.begin();
    M5.Speaker.setVolume(config::SPEAKER_VOLUME);
    g_bus = Bus::Speak;
    Serial.println("[voice] I2S speaker");
}

static void finishMicBegin() {
    auto cfg = M5.Mic.config();
    cfg.sample_rate = VOICE_I2S_RATE;
    cfg.stereo = false;
    M5.Mic.config(cfg);
    if (!M5.Mic.begin()) {
        Serial.println("[voice] Mic.begin failed");
        g_bus = Bus::Idle;
        return;
    }
    g_bus = Bus::Mic;
    g_mic_have = false;
    g_mic_i = 0;
    if (g_mic[0]) M5.Mic.record(g_mic[0], VOICE_MIC_SAMPLES, VOICE_I2S_RATE, false);
    Serial.println("[voice] I2S mic");
}

void voiceI2sTick() {
    uint32_t now = millis();
    if (g_bus == Bus::WaitSpeak && now - g_bus_at >= VOICE_I2S_SETTLE_MS) {
        finishSpeakBegin();
    }
    if (g_bus == Bus::WaitMic && now - g_bus_at >= VOICE_I2S_SETTLE_MS) {
        finishMicBegin();
    }
    if (g_want_speak && (g_bus == Bus::Mic || g_bus == Bus::Idle)) {
        voiceI2sGoSpeak();
    }
}

void voiceI2sPumpPlay() {
    if (g_bus != Bus::Speak) return;
    if (!g_play_n) return;
    if (M5.Speaker.isPlaying(0) >= 2) return;
    M5.Speaker.playRaw(g_play[g_play_r], g_play_len[g_play_r], VOICE_I2S_RATE, false, 1, 0, false);
    g_play_r = (uint8_t)((g_play_r + 1) % kPlaySlots);
    g_play_n--;
}

bool voiceI2sEnqueuePcm(const uint8_t* bytes, size_t nbytes) {
    if (!bytes || nbytes < 2) return false;
    size_t samples = nbytes / 2;
    const int16_t* src = reinterpret_cast<const int16_t*>(bytes);
    while (samples) {
        if (g_play_n >= kPlaySlots) {
            Serial.println("[voice] play ring full — drop");
            return false;
        }
        size_t chunk = samples;
        if (chunk > (size_t)VOICE_PLAY_SAMPLES) chunk = VOICE_PLAY_SAMPLES;
        memcpy(g_play[g_play_w], src, chunk * 2);
        g_play_len[g_play_w] = chunk;
        g_play_w = (uint8_t)((g_play_w + 1) % kPlaySlots);
        g_play_n++;
        src += chunk;
        samples -= chunk;
    }
    return true;
}

static int b64val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

void voiceI2sEnqueueB64(const char* b64) {
    if (!b64 || !b64[0]) return;
    uint8_t tmp[1024];
    size_t out = 0;
    int val = 0, valb = -8;
    for (const char* p = b64; *p; ++p) {
        if (*p == '=' || *p == '\n' || *p == '\r') continue;
        int d = b64val(*p);
        if (d < 0) continue;
        val = (val << 6) + d;
        valb += 6;
        if (valb >= 0) {
            tmp[out++] = (uint8_t)((val >> valb) & 0xFF);
            valb -= 8;
            if (out >= sizeof(tmp)) {
                voiceI2sEnqueuePcm(tmp, out);
                out = 0;
            }
        }
    }
    if (out) voiceI2sEnqueuePcm(tmp, out);
}

bool voiceI2sTakeMicFrame(const uint8_t** data, size_t* nbytes) {
    if (g_bus != Bus::Mic || g_want_speak) return false;
    if (!g_mic[0] || !g_mic[1]) return false;
    if (M5.Mic.isRecording()) return false;
    bool ready = g_mic_have;
    if (ready) {
        *data = reinterpret_cast<const uint8_t*>(g_mic[g_mic_i]);
        *nbytes = (size_t)VOICE_MIC_SAMPLES * 2;
    }
    g_mic_i ^= 1;
    M5.Mic.record(g_mic[g_mic_i], VOICE_MIC_SAMPLES, VOICE_I2S_RATE, false);
    g_mic_have = true;
    return ready;
}

void voiceI2sForceSpeakerNow() {
    soundStop();
    if (M5.Mic.isRunning()) M5.Mic.end();
    if (!M5.Speaker.isRunning()) {
        M5.Speaker.begin();
        M5.Speaker.setVolume(config::SPEAKER_VOLUME);
    }
    g_bus = Bus::Speak;
    delay((int)VOICE_I2S_SETTLE_MS);
}
