#include "offline_demo.h"
#include "face.h"
#include "sound.h"
#include "rgb.h"
#include "motion.h"

enum class Phase {
    Idle,
    Sleep,
    Person,
    Snap,
    Chord,
    Friend,
    Fist,
    Done,
};

static Phase g_phase = Phase::Idle;
static uint32_t g_phase_at = 0;
static bool g_running = false;

void demoBegin() { g_phase = Phase::Idle; }

void demoStart() {
    g_running = true;
    g_phase = Phase::Sleep;
    g_phase_at = millis();
    faceSet(Face::Sleep);
    faceSetCaption("");
    rgbSetEmotion("sleep");
    motionHome();
    Serial.println("[demo] 12s offline sequence start");
}

void demoCancel() {
    g_running = false;
    g_phase = Phase::Idle;
}

bool demoRunning() { return g_running; }

void demoTick() {
    if (!g_running) return;
    uint32_t t = millis() - g_phase_at;

    switch (g_phase) {
        case Phase::Sleep:
            if (t >= 2000) {
                g_phase = Phase::Person;
                g_phase_at = millis();
                faceSet(Face::Think);
                faceSetCaption("...person?");
                rgbSetEmotion("blue");
                soundPlay(SoundFx::Ping);
            }
            break;
        case Phase::Person:
            if (t >= 1200) {
                g_phase = Phase::Snap;
                g_phase_at = millis();
                motionSnapTowardPerson();
                faceSet(Face::Amaze);
                faceSetCaption("Amaze!");
            }
            break;
        case Phase::Snap:
            if (t >= 1200) {
                g_phase = Phase::Chord;
                g_phase_at = millis();
                soundPlay(SoundFx::ChordHappy);
                rgbSetEmotion("orange");
            }
            break;
        case Phase::Chord:
            if (t >= 1200) {
                g_phase = Phase::Friend;
                g_phase_at = millis();
                faceSet(Face::Happy);
                faceSetCaption("Friend! Friend! Friend!");
            }
            break;
        case Phase::Friend:
            if (t >= 3200) {
                g_phase = Phase::Fist;
                g_phase_at = millis();
                faceSet(Face::Fist);
                faceSetCaption("Fist my bump!");
            }
            break;
        case Phase::Fist:
            if (t >= 3200) {
                g_phase = Phase::Done;
                g_phase_at = millis();
                Serial.println("[demo] 12s sequence done — looping sleep");
                faceSet(Face::Sleep);
                faceSetCaption("");
                rgbSetEmotion("sleep");
                motionHome();
            }
            break;
        case Phase::Done:
            if (t >= 2500) demoStart();
            break;
        default:
            break;
    }
}
