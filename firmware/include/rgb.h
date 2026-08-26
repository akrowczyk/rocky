#pragma once

#include <Arduino.h>

void rgbBegin();
void rgbSetServoPower(bool on);
void rgbSetAll(uint8_t r, uint8_t g, uint8_t b);
void rgbSetEmotion(const char* name);
void rgbOff();
bool rgbAvailable();
