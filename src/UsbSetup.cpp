#include "UsbSetup.h"
#include "DeviceConfig.h"
#include "PlatformClient.h"
#include "CardCapture.h"
#include "FirmwareVersion.h"
#include <WiFi.h>

namespace {
String line;
String captureId;
CardCapture capture;
bool oversized = false, restartPending = false;
uint32_t releasedAt = 0, lastByteAt = 0;
void handleLine(bool configured) {
  cJSON* message = cJSON_Parse(line.c_str());
  if (!message || strcmp(jsonText(message, "protocol"), "keirost-usb-v1") != 0) { cJSON_Delete(message); return; }
  const char* id = jsonText(message, "id");
  if (strlen(id) != 36) { cJSON_Delete(message); return; }
  cJSON* reply = cJSON_CreateObject();
  cJSON_AddStringToObject(reply, "protocol", "keirost-usb-v1");
  cJSON_AddStringToObject(reply, "id", id);
  const char* command = jsonText(message, "command");
  const char* error = nullptr;
  if (!strcmp(command, "hello") || !strcmp(command, "status")) {
    cJSON_AddStringToObject(reply, "firmware", KEIROST_FIRMWARE_VERSION);
    cJSON_AddStringToObject(reply, "board", KEIROST_BOARD);
    cJSON_AddBoolToObject(reply, "otaUpdate", true);
    cJSON_AddBoolToObject(reply, "cardCapture", true);
    cJSON_AddBoolToObject(reply, "configured", configured);
    cJSON_AddBoolToObject(reply, "wifi", WiFi.status() == WL_CONNECTED);
    cJSON_AddBoolToObject(reply, "bootHeld", digitalRead(0) == LOW);
    cJSON_AddStringToObject(reply, "state", restartPending ? "release_boot" : platformState());
  } else if (!strcmp(command, "capture_start") || !strcmp(command, "capture_poll") || !strcmp(command, "capture_cancel")) {
    const char* requested = jsonText(message, "captureId");
    const bool active = capture.active(millis());
    if (strlen(requested) != 36) error = "CAPTURE_INVALID";
    else if (!strcmp(command, "capture_start")) {
      if (!strcmp(platformState(), "updating_firmware") || restartPending || (active && captureId != requested)) error = "CAPTURE_BUSY";
      else if (!active) { captureId = requested; capture.start(millis()); }
    } else if (active && captureId != requested) error = "CAPTURE_INVALID";
    else if (!strcmp(command, "capture_cancel")) {
      capture.cancel(); wipeString(captureId);
    } else if (!active) cJSON_AddStringToObject(reply, "captureState", "expired");
    else if (!capture.size()) cJSON_AddStringToObject(reply, "captureState", "waiting");
    else {
      char uid[21] = {};
      for (uint8_t i = 0; i < capture.size(); ++i) snprintf(uid + i * 2, 3, "%02X", capture.at(i));
      cJSON_AddStringToObject(reply, "captureState", "captured");
      cJSON_AddStringToObject(reply, "uid", uid);
      memset(uid, 0, sizeof(uid));
    }
  } else if (!strcmp(command, "configure")) {
    DeviceConfig config;
    if (!strcmp(platformState(), "updating_firmware") || capture.active(millis())) error = "CAPTURE_BUSY";
    else if (digitalRead(0) != LOW) error = "BOOT_REQUIRED";
    else if (!parseConfig(message, config)) error = "INVALID_CONFIG";
    else if (!saveConfig(config)) error = "STORAGE_ERROR";
    else { restartPending = true; releasedAt = 0; }
  } else error = "UNKNOWN_COMMAND";
  cJSON_AddBoolToObject(reply, "ok", !error);
  if (error) cJSON_AddStringToObject(reply, "code", error);
  char* output = cJSON_PrintUnformatted(reply);
  if (output) { Serial.println(output); cJSON_free(output); }
  cJSON_Delete(reply);
  cJSON_Delete(message);
}
}
void pollUsbSetup(bool configured) {
  if (!capture.active(millis())) { capture.cancel(); wipeString(captureId); }
  if (restartPending) {
    if (digitalRead(0) == LOW) releasedAt = 0;
    else if (!releasedAt) releasedAt = millis();
    else if (millis() - releasedAt > 600) ESP.restart();
  }
  // Process only a bounded amount per loop so USB traffic cannot starve RFID.
  for (unsigned n = 0; Serial.available() && n < 512; ++n) {
    char c = Serial.read(); lastByteAt = millis();
    if (c == '\n') {
      if (!oversized) handleLine(configured);
      wipeString(line); oversized = false;
    } else if (c != '\r' && !oversized) {
      line += c;
      if (line.length() > 8191) { wipeString(line); oversized = true; }
    }
  }
  if (millis() - lastByteAt > 10000) { wipeString(line); oversized = false; }
}

bool captureCardForUsb(const uint8_t* uid, uint8_t size) {
  return capture.consume(uid, size, millis());
}
