#pragma once

#include <Arduino.h>

void proximityBegin();
bool proximityAvailable();
uint16_t proximityRaw();
int proximityRangeMm();
bool proximityPersonPresent();
