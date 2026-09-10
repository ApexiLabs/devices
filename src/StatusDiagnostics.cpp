#include "StatusDiagnostics.h"

#include <limits>
#if defined(ESP32)
#include <Preferences.h>
#endif

uint32_t StatusDiagnostics::saturatingAdd(uint32_t value, uint32_t increment) {
  const uint32_t maximum = std::numeric_limits<uint32_t>::max();
  return increment > maximum - value ? maximum : value + increment;
}

void StatusDiagnostics::recordCompletedBoot() {
  if (bootRecorded_) return;
  bootRecorded_ = true;
#if defined(ESP32)
  Preferences store;
  if (!store.begin("apexi_diag", false)) return;
  const uint32_t previous = store.getUInt("boots", 0);
  if (store.isKey("boots") && previous == 0) {
    store.end();
    return;  // Present but unreadable/invalid is not a new counter.
  }
  const uint32_t next = saturatingAdd(previous);
  if (store.putUInt("boots", next) > 0) reboots_ = next;
  store.end();
#endif
}

void StatusDiagnostics::observeClockFault(bool fault) { clockFault_ = fault; }

void StatusDiagnostics::observeDesired(uint32_t desired, uint32_t applied) {
  if (desired > desiredVersion_) desiredVersion_ = desired;
  configStale_ = desiredVersion_ > applied;
}

void StatusDiagnostics::configurationRejected() { configRejected_ = true; }
void StatusDiagnostics::configurationAccepted() { configRejected_ = false; }
void StatusDiagnostics::configurationApplied(uint32_t applied) {
  if (configStale_.has_value()) configStale_ = desiredVersion_ > applied;
  configRejected_ = false;
}

void StatusDiagnostics::transportAttempt(bool recovering) {
  if (attempted_ && recovering) reconnects_ = saturatingAdd(reconnects_);
  attempted_ = true;
}

std::string StatusDiagnostics::json(bool queueAvailable, uint32_t pending, uint32_t dropped,
                                    bool dropsKnown) const {
  std::string result = "{";
  const auto add = [&result](const char *key, const std::string &value) {
    if (result.size() > 1) result += ',';
    result += '"';
    result += key;
    result += "\":";
    result += value;
  };
  if (queueAvailable) {
    add("store_forward_pending_records", std::to_string(pending));
    if (dropsKnown) add("store_forward_dropped_records", std::to_string(dropped));
  }
  if (clockFault_.has_value()) add("clock_fault", *clockFault_ ? "true" : "false");
  if (configStale_.has_value()) add("config_stale", *configStale_ ? "true" : "false");
  if (configRejected_.has_value()) add("config_rejected", *configRejected_ ? "true" : "false");
  if (attempted_) add("reconnects_total", std::to_string(reconnects_));
  if (reboots_.has_value()) add("reboots_total", std::to_string(*reboots_));
  return result + '}';
}
