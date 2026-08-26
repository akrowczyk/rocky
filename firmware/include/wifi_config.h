#pragma once

#include <Arduino.h>

struct WifiConfig {
    String ssid;
    String password;
    String llm_base_url;
    String llm_api_key;
    String llm_model = "grok-4.6";
    bool voice = true;                              // Grok Voice Agent (realtime WSS)
    String llm_voice_model = "grok-voice-latest";   // query param on /v1/realtime
    String llm_voice = "eve";                       // built-in voice id (docs default)
    String brain_host;
    uint16_t brain_port = 8080;
    String brain_path = "/ws";
    bool loaded = false;
};

bool wifiConfigLoad(WifiConfig& out);  // LittleFS /wifi.json
bool wifiConnect(const WifiConfig& cfg, uint32_t timeout_ms);
