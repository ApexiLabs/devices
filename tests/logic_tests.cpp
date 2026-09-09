#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include "Logic.h"
#include "DashLinkProtocol.h"
#include "DashWifiPolicy.h"

namespace {

int failures = 0;

void expectNear(const float actual, const float expected, const float tolerance, const std::string &message) {
  if (std::fabs(actual - expected) > tolerance) {
    std::cerr << "FAIL: " << message << " expected=" << expected << " actual=" << actual << "\n";
    ++failures;
  }
}

void expectEqual(const std::string &actual, const std::string &expected, const std::string &message) {
  if (actual != expected) {
    std::cerr << "FAIL: " << message << " expected='" << expected << "' actual='" << actual << "'\n";
    ++failures;
  }
}

void expectEqual(const int actual, const int expected, const std::string &message) {
  if (actual != expected) {
    std::cerr << "FAIL: " << message << " expected=" << expected << " actual=" << actual << "\n";
    ++failures;
  }
}

void expectFault(const SensorFault actual, const SensorFault expected, const std::string &message) {
  if (actual != expected) {
    std::cerr << "FAIL: " << message << " expected=" << sensorFaultToString(expected)
              << " actual=" << sensorFaultToString(actual) << "\n";
    ++failures;
  }
}

void testCurrentConversion() {
  expectNear(Logic::currentFromVoltage(0.66f, 165.0f), 4.0f, 0.001f, "4 mA conversion");
  expectNear(Logic::currentFromVoltage(3.30f, 165.0f), 20.0f, 0.001f, "20 mA conversion");
  expectNear(Logic::currentFromVoltage(1.98f, 165.0f), 12.0f, 0.001f, "12 mA conversion");
  expectNear(Logic::currentFromVoltage(1.0f, 0.0f), 0.0f, 0.001f, "zero-ohm guard");
}

void testFaultThresholds() {
  expectFault(Logic::determineSensorFault(1.0f, 12.0f, false), SensorFault::AdcUnavailable,
              "ADC unavailable fault");
  expectFault(Logic::determineSensorFault(3.28f, 19.9f, true), SensorFault::AdcSaturation,
              "saturation threshold");
  expectFault(Logic::determineSensorFault(0.59f, 3.59f, true), SensorFault::Underrange,
              "underrange below threshold");
  expectFault(Logic::determineSensorFault(0.594f, 3.60f, true), SensorFault::None,
              "3.6 mA is valid");
  expectFault(Logic::determineSensorFault(3.20f, 20.81f, true), SensorFault::Overrange,
              "overrange above threshold");
  expectFault(Logic::determineSensorFault(3.20f, 20.80f, true), SensorFault::None,
              "20.8 mA is valid");
}

void testEngineeringScaling() {
  expectNear(Logic::scaleEngineeringValue(4.0f, 4.0f, 20.0f, 0.0f, 10.0f), 0.0f, 0.001f,
             "4 mA maps to min engineering value");
  expectNear(Logic::scaleEngineeringValue(12.0f, 4.0f, 20.0f, 0.0f, 10.0f), 5.0f, 0.001f,
             "midpoint maps correctly");
  expectNear(Logic::scaleEngineeringValue(20.0f, 4.0f, 20.0f, 0.0f, 10.0f), 10.0f, 0.001f,
             "20 mA maps to max engineering value");
  expectNear(Logic::scaleEngineeringValue(2.0f, 4.0f, 20.0f, 0.0f, 10.0f), 0.0f, 0.001f,
             "below range clamps low");
  expectNear(Logic::scaleEngineeringValue(25.0f, 4.0f, 20.0f, 0.0f, 10.0f), 10.0f, 0.001f,
             "above range clamps high");
  expectNear(Logic::scaleEngineeringValue(12.0f, 4.0f, 4.0f, 0.0f, 10.0f), 0.0f, 0.001f,
             "invalid scaling range guard");
}

void testFilter() {
  expectNear(Logic::applyLowPassFilter(10.0f, 20.0f, 0.18f), 11.8f, 0.001f, "normal low-pass step");
  expectNear(Logic::applyLowPassFilter(10.0f, 20.0f, -1.0f), 10.0f, 0.001f, "alpha clamps low");
  expectNear(Logic::applyLowPassFilter(10.0f, 20.0f, 2.0f), 20.0f, 0.001f, "alpha clamps high");
}

void testIntervalTiming() {
  expectEqual(Logic::intervalElapsed(60000, 0, 60000), 1,
              "interval is due at boundary");
  expectEqual(Logic::intervalElapsed(59999, 0, 60000), 0,
              "interval is not due early");
  expectEqual(Logic::intervalElapsed(25, 0xFFFFFFF0U, 40), 1,
              "interval timing survives millis wrap");
}

void testDashRetryTiming() {
  expectEqual(DashLinkProtocol::retryDue(5000, 0, 5000), 1,
              "dash retry is due at boundary");
  expectEqual(DashLinkProtocol::retryDue(4999, 0, 5000), 0,
              "dash retry is not due early");
  expectEqual(DashLinkProtocol::retryDue(25, 0xFFFFFFF0U, 40), 1,
              "dash retry timing survives millis wrap");
}

void testHttpRetryPolicy() {
  expectEqual(Logic::isRetryableHttpStatus(-1), 1, "retries transport failures");
  expectEqual(Logic::isRetryableHttpStatus(502), 1, "retries gateway failures");
  expectEqual(Logic::isRetryableHttpStatus(429), 1, "retries rate limits");
  expectEqual(Logic::isRetryableHttpStatus(401), 0, "does not retry bad credentials");
  expectEqual(Logic::shouldDiscardQueuedHttpStatus(422), 1,
              "discards permanently invalid queued payloads");
  expectEqual(Logic::shouldDiscardQueuedHttpStatus(503), 0,
              "retains queued payloads during server outages");
}

void testTimestampFormatting() {
  expectEqual(Logic::fallbackTimestamp(12345), "boot+12345", "fallback timestamp");
  expectEqual(Logic::formatUptime(0), "00:00:00:00", "zero uptime");
  expectEqual(Logic::formatUptime(93784000), "01:02:03:04",
              "formats uptime as days hours minutes seconds");
  expectEqual(Logic::formatUptime(1ULL << 32), "49:17:02:47",
              "formats extended uptime beyond millis wrap");
  expectEqual(Logic::formatTimestamp(2026, 3, 4, 5, 6, 7), "2026-03-04 05:06:07",
              "RTC timestamp formatting");
  expectEqual(Logic::formatDateStamp(2026, 3, 4), "20260304", "date stamp formatting");
}

void testFileNameNormalization() {
  expectEqual(Logic::normalizeLogFileName(" logs-20260314.csv "), "/logs-20260314.csv",
              "trims and prepends slash");
  expectEqual(Logic::normalizeLogFileName("/nested/logs-20260314.csv"), "/nested/logs-20260314.csv",
              "keeps valid nested path");
  expectEqual(Logic::normalizeLogFileName("../secret.csv"), "", "rejects traversal");
  expectEqual(Logic::normalizeLogFileName("bad name.csv"), "", "rejects spaces");
  expectEqual(Logic::normalizeLogFileName("bad$name.csv"), "", "rejects symbols");
  expectEqual(Logic::normalizeLogFileName(""), "", "rejects empty names");
}

void testUploadIdentifiers() {
  expectEqual(static_cast<int>(Logic::kLivePayloadSchemaVersion), 1,
              "pins live MQTT payload schema version");
  expectEqual(Logic::normalizeTopicSegment(" Car 01 / Logger "), "car-01-logger",
              "normalizes MQTT topic segment");
  expectEqual(Logic::mqttIdentityMatches(" Car 01 / Logger ", "car-01-logger"), 1,
              "accepts username matching normalized device ID");
  expectEqual(Logic::mqttIdentityMatches("Car 01", ""), 1,
              "allows anonymous MQTT configuration");
  expectEqual(Logic::mqttIdentityMatches("Car 01", "Car 01"), 0,
              "rejects unnormalized MQTT username");
  expectEqual(Logic::mqttIdentityMatches("Car 01", "another-device"), 0,
              "rejects another device identity");
  expectEqual(Logic::formatUploadTopic("/motorsport/live/", "Car 01", "Telemetry"),
              "motorsport/live/car-01/telemetry", "formats MQTT topic");
  expectEqual(Logic::formatSessionId("Car 01", 42), "car-01-boot-42", "formats session id");
  expectEqual(Logic::formatPairingCode(0), "AAAA-AAAA", "formats zero pairing entropy");
  const std::string pairingCode = Logic::formatPairingCode(0x123456789aULL);
  expectEqual(static_cast<int>(pairingCode.size()), 9, "formats pairing code with separator");
  expectEqual(static_cast<int>(pairingCode[4]), static_cast<int>('-'),
              "places pairing code separator");
}

}  // namespace

