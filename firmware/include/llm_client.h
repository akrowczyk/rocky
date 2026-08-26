#pragma once

#include <Arduino.h>
#include "wifi_config.h"

// On-device OpenAI-compatible chat/completions client.
// Default host is xAI: https://api.x.ai/v1  model grok-4.6
// Weekend prototype: TLS uses WiFiClientSecure::setInsecure() because the
// host can still vary (xAI, Groq, LAN reverse-proxies, etc.). See README.

void llmBegin(const WifiConfig& cfg);
bool llmConfigured();  // base_url and api_key both non-empty

// POST {base}/chat/completions. Shows think face while in flight.
// Parses choices[0].message.content as a JSON array of body commands and
// applies them. Returns false on HTTP/parse failure so the caller can run
// a local canned greeting — no laptop required.
// range_mm is only meaningful for event "person" (LTR-553 heuristic).
bool llmAsk(const char* event, int range_mm = 0);
