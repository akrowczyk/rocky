#pragma once

#include <Arduino.h>

struct Look {
    float yaw_deg   = 0.0f;
    float pitch_deg = 45.0f;
};

void motionBegin();
void motionLook(float yaw_deg, float pitch_deg, uint16_t time_ms = 300);
void motionLook(const Look& look, uint16_t time_ms = 300);
void motionHome();
void motionSnapTowardPerson();
Look motionCurrent();
float clampPitch(float pitch_deg);
