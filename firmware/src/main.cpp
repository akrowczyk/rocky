#include <M5Unified.h>
#include <M5GFX.h>

#include "config.h"
#include "pins.h"
#include "face.h"
#include "sound.h"
#include "rgb.h"
#include "motion.h"
#include "proximity.h"
#include "wifi_config.h"
#include "brain_client.h"
#include "offline_demo.h"

static WifiConfig g_wifi;
static bool g_online = false;
static uint32_t g_last_person_ms = 0;
static bool g_person_latched = false;

void setup() {
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    M5.begin(cfg);
    Serial.begin(115200);
    delay(200);
    Serial.println("\n=== Desk Rocky  (StackChan CoreS3 / K151-R) ===");
    Serial.println("Owner: Andrew Krowczyk  |  Brain is a laptop, not Module LLM");

    faceBegin();
    soundBegin();
    rgbBegin();
    motionBegin();
    proximityBegin();
    demoBegin();

    faceSet(Face::Sleep);
    rgbSetEmotion("sleep");

    bool have_cfg = wifiConfigLoad(g_wifi);
    bool wifi_ok = false;
    if (have_cfg) {
        faceSetCaption("wifi...");
        wifi_ok = wifiConnect(g_wifi, config::WIFI_TIMEOUT_MS);
    }

    if (wifi_ok) {
        brainBegin(g_wifi);
        faceSetCaption("brain...");
        uint32_t start = millis();
        while (!brainConnected() && millis() - start < config::BRAIN_TIMEOUT_MS) {
            brainTick();
            soundTick();
            faceTick();
            delay(20);
        }
        g_online = brainConnected();
    }

    if (!g_online) {
        Serial.println("[main] brain unreachable — offline 12s demo");
        demoStart();
    } else {
        faceSet(Face::Think);
        faceSetCaption("Me Rocky. Me wait.");
        rgbSetEmotion("blue");
        soundPlay(SoundFx::Ping);
    }
}

void loop() {
    M5.update();
    brainTick();
    soundTick();
    faceTick();
    demoTick();

    if (g_online && !brainConnected()) {
        if (!demoRunning()) {
            Serial.println("[main] brain lost — offline demo");
            demoStart();
        }
        g_online = false;
    }
    if (!g_online && brainConnected()) {
        demoCancel();
        g_online = true;
        faceSet(Face::Think);
        faceSetCaption("Brain back. Full good.");
    }

    bool present = proximityPersonPresent();
    uint32_t now = millis();
    if (present && !g_person_latched) {
        if (now - g_last_person_ms > config::PERSON_COOLDOWN_MS) {
            int mm = proximityRangeMm();
            Serial.printf("[main] person range_mm~%d (heuristic) raw=%u\n", mm, proximityRaw());
            motionSnapTowardPerson();
            if (g_online) {
                demoCancel();
                brainSendPerson(mm);
            }
            g_last_person_ms = now;
        }
        g_person_latched = true;
    } else if (!present) {
        g_person_latched = false;
    }

    delay(15);
}