int main() {
  expectEqual(DashWifiPolicy::shouldRetry(true, false, 29999, 0), 0,
              "Wi-Fi attempt gets full connection interval");
  expectEqual(DashWifiPolicy::shouldRetry(true, false, 30000, 0), 1,
              "Wi-Fi retries at interval boundary");
  expectEqual(DashWifiPolicy::shouldRetry(true, true, 30000, 0), 0,
              "Wi-Fi does not interrupt a connected station");
  expectEqual(DashWifiPolicy::shouldRetry(false, false, 30000, 0), 0,
              "Wi-Fi does not attempt empty credentials");
  expectEqual(DashWifiPolicy::shouldRetry(true, false, 29984, 0xfffffff0U), 1,
              "Wi-Fi retry survives millis rollover");
  testCurrentConversion();
  testFaultThresholds();
  testEngineeringScaling();
  testFilter();
  testIntervalTiming();
  testDashRetryTiming();
  testHttpRetryPolicy();
  testTimestampFormatting();
  testFileNameNormalization();
  testUploadIdentifiers();

  if (failures == 0) {
    std::cout << "All host logic tests passed\n";
    return EXIT_SUCCESS;
  }

  std::cerr << failures << " host logic test(s) failed\n";
  return EXIT_FAILURE;
}
