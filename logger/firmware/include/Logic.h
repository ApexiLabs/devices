#pragma once

#include <stdint.h>

#include <string>
#include <string_view>

#include "SensorTypes.h"

namespace Logic {

constexpr uint16_t kLivePayloadSchemaVersion = 1;

float currentFromVoltage(float voltage, float shuntResistanceOhms);
SensorFault determineSensorFault(float voltage, float currentmA, bool adcAvailable);
float scaleEngineeringValue(float currentmA,
                            float currentMinmA,
                            float currentMaxmA,
                            float engMin,
                            float engMax);
float applyLowPassFilter(float previousValue, float currentValue, float alpha);
bool intervalElapsed(uint32_t nowMs, uint32_t previousMs, uint32_t intervalMs);
bool isRetryableHttpStatus(int status);
bool shouldDiscardQueuedHttpStatus(int status);
bool isValidRecorderAssignmentStatus(std::string_view status);
bool productionSecurityAllowsNetwork(bool required,
                                     bool secureBootEnabled,
                                     bool flashEncryptionReleaseMode);

std::string fallbackTimestamp(uint32_t uptimeMs);
std::string formatUptime(uint64_t uptimeMs);
std::string formatTimestamp(int year, int month, int day, int hour, int minute, int second);
std::string formatDateStamp(int year, int month, int day);
std::string normalizeLogFileName(std::string_view rawName);
std::string normalizeTopicSegment(std::string_view rawSegment);
bool mqttIdentityMatches(std::string_view deviceId, std::string_view username);
std::string formatUploadTopic(std::string_view prefix, std::string_view deviceId, std::string_view leaf);
std::string formatSessionId(std::string_view deviceId, uint32_t bootCounter);
std::string formatPairingCode(uint64_t entropy);

}  // namespace Logic
