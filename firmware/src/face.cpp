#include "face.h"
#include "config.h"
#include <M5Unified.h>
#include <cstring>

static Face g_face = Face::Sleep;
static bool g_dirty = true;
static char g_caption[96] = "";
static uint32_t g_blink_at = 0;
static bool g_blinking = false;
static uint32_t g_blink_until = 0;

const char* faceName(Face f) {
    switch (f) {
        case Face::Sleep:   return "sleep";
        case Face::Happy:   return "happy";
        case Face::Think:   return "think";
        case Face::Disgust: return "disgust";
        case Face::Amaze:   return "amaze";
        case Face::Fist:    return "fist";
    }
    return "sleep";
}

Face faceFromName(const char* name) {
    if (!name) return Face::Happy;
    if (!strcasecmp(name, "sleep")) return Face::Sleep;
    if (!strcasecmp(name, "happy")) return Face::Happy;
    if (!strcasecmp(name, "think")) return Face::Think;
    if (!strcasecmp(name, "disgust")) return Face::Disgust;
    if (!strcasecmp(name, "amaze")) return Face::Amaze;
    if (!strcasecmp(name, "fist")) return Face::Fist;
    return Face::Happy;
}

static uint16_t bgFor(Face f) {
    switch (f) {
        case Face::Sleep:   return M5.Display.color565(8, 10, 28);
        case Face::Happy:   return M5.Display.color565(18, 12, 4);
        case Face::Think:   return M5.Display.color565(8, 12, 22);
        case Face::Disgust: return M5.Display.color565(18, 22, 8);
        case Face::Amaze:   return M5.Display.color565(22, 8, 22);
        case Face::Fist:    return M5.Display.color565(24, 10, 4);
    }
    return TFT_BLACK;
}

static void drawEye(int cx, int cy, int rx, int ry, int pupil_x, int pupil_y, int pupil_r, bool closed) {
    auto& d = M5.Display;
    if (closed) {
        d.fillRoundRect(cx - rx, cy - 6, rx * 2, 12, 4, TFT_WHITE);
        d.fillRect(cx - rx + 4, cy - 2, rx * 2 - 8, 4, TFT_BLACK);
        return;
    }
    d.fillEllipse(cx, cy, rx, ry, TFT_WHITE);
    d.fillCircle(cx + pupil_x, cy + pupil_y, pupil_r, TFT_BLACK);
    d.fillCircle(cx + pupil_x - pupil_r / 3, cy + pupil_y - pupil_r / 3, pupil_r / 4, TFT_WHITE);
}

void faceBegin() {
    M5.Display.setBrightness(config::DISPLAY_BRIGHTNESS);
    g_face = Face::Sleep;
    g_dirty = true;
    g_blink_at = millis() + 2400;
}

void faceSet(Face f) {
    if (g_face == f) return;
    g_face = f;
    g_blinking = false;
    g_dirty = true;
}

Face faceCurrent() { return g_face; }

void faceSetCaption(const char* text) {
    if (!text) g_caption[0] = '\0';
    else {
        strncpy(g_caption, text, sizeof(g_caption) - 1);
        g_caption[sizeof(g_caption) - 1] = '\0';
    }
    g_dirty = true;
}

void faceTick() {
    if (g_dirty) {
        auto& d = M5.Display;
        d.startWrite();
        d.fillScreen(bgFor(g_face));
        const bool closed = (g_face == Face::Sleep) || g_blinking;
        int lx = 100, rxeye = 220, cy = 110;
        if (g_face == Face::Fist) {
            d.fillRoundRect(40, 70, 100, 90, 14, d.color565(240, 190, 140));
            d.fillRoundRect(180, 70, 100, 90, 14, d.color565(240, 190, 140));
            d.fillCircle(160, 110, 10, d.color565(255, 200, 80));
            d.setFont(&fonts::Font2);
            d.setTextDatum(middle_center);
            d.setTextColor(d.color565(255, 180, 80));
            d.drawString("BUMP", 160, 185);
        } else if (g_face == Face::Sleep) {
            drawEye(lx, cy, 48, 20, 0, 0, 18, true);
            drawEye(rxeye, cy, 48, 20, 0, 0, 18, true);
        } else if (g_face == Face::Amaze) {
            drawEye(lx, cy, 56, 64, 0, 0, 24, closed);
            drawEye(rxeye, cy, 56, 64, 0, 0, 24, closed);
        } else {
            drawEye(lx, cy, 48, 56, 0, 6, 18, closed);
            drawEye(rxeye, cy, 48, 56, 0, 6, 18, closed);
        }
        if (g_caption[0]) {
            d.setFont(&fonts::Font4);
            d.setTextDatum(middle_center);
            d.setTextColor(TFT_WHITE, TFT_BLACK);
            d.fillRoundRect(8, 198, 304, 32, 8, TFT_BLACK);
            d.drawString(g_caption, 160, 214);
        }
        d.endWrite();
        g_dirty = false;
    }
    uint32_t now = millis();
    if (g_face != Face::Sleep && g_face != Face::Fist) {
        if (!g_blinking && now >= g_blink_at) {
            g_blinking = true;
            g_blink_until = now + 120;
            g_dirty = true;
        } else if (g_blinking && now >= g_blink_until) {
            g_blinking = false;
            g_blink_at = now + 1800 + (now % 1400);
            g_dirty = true;
        }
    }
}
