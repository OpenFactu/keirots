#include "PlatformClient.h"
#include "RetryPolicy.h"
#include "FirmwareVersion.h"
#include "FirmwareUpdate.h"
#include <WiFi.h>
#include <NetworkClientSecure.h>
#include <HTTPClient.h>
#include <esp_random.h>
#include <time.h>
#include <atomic>

namespace {
DeviceConfig config;
struct Card { char uid[21]; uint32_t createdAt; };
QueueHandle_t cards = nullptr;
std::atomic<bool> accepting{false};
std::atomic<bool> slotBusy{false};
std::atomic<const char*> state{"unconfigured"};
constexpr char PREFIX[] = "/api/hr/timeclock/terminal/v1";
String failedUpdate;

// Limit response memory even for chunked or unexpectedly large server replies.
class ResponseBuffer : public Stream {
 public:
  String body;
  size_t write(uint8_t byte) override { return write(&byte, 1); }
  size_t write(const uint8_t* data, size_t size) override {
    if (body.length() + size > 4096) return 0;
    return body.concat(reinterpret_cast<const char*>(data), size) ? size : 0;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
};
struct Response { int status; String body; uint32_t retryAfter; };
uint32_t retryAfterMs(const String& value) {
  bool numeric = !value.isEmpty();
  uint64_t seconds = 0;
  for (char c : value) {
    if (c < '0' || c > '9') { numeric = false; break; }
    seconds = seconds * 10 + (c - '0');
    if (seconds > 86400) return 86400000;
  }
  if (!numeric) {
    struct tm parsed = {};
    if (!strptime(value.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &parsed)) return 60000;
    double difference = difftime(mktime(&parsed), time(nullptr));
    seconds = difference > 0 ? static_cast<uint64_t>(difference) : 1;
  }
  return static_cast<uint32_t>(seconds > 86400 ? 86400000 : seconds * 1000);
}
Response request(const char* route, const char* body = nullptr) {
  NetworkClientSecure tls;
  if (config.caCert.isEmpty()) tls.useBuiltinCACertBundle();
  else tls.setCACert(config.caCert.c_str());
  tls.setHandshakeTimeout(6);
  HTTPClient http;
  Response response{-1, "", 0};
  if (!http.begin(tls, config.serverUrl + PREFIX + route)) return response;
  http.setConnectTimeout(5000);
  http.setTimeout(5000);
  http.setReuse(false);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  const char* headers[] = {"Retry-After"};
  http.collectHeaders(headers, 1);
  http.addHeader("x-tenant-id", config.tenantId);
  http.addHeader("x-device-token", config.deviceToken);
  http.addHeader("x-device-firmware", KEIROST_FIRMWARE_VERSION);
  http.addHeader("x-device-board", KEIROST_BOARD);
  if (!failedUpdate.isEmpty()) http.addHeader("x-device-update-failed", failedUpdate);
  if (body) http.addHeader("Content-Type", "application/json");
  response.status = body ? http.POST(reinterpret_cast<uint8_t*>(const_cast<char*>(body)), strlen(body)) : http.GET();
  if (response.status == 429 || http.hasHeader("Retry-After")) response.retryAfter = retryAfterMs(http.header("Retry-After"));
  if (response.status > 0) {
    ResponseBuffer buffer;
    if (http.getSize() <= 4096 && http.writeToStream(&buffer) >= 0) response.body = buffer.body;
  }
  http.end();
  return response;
}
void uuid(char* output) {
  uint8_t bytes[16];
  esp_fill_random(bytes, sizeof(bytes));
  bytes[6] = (bytes[6] & 0x0f) | 0x40;
  bytes[8] = (bytes[8] & 0x3f) | 0x80;
  snprintf(output, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
    bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5],bytes[6],bytes[7],bytes[8],bytes[9],bytes[10],bytes[11],bytes[12],bytes[13],bytes[14],bytes[15]);
}
void worker(void*) {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(config.ssid.c_str(), config.password.c_str());
  configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
  uint32_t nextConfig = millis(), nextAttempt = 0, cooldown = 0;
  bool cooling = false, valid = false, unauthorized = false, pending = false;
  unsigned attempts = 0;
  uint32_t createdAt = 0;
  char body[160] = {};
  String deviceId, kioskId;
  for (;;) {
    uint32_t now = millis();
    if (pending && pendingExpired(now, createdAt)) {
      memset(body, 0, sizeof(body)); pending = false;
      slotBusy = false;
      state = "reading_expired";
    }
    if (unauthorized) { accepting = false; state = "unauthorized"; }
    else if (WiFi.status() != WL_CONNECTED) { accepting = false; state = "wifi_connecting"; valid = false; }
    else if (time(nullptr) < 1700000000) { accepting = false; state = "syncing_clock"; }
    else if (cooling && !due(now, cooldown)) { accepting = false; }
    else {
      cooling = false;
      if (!valid || due(now, nextConfig)) {
        accepting = false;
        Response response = request("/device/config");
        nextConfig = millis() + 60000;
        if (response.status == 401) { unauthorized = true; pending = false; memset(body, 0, sizeof(body)); }
        else if (response.status == 200) {
          cJSON* json = cJSON_Parse(response.body.c_str());
          const cJSON* version = cJSON_GetObjectItemCaseSensitive(json, "protocolVersion");
          String newDevice = jsonText(json, "deviceId"), newKiosk = jsonText(json, "kioskId");
          bool paired = strcmp(jsonText(json, "mode"), "paired") == 0;
          valid = cJSON_IsNumber(version) && version->valuedouble == 1 && paired && !newDevice.isEmpty() && !newKiosk.isEmpty();
          if (!valid || (!deviceId.isEmpty() && (deviceId != newDevice || kioskId != newKiosk))) {
            pending = false; memset(body, 0, sizeof(body));
            Card discarded = {};
            while (xQueueReceive(cards, &discarded, 0) == pdTRUE) memset(&discarded, 0, sizeof(discarded));
            slotBusy = false;
          }
          deviceId = newDevice; kioskId = newKiosk;
          state = valid ? "ready" : (paired ? "invalid_response" : "wrong_mode");
          const cJSON* update = cJSON_GetObjectItemCaseSensitive(json, "firmwareUpdate");
          String updateId = jsonText(update, "id");
          if (valid && !pending && !slotBusy && updateId.length() == 36 && updateId != failedUpdate) {
            state = "updating_firmware";
            if (installFirmware(config, update)) {
              cJSON_Delete(json);
              Serial.println("Actualizacion verificada. Reiniciando lector.");
              delay(300);
              ESP.restart();
            }
            failedUpdate = updateId;
            state = "firmware_update_failed";
            nextConfig = millis() + 2000;
          }
          cJSON_Delete(json);
          if (!valid) { cooling = true; cooldown = nextConfig; }
        } else {
          valid = false; state = response.status == 429 ? "rate_limited" : "server_unavailable";
          cooling = true; cooldown = millis() + (response.retryAfter ? response.retryAfter : 8000);
        }
      }
      if (valid && !unauthorized && !cooling) {
        if (!pending) {
          Card card = {};
          if (xQueueReceive(cards, &card, 0) == pdTRUE) {
            if (!pendingExpired(millis(), card.createdAt)) {
              char requestId[37]; uuid(requestId);
              snprintf(body, sizeof(body), "{\"requestId\":\"%s\",\"uid\":\"%s\"}", requestId, card.uid);
              createdAt = card.createdAt; pending = true; attempts = 0; nextAttempt = millis();
            } else slotBusy = false;
          }
          memset(&card, 0, sizeof(card));
        }
        if (pending && due(millis(), nextAttempt)) {
          state = "sending";
          Response response = request("/device/readings", body);
          if (response.status == 202) {
            cJSON* json = cJSON_Parse(response.body.c_str());
            bool accepted = strcmp(jsonText(json, "status"), "awaiting_confirmation") == 0 &&
              *jsonText(json, "readingId") && *jsonText(json, "expiresAt");
            cJSON_Delete(json);
            if (accepted) {
              pending = false; state = "awaiting_confirmation";
            } else { nextAttempt = millis() + retryDelayMs(attempts++); state = "invalid_response"; }
          } else if (retryableStatus(response.status)) {
            uint32_t wait = response.retryAfter ? response.retryAfter : retryDelayMs(attempts++);
            nextAttempt = millis() + wait;
            state = response.status == 429 ? "rate_limited" : "retrying";
            if (response.retryAfter) { cooling = true; cooldown = nextAttempt; }
          } else {
            pending = false;
            if (response.status == 401) { unauthorized = true; state = "unauthorized"; }
            else if (response.status == 403) { valid = false; nextConfig = millis(); state = "wrong_mode"; }
            else if (response.status == 404) state = "unknown_card";
            else if (response.status == 409) state = "kiosk_busy_or_conflict";
            else state = "reading_rejected";
          }
          if (!pending) { memset(body, 0, sizeof(body)); slotBusy = false; }
        }
        accepting = valid && !unauthorized && !cooling;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
} // namespace
bool startPlatform(const DeviceConfig& saved) {
  config = saved;
  cards = xQueueCreate(1, sizeof(Card));
  if (!cards || xTaskCreate(worker, "keirost-network", 12288, nullptr, 1, nullptr) != pdPASS) {
    state = "startup_error"; return false;
  }
  state = "wifi_connecting";
  return true;
}
bool submitCard(const uint8_t* uid, uint8_t size) {
  if (size != 4 && size != 7 && size != 10) return false;
  if (!accepting || WiFi.status() != WL_CONNECTED) return false;
  bool expected = false;
  if (!slotBusy.compare_exchange_strong(expected, true)) return false;
  Card card = {};
  card.createdAt = millis();
  for (uint8_t i = 0; i < size; ++i) snprintf(card.uid + 2 * i, 3, "%02X", uid[i]);
  bool sent = xQueueSend(cards, &card, 0) == pdTRUE;
  if (!sent) slotBusy = false;
  memset(&card, 0, sizeof(card));
  return sent;
}
const char* platformState() { return state.load(); }
