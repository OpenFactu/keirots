#pragma once
#include <Arduino.h>
#include <cJSON.h>
struct DeviceConfig {
  String serverUrl, tenantId, deviceToken, ssid, password, caCert;
};
bool parseConfig(const cJSON* json, DeviceConfig& config);
bool loadConfig(DeviceConfig& config);
bool saveConfig(const DeviceConfig& config);
const char* jsonText(const cJSON* json, const char* key);
void wipeString(String& text);
