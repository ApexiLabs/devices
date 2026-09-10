#include "Logic.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace {

std::string trimWhitespace(std::string_view input) {
  size_t start = 0;
  while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start])) != 0) {
    ++start;
  }

  size_t end = input.size();
  while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1])) != 0) {
    --end;
  }

  return std::string(input.substr(start, end - start));
}

}  // namespace

namespace Logic {

float currentFromVoltage(const float voltage, const float shuntResistanceOhms) {
  if (shuntResistanceOhms <= 0.0f) {
    return 0.0f;
  }
  return (voltage / shuntResistanceOhms) * 1000.0f;
}

SensorFault determineSensorFault(const float voltage,
                                 const float currentmA,
                                 const bool adcAvailable) {
  if (!adcAvailable) {
    return SensorFault::AdcUnavailable;
  }
  if (voltage >= 3.28f) {
    return SensorFault::AdcSaturation;
  }
  if (currentmA < 3.6f) {
    return SensorFault::Underrange;
  }
  if (currentmA > 20.8f) {
    return SensorFault::Overrange;
  }
  return SensorFault::None;
}

float scaleEngineeringValue(const float currentmA,
                            const float currentMinmA,
                            const float currentMaxmA,
                            const float engMin,
                            const float engMax) {
  if (currentMaxmA <= currentMinmA) {
    return engMin;
  }

  const float normalized = (currentmA - currentMinmA) / (currentMaxmA - currentMinmA);
  const float clamped = std::clamp(normalized, 0.0f, 1.0f);
  return engMin + (clamped * (engMax - engMin));
}

float applyLowPassFilter(const float previousValue, const float currentValue, const float alpha) {
  const float clampedAlpha = std::clamp(alpha, 0.0f, 1.0f);
  return (clampedAlpha * currentValue) + ((1.0f - clampedAlpha) * previousValue);
}

bool intervalElapsed(const uint32_t nowMs,
                     const uint32_t previousMs,
                     const uint32_t intervalMs) {
  return static_cast<uint32_t>(nowMs - previousMs) >= intervalMs;
}

bool isRetryableHttpStatus(const int status) {
  return status < 0 || status == 408 || status == 425 || status == 429 || status >= 500;
}

bool shouldDiscardQueuedHttpStatus(const int status) {
  return status == 400 || status == 404 || status == 413 || status == 422;
}

bool productionSecurityAllowsNetwork(const bool required,
                                     const bool secureBootEnabled,
                                     const bool flashEncryptionReleaseMode) {
  return !required || (secureBootEnabled && flashEncryptionReleaseMode);
}

bool isValidRecorderAssignmentStatus(const std::string_view status) {
  return status == "unassigned" || status == "armed" || status == "claimed" ||
         status == "finished" || status == "revoked" || status == "expired";
}

std::string fallbackTimestamp(const uint32_t uptimeMs) {
  return "boot+" + std::to_string(uptimeMs);
}

std::string formatUptime(const uint64_t uptimeMs) {
  const uint64_t totalSeconds = uptimeMs / 1000ULL;
  const uint64_t days = totalSeconds / 86400ULL;
  const uint64_t hours = (totalSeconds / 3600ULL) % 24ULL;
  const uint64_t minutes = (totalSeconds / 60ULL) % 60ULL;
  const uint64_t seconds = totalSeconds % 60ULL;
  char buffer[32];
  std::snprintf(buffer,
                sizeof(buffer),
                "%02llu:%02llu:%02llu:%02llu",
                static_cast<unsigned long long>(days),
                static_cast<unsigned long long>(hours),
                static_cast<unsigned long long>(minutes),
                static_cast<unsigned long long>(seconds));
  return std::string(buffer);
}

std::string formatTimestamp(const int year,
                            const int month,
                            const int day,
                            const int hour,
                            const int minute,
                            const int second) {
  char buffer[24];
  std::snprintf(buffer,
                sizeof(buffer),
                "%04d-%02d-%02d %02d:%02d:%02d",
                year,
                month,
                day,
                hour,
                minute,
                second);
  return std::string(buffer);
}

std::string formatDateStamp(const int year, const int month, const int day) {
  char buffer[16];
  std::snprintf(buffer, sizeof(buffer), "%04d%02d%02d", year, month, day);
  return std::string(buffer);
}

std::string normalizeLogFileName(std::string_view rawName) {
  std::string candidate = trimWhitespace(rawName);
  if (candidate.empty()) {
    return "";
  }

  if (candidate.front() != '/') {
    candidate.insert(candidate.begin(), '/');
  }

  for (const char ch : candidate) {
    const bool allowed = std::isalnum(static_cast<unsigned char>(ch)) != 0 ||
                         ch == '/' || ch == '-' || ch == '_' || ch == '.';
    if (!allowed) {
      return "";
    }
  }

  if (candidate.find("..") != std::string::npos) {
    return "";
  }

  return candidate;
}

std::string normalizeTopicSegment(std::string_view rawSegment) {
  std::string trimmed = trimWhitespace(rawSegment);
  std::string normalized;
  normalized.reserve(trimmed.size());

  for (const char ch : trimmed) {
    const unsigned char value = static_cast<unsigned char>(ch);
    if (std::isalnum(value) != 0 || ch == '-' || ch == '_') {
      normalized.push_back(static_cast<char>(std::tolower(value)));
    } else if (ch == ' ' || ch == '/' || ch == '.') {
      if (normalized.empty() || normalized.back() == '-') {
        continue;
      }
      normalized.push_back('-');
    }
  }

  while (!normalized.empty() && normalized.back() == '-') {
    normalized.pop_back();
  }

  return normalized;
}

bool mqttIdentityMatches(const std::string_view deviceId, const std::string_view username) {
  if (username.empty()) {
    return true;
  }
  std::string normalizedDevice = normalizeTopicSegment(deviceId);
  if (normalizedDevice.empty()) {
    normalizedDevice = "mda-logger";
  }
  return username == normalizedDevice;
}

std::string formatUploadTopic(std::string_view prefix, std::string_view deviceId, std::string_view leaf) {
  std::string normalizedPrefix = trimWhitespace(prefix);
  while (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
    normalizedPrefix.erase(normalizedPrefix.begin());
  }
  while (!normalizedPrefix.empty() && normalizedPrefix.back() == '/') {
    normalizedPrefix.pop_back();
  }

  const std::string normalizedDevice = normalizeTopicSegment(deviceId);
  const std::string normalizedLeaf = normalizeTopicSegment(leaf);

  std::string topic = normalizedPrefix.empty() ? std::string("motorsport") : normalizedPrefix;
  if (!normalizedDevice.empty()) {
    topic += "/" + normalizedDevice;
  }
  if (!normalizedLeaf.empty()) {
    topic += "/" + normalizedLeaf;
  }
  return topic;
}

std::string formatSessionId(std::string_view deviceId, const uint32_t bootCounter) {
  const std::string normalizedDevice = normalizeTopicSegment(deviceId);
  const std::string prefix = normalizedDevice.empty() ? "logger" : normalizedDevice;
  return prefix + "-boot-" + std::to_string(bootCounter);
}

std::string formatPairingCode(uint64_t entropy) {
  constexpr char kPairingAlphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  char pairingBuffer[10]{};
  for (size_t index = 0; index < 8; ++index) {
    const size_t outputIndex = index < 4 ? index : index + 1;
    pairingBuffer[outputIndex] = kPairingAlphabet[entropy & 0x1FULL];
    entropy >>= 5;
  }
  pairingBuffer[4] = '-';
  return std::string(pairingBuffer);
}

}  // namespace Logic
