#pragma once

#include <stdint.h>

namespace DashLinkProtocol {

inline constexpr char kServiceUuid[] =
    "8f771000-6d7a-4f48-9f8a-67a8c14b6c01";
inline constexpr char kHandshakeCharacteristicUuid[] =
    "8f771001-6d7a-4f48-9f8a-67a8c14b6c01";
inline constexpr char kDashDeviceName[] = "APEXI-DASH";
inline constexpr char kLoggerDeviceName[] = "APEXI-LOGGER";
inline constexpr char kHandshake[] = "mda-logger/1";

inline constexpr uint32_t kDefaultRetryIntervalMs = 5000;
inline constexpr uint32_t kDefaultScanDurationSeconds = 2;

inline bool retryDue(const uint32_t nowMs,
                     const uint32_t previousAttemptMs,
                     const uint32_t retryIntervalMs) {
  return static_cast<uint32_t>(nowMs - previousAttemptMs) >= retryIntervalMs;
}

}  // namespace DashLinkProtocol
