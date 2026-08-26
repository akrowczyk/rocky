#include "wifi_config.h"
#include "config.h"
#include <WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

bool wifiConfigLoad(WifiConfig& out) {
    out.loaded = false;
    if (!LittleFS.begin(true)) {
        Serial.println("[wifi] LittleFS mount failed");
        return false;
    }
    File f = LittleFS.open("/wifi.json", "r");
    if (!f) {
        Serial.println("[wifi] /wifi.json missing — copy data/wifi.json.example to data/wifi.json and uploadfs");
        return false;
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        Serial.printf("[wifi] json parse error: %s\n", err.c_str());
        return false;
    }
    out.ssid = doc["ssid"] | "";
    out.password = doc["password"] | "";
    out.llm_base_url = doc["llm_base_url"] | "";
    out.llm_api_key = doc["llm_api_key"] | "";
    out.llm_model = doc["llm_model"] | "gpt-4o-mini";
    out.brain_host = doc["brain_host"] | "";
    out.brain_port = doc["brain_port"] | 8080;
    out.brain_path = doc["brain_path"] | "/ws";
    if (out.ssid.isEmpty()) {
        Serial.println("[wifi] ssid empty");
        return false;
    }
    out.loaded = true;
    Serial.printf("[wifi] ssid=%s llm=%s model=%s key=%s brain=%s:%u%s\n",
                  out.ssid.c_str(),
                  out.llm_base_url.isEmpty() ? "-" : out.llm_base_url.c_str(),
                  out.llm_model.c_str(),
                  out.llm_api_key.isEmpty() ? "no" : "yes",
                  out.brain_host.isEmpty() ? "-" : out.brain_host.c_str(),
                  out.brain_port,
                  out.brain_path.c_str());
    return true;
}

bool wifiConnect(const WifiConfig& cfg, uint32_t timeout_ms) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.ssid.c_str(), cfg.password.c_str());
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > timeout_ms) {
            Serial.println("[wifi] connect timeout");
            return false;
        }
        delay(200);
        Serial.print(".");
    }
    Serial.printf("\n[wifi] ip=%s rssi=%d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    return true;
}
