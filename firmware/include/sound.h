#pragma once

#include <Arduino.h>

enum class SoundFx {
    None,
    Ping,
    ChordHappy,
    WhistleThoughtful,
};

SoundFx soundFromName(const char* name);
void soundBegin();
void soundPlay(SoundFx fx);
void soundStop();  // cancel sequencer; Voice client calls this before I2S swap
void soundTick();  // non-blocking sequencer
bool soundBusy();
