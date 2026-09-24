#pragma once
#include <stdint.h>

// RAM-only enrollment window. The caller suppresses normal kiosk submission
// whenever consume() is true, including additional cards within the window.
class CardCapture {
public:
  constexpr void cancel() {
    armed = false;
    length = 0;
    started = 0;
    for (auto& value : bytes) value = 0;
  }
  constexpr void start(uint32_t now) { cancel(); started = now; armed = true; }
  constexpr bool active(uint32_t now) const {
    return armed && static_cast<uint32_t>(now - started) < 30000;
  }
  constexpr bool consume(const uint8_t* uid, uint8_t size, uint32_t now) {
    if (!active(now)) { cancel(); return false; }
    if (!length && (size == 4 || size == 7 || size == 10)) {
      for (uint8_t i = 0; i < size; ++i) bytes[i] = uid[i];
      length = size;
    }
    return true;
  }
  constexpr uint8_t size() const { return length; }
  constexpr uint8_t at(uint8_t i) const { return i < 10 ? bytes[i] : 0; }
private:
  bool armed = false;
  uint32_t started = 0;
  uint8_t length = 0;
  uint8_t bytes[10] = {};
};
