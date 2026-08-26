#include "sound.h"
#include "config.h"
#include <M5Unified.h>

struct Step {
    uint16_t freq;
    uint16_t ms;
};

static const Step* g_seq = nullptr;
static int g_seq_len = 0;
static int g_seq_i = 0;
static uint32_t g_step_until = 0;
static bool g_busy = false;

static const Step kPing[] = {{880, 90}, {0, 40}, {1320, 90}};
static const Step kChord[] = {
    {523, 110}, {0, 30}, {659, 110}, {0, 30}, {784, 180}, {0, 40}, {1046, 220},
};
static const Step kWhistle[] = {
    {740, 80}, {784, 80}, {830, 90}, {880, 110}, {830, 90}, {784, 140},
};

SoundFx soundFromName(const char* name) {
    if (!name) return SoundFx::None;
    if (!strcasecmp(name, "ping")) return SoundFx::Ping;
    if (!strcasecmp(name, "chord_happy") || !strcasecmp(name, "chord")) return SoundFx::ChordHappy;
    if (!strcasecmp(name, "whistle_thoughtful") || !strcasecmp(name, "whistle"))
        return SoundFx::WhistleThoughtful;
    return SoundFx::None;
}

void soundBegin() {
    auto cfg = M5.Speaker.config();
    M5.Speaker.config(cfg);
    M5.Speaker.begin();
    M5.Speaker.setVolume(config::SPEAKER_VOLUME);
}

static void startSeq(const Step* s, int n) {
    g_seq = s;
    g_seq_len = n;
    g_seq_i = 0;
    g_busy = true;
    g_step_until = 0;
}

void soundPlay(SoundFx fx) {
    switch (fx) {
        case SoundFx::Ping:              startSeq(kPing, 3); break;
        case SoundFx::ChordHappy:        startSeq(kChord, 7); break;
        case SoundFx::WhistleThoughtful: startSeq(kWhistle, 6); break;
        default: break;
    }
}

void soundTick() {
    if (!g_busy || !g_seq) return;
    uint32_t now = millis();
    if (now < g_step_until) return;
    if (g_seq_i >= g_seq_len) {
        M5.Speaker.stop();
        g_busy = false;
        g_seq = nullptr;
        return;
    }
    const Step& st = g_seq[g_seq_i++];
    if (st.freq == 0) {
        M5.Speaker.stop();
    } else {
        M5.Speaker.tone(st.freq, st.ms);
    }
    g_step_until = now + st.ms;
}

bool soundBusy() { return g_busy; }
