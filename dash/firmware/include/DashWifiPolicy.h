#pragma once

#include <stdint.h>

namespace DashWifiPolicy {

inline constexpr uint32_t kRetryIntervalMs = 30000;

inline bool shouldRetry(bool configured, bool connected, uint32_t nowMs,
                        uint32_t lastAttemptMs) {
  return configured && !connected &&
         static_cast<uint32_t>(nowMs - lastAttemptMs) >= kRetryIntervalMs;
}

}  // namespace DashWifiPolicy
