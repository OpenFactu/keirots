// Contract checks evaluated by the actual ESP32 compiler; no runtime overhead.
#include "RetryPolicy.h"
#include "CardCapture.h"

constexpr bool captureChecks() {
  CardCapture c;
  const uint8_t first[10] = {0x04, 0xA1, 0xB2, 0xC3, 1, 2, 3, 4, 5, 6};
  const uint8_t second[4] = {0xDE, 0xAD, 0xBE, 0xEF};
  if (c.consume(first, 4, 0)) return false;
  c.start(100);
  if (!c.consume(first, 4, 101) || c.size() != 4) return false;
  if (!c.consume(second, 4, 102) || c.at(0) != 0x04) return false;
  if (!c.active(30099) || c.active(30100)) return false;
  if (c.consume(first, 4, 30100) || c.size() || c.at(0)) return false;
  c.start(UINT32_MAX - 1000);
  if (!c.active(1000) || c.active(28999)) return false;
  c.start(0);
  if (!c.consume(first, 3, 0) || c.size()) return false;
  if (!c.consume(first, 7, 0) || c.size() != 7) return false;
  c.cancel();
  if (c.active(0) || c.size() || c.at(6)) return false;
  c.start(0);
  return c.consume(first, 10, 1) && c.size() == 10 && c.at(9) == 6;
}
static_assert(captureChecks(), "Enrollment consumes only one UID, expires, wipes and suppresses kiosk readings");
static_assert(retryableStatus(-1) && retryableStatus(503) && retryableStatus(429), "Temporary errors retry");
static_assert(!retryableStatus(401) && !retryableStatus(409) && !retryableStatus(202), "Permanent responses stop retrying");
static_assert(retryDelayMs(0) == 1000 && retryDelayMs(10) == 8000, "Backoff is bounded");
static_assert(!pendingExpired(1000, 1000) && pendingExpired(61000, 1000), "Intention expires after 60 seconds");
static_assert(pendingExpired(59000, UINT32_MAX - 1000), "Expiry survives millis wraparound");
static_assert(!due(100, 200) && due(10, UINT32_MAX - 5), "Deadline survives millis wraparound");
