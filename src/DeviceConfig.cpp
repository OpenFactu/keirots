#include "DeviceConfig.h"
#include <Preferences.h>
#include <mbedtls/x509_crt.h>

const char* jsonText(const cJSON* json, const char* key) {
  const cJSON* value = cJSON_GetObjectItemCaseSensitive(json, key);
  return cJSON_IsString(value) && value->valuestring ? value->valuestring : "";
}
void wipeString(String& text) {
  for (size_t i = 0; i < text.length(); ++i) text.setCharAt(i, '\0');
  text = "";
}
static bool headerSafe(const String& text, size_t max) {
  if (text.isEmpty() || text.length() > max) return false;
  for (char c : text) if (c <= 32 || c > 126) return false;
  return true;
}
static bool originSafe(String& url) {
  if (!url.startsWith("https://") || url.length() > 256) return false;
  if (url.endsWith("/")) url.remove(url.length() - 1);
  String host = url.substring(8);
  if (host.isEmpty()) return false;
  int colon = host.indexOf(':');
  if (colon >= 0) {
    String port = host.substring(colon + 1);
    if (port.isEmpty() || port.length() > 5) return false;
    for (char c : port) if (c < '0' || c > '9') return false;
    if (port.toInt() < 1 || port.toInt() > 65535) return false;
    host = host.substring(0, colon);
  }
  for (char c : host) if (!isalnum(static_cast<unsigned char>(c)) && c != '.' && c != '-') return false;
  host.toLowerCase();
  if (host.isEmpty() || host == "localhost" || host == "localhost." || host.endsWith(".localhost") ||
      host.startsWith("127.") || host == "0.0.0.0" || host.indexOf('.') < 0) return false;
  return true;
}
bool parseConfig(const cJSON* json, DeviceConfig& config) {
  if (!cJSON_IsObject(json)) return false;
  DeviceConfig parsed;
  parsed.serverUrl = jsonText(json, "serverUrl");
  parsed.tenantId = jsonText(json, "tenantId");
  parsed.deviceToken = jsonText(json, "deviceToken");
  parsed.ssid = jsonText(json, "ssid");
  parsed.password = jsonText(json, "password");
  parsed.caCert = jsonText(json, "caCert");
  if (!originSafe(parsed.serverUrl) || !headerSafe(parsed.tenantId, 100) ||
      !headerSafe(parsed.deviceToken, 200) || parsed.ssid.isEmpty() || parsed.ssid.length() > 32 ||
      (!parsed.password.isEmpty() && (parsed.password.length() < 8 || parsed.password.length() > 63)) ||
      parsed.caCert.length() > 4096) return false;
  if (!parsed.caCert.isEmpty()) {
    mbedtls_x509_crt cert;
    mbedtls_x509_crt_init(&cert);
    int result = mbedtls_x509_crt_parse(&cert,
      reinterpret_cast<const unsigned char*>(parsed.caCert.c_str()), parsed.caCert.length() + 1);
    mbedtls_x509_crt_free(&cert);
    if (result != 0) return false;
  }
  config = parsed;
  return true;
}
bool loadConfig(DeviceConfig& config) {
  Preferences store;
  if (!store.begin("keirost", true)) return false;
  String serialized = store.getString("config", "");
  store.end();
  cJSON* json = cJSON_Parse(serialized.c_str());
  bool valid = parseConfig(json, config);
  cJSON_Delete(json);
  wipeString(serialized);
  return valid;
}
bool saveConfig(const DeviceConfig& config) {
  cJSON* json = cJSON_CreateObject();
  if (!json) return false;
  cJSON_AddStringToObject(json, "serverUrl", config.serverUrl.c_str());
  cJSON_AddStringToObject(json, "tenantId", config.tenantId.c_str());
  cJSON_AddStringToObject(json, "deviceToken", config.deviceToken.c_str());
  cJSON_AddStringToObject(json, "ssid", config.ssid.c_str());
  cJSON_AddStringToObject(json, "password", config.password.c_str());
  cJSON_AddStringToObject(json, "caCert", config.caCert.c_str());
  char* serialized = cJSON_PrintUnformatted(json);
  cJSON_Delete(json);
  if (!serialized) return false;
  Preferences store;
  bool saved = false;
  if (store.begin("keirost", false)) {
    saved = store.putString("config", serialized) == strlen(serialized);
    store.end();
  }
  memset(serialized, 0, strlen(serialized));
  cJSON_free(serialized);
  return saved;
}
