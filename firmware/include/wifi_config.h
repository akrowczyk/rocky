#pragma once

#include <Arduino.h>

struct WifiConfig {
    String ssid;
    String password;
    String llm_base_url;
    String llm_api_key;
    String llm_model = "grok-4.6";
    String brain_host;
    uint16_t brain_port = 8080;
    String brain_path = "/ws";
    bool loaded = false;
};

bool wifiConfigLoad(WifiConfig& out);  // LittleFS /wifi.json
bool wifiConnect(const WifiConfig& cfg, uint32_t timeout_ms);
