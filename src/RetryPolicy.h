#pragma once
#include <stdint.h>
constexpr uint32_t PENDING_LIFETIME_MS = 60000;
constexpr bool retryableStatus(int status) {
  return status < 0 || status == 408 || status == 429 || status >= 500;
}
constexpr uint32_t retryDelayMs(unsigned attempt) {
  return 1000U << (attempt > 3 ? 3 : attempt);
}
constexpr bool pendingExpired(uint32_t now, uint32_t createdAt) {
  return now - createdAt >= PENDING_LIFETIME_MS;
}
constexpr bool due(uint32_t now, uint32_t at) {
  return static_cast<int32_t>(now - at) >= 0;
}
