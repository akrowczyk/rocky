#pragma once

#include <Arduino.h>
#include "wifi_config.h"

void llmBegin(const WifiConfig& cfg);
bool llmConfigured();
bool llmAsk(const char* event, int range_mm = 0);
