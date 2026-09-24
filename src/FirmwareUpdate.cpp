#include "FirmwareUpdate.h"
#include "FirmwareVersion.h"
#include <NetworkClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <esp_ota_ops.h>

bool installFirmware(const DeviceConfig& config, const cJSON* update) {
  const char* version = jsonText(update, "version");
  const char* digest = jsonText(update, "sha256");
  const cJSON* size = cJSON_GetObjectItemCaseSensitive(update, "size");
  if (strcmp(jsonText(update, "board"), KEIROST_BOARD) || !*version ||
      !strcmp(version, KEIROST_FIRMWARE_VERSION) || strlen(version) > 24 ||
      strlen(digest) != 64 || !cJSON_IsNumber(size) || size->valuedouble < 1024 ||
      size->valuedouble > 0x140000 || size->valuedouble != size->valueint) return false;
  for (const char* c = version; *c; ++c) if (!isdigit(*c) && *c != '.') return false;
  for (const char* c = digest; *c; ++c) if (!isxdigit(*c)) return false;
  const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
  if (!next || next->size < static_cast<size_t>(size->valueint)) return false;
  NetworkClientSecure tls;
  if (config.caCert.isEmpty()) tls.useBuiltinCACertBundle();
  else tls.setCACert(config.caCert.c_str());
  tls.setHandshakeTimeout(10);
  HTTPClient http;
  const String url = config.serverUrl + "/api/hr/timeclock/terminal/v1/device/firmware/" + version;
  if (!http.begin(tls, url)) return false;
  http.setConnectTimeout(10000);
  http.setTimeout(15000);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  http.addHeader("x-tenant-id", config.tenantId);
  http.addHeader("x-device-token", config.deviceToken);
  bool success = false;
  if (http.GET() == 200 && http.getSize() == size->valueint && Update.begin(size->valueint)) {
    if (Update.setSHA256(digest)) {
      // Update uses the inactive application slot. end() activates it only after
      // validating the full image and SHA-256. NVS and partition table are untouched.
      size_t written = Update.writeStream(*http.getStreamPtr());
      success = written == static_cast<size_t>(size->valueint) && Update.end();
    }
    if (!success) Update.abort();
  }
  http.end();
  return success;
}
