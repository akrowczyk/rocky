#pragma once

#include <Arduino.h>

struct WifiConfig {
    String ssid;
    String password;
    String brain_host;
    uint16_t brain_port = 8080;
    String brain_path = "/ws";
    bool loaded = false;
};

bool wifiConfigLoad(WifiConfig& out);
bool wifiConnect(const WifiConfig& cfg, uint32_t timeout_ms);
