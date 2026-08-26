#pragma once

#include <Arduino.h>
#include "wifi_config.h"

void brainBegin(const WifiConfig& cfg);
void brainTick();
bool brainConnected();
void brainSendJson(const char* json);
void brainSendBoot();
void brainSendPerson(int range_mm);
