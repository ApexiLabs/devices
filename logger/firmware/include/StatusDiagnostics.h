#pragma once

#include <cstdint>
#include <optional>
#include <string>

class StatusDiagnostics {
 public:
  static uint32_t saturatingAdd(uint32_t value, uint32_t increment = 1);
  void recordCompletedBoot();
  void observeClockFault(bool fault);
  void observeDesired(uint32_t desired, uint32_t applied);
  void configurationRejected();
  void configurationAccepted();
  void configurationApplied(uint32_t applied);
  void transportAttempt(bool recovering);
  std::string json(bool queueAvailable, uint32_t pending, uint32_t dropped,
                   bool dropsKnown = true) const;

 private:
  bool bootRecorded_ = false;
  bool attempted_ = false;
  uint32_t reconnects_ = 0;
  uint32_t desiredVersion_ = 0;
  std::optional<uint32_t> reboots_;
  std::optional<bool> clockFault_;
  std::optional<bool> configStale_;
  std::optional<bool> configRejected_;
};
