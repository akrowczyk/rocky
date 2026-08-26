#pragma once

#include <cstddef>
#include <cstdint>

// CoreS3 ES7210 mic + AW88298 speaker share I2S_NUM_1 and cannot run together
// (M5Unified CoreS3 Mic sample). Half-duplex: listen XOR talk.
// Internal to the voice client. Pitch clamp lives in motionLook(), not here.

static constexpr uint32_t VOICE_I2S_RATE = 16000;
static constexpr int VOICE_MIC_SAMPLES = 1600;    // 100 ms
static constexpr int VOICE_PLAY_SAMPLES = 2048;   // ~128 ms
static constexpr uint32_t VOICE_I2S_SETTLE_MS = 80;

bool voiceI2sAlloc();
void voiceI2sRequestSpeak();
void voiceI2sClearWantSpeak();
bool voiceI2sWantSpeak();
void voiceI2sSetAudioDone(bool done);
bool voiceI2sAudioDone();
void voiceI2sGoSpeak();
void voiceI2sGoMic();
void voiceI2sTick();       // settle WaitSpeak/WaitMic; Mic->Speak if requested
void voiceI2sPumpPlay();
bool voiceI2sEnqueuePcm(const uint8_t* bytes, size_t nbytes);
void voiceI2sEnqueueB64(const char* b64);
bool voiceI2sPlaying();
bool voiceI2sIsSpeak();
bool voiceI2sIsMic();
void voiceI2sForceSpeakerNow();  // TTS fallback: drop mic, bring speaker up

// If a 100 ms PCM16 frame is ready, returns true and starts the next record.
bool voiceI2sTakeMicFrame(const uint8_t** data, size_t* nbytes);
