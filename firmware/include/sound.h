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
void soundTick();
bool soundBusy();
