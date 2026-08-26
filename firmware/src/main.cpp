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
#include "llm_client.h"
#include "offline_demo.h"

enum class RunMode : uint8_t { Offline, Llm, Brain };

static WifiConfig g_wifi;
static RunMode g_mode = RunMode::Offline;
static uint32_t g_last_person_ms = 0;
static bool g_person_latched = false;

static void playCannedGreeting() {
    faceSet(Face::Happy);
    faceSetCaption("Friend! Friend! Friend!");
    rgbSetEmotion("orange");
    soundPlay(SoundFx::ChordHappy);
    motionLook(0.0f, 20.0f, 250);
    faceTick();
    uint32_t until = millis() + config::LLM_CMD_GAP_MS;
    while (millis() < until) {
        soundTick();
        faceTick();
        delay(15);
    }
    faceSet(Face::Fist);
    faceSetCaption("Fist my bump!");
    faceTick();
}

void setup() {
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    M5.begin(cfg);
    Serial.begin(115200);
    delay(200);
    Serial.println("\n=== Desk Rocky  (StackChan CoreS3 / K151-R) ===");
    Serial.println("Owner: Andrew Krowczyk  |  On-device LLM  |  laptop brain optional");

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
        llmBegin(g_wifi);
        if (llmConfigured()) {
            g_mode = RunMode::Llm;
            Serial.println("[main] LLM mode — no laptop required");
            if (!llmAsk("boot")) {
                Serial.println("[main] llm boot failed — local wait");
                faceSet(Face::Think);
                faceSetCaption("Me Rocky. Me wait.");
                rgbSetEmotion("blue");
                soundPlay(SoundFx::Ping);
            }
        } else if (!g_wifi.brain_host.isEmpty()) {
            brainBegin(g_wifi);
            faceSetCaption("brain...");
            uint32_t start = millis();
            while (!brainConnected() && millis() - start < config::BRAIN_TIMEOUT_MS) {
                brainTick();
                soundTick();
                faceTick();
                delay(20);
            }
            if (brainConnected()) {
                g_mode = RunMode::Brain;
                faceSet(Face::Think);
                faceSetCaption("Me Rocky. Me wait.");
                rgbSetEmotion("blue");
                soundPlay(SoundFx::Ping);
            }
        }
    }

    if (g_mode == RunMode::Offline) {
        Serial.println("[main] offline 12s demo");
        demoStart();
    }
}

void loop() {
    M5.update();
    brainTick();
    soundTick();
    faceTick();
    demoTick();

    if (g_mode == RunMode::Brain && !brainConnected()) {
        if (!demoRunning()) {
            Serial.println("[main] brain lost — offline demo");
            demoStart();
        }
        g_mode = RunMode::Offline;
    }
    if (g_mode == RunMode::Offline && brainConnected()) {
        demoCancel();
        g_mode = RunMode::Brain;
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
            if (g_mode == RunMode::Llm) {
                demoCancel();
                if (!llmAsk("person", mm)) {
                    Serial.println("[main] llm person failed — canned greeting");
                    playCannedGreeting();
                }
            } else if (g_mode == RunMode::Brain) {
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
