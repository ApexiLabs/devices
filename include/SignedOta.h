#pragma once
#include <cstddef>
#include <cstdint>

namespace OtaPolicy {
constexpr bool legacyAllowed(bool productionRequired, bool enabled) {
  return enabled && !productionRequired;
}
}

// No network/serial ingress is provided here. A future authenticated delivery
// service must authorize an update before calling this single-owner writer.
class SignedOtaBackend {
 public:
  enum class Boot { Valid, Pending, Unknown };
  virtual ~SignedOtaBackend() = default;
  virtual bool securePosture() const = 0;
  virtual Boot bootState() const = 0;
  virtual size_t inactiveCapacity() const = 0;
  virtual bool beginInactive(size_t bytes) = 0;
  virtual bool write(const uint8_t *data, size_t bytes) = 0;
  // Must verify the boot signature AND target/project/security-version policy.
  virtual bool finishAndVerify() = 0;
  virtual bool selectVerified() = 0;
  virtual void abort() = 0;
  virtual bool confirmHealthy() = 0;
  virtual bool rollbackPossible() const = 0;
  virtual bool rollback() = 0;
};

class SignedOta {
 public:
  enum class State { Idle, Receiving, ReadyToReboot, Failed };
  explicit SignedOta(SignedOtaBackend &backend) : backend_(backend) {}
  bool begin(size_t bytes);
  bool append(const uint8_t *data, size_t bytes);
  bool finish();
  void cancel();
  State state() const { return state_; }
  const char *error() const { return error_; }
 private:
  bool fail(const char *error);
  SignedOtaBackend &backend_;
  State state_ = State::Idle;
  size_t expected_ = 0;
  size_t received_ = 0;
  const char *error_ = "";
};

class OtaBootHealth {
 public:
  enum class State { Inactive, Pending, Confirmed, RolledBack, RecoveryRequired };
  explicit OtaBootHealth(SignedOtaBackend &backend) : backend_(backend) {}
  void begin(uint32_t nowMs, bool productionRequired);
  void poll(uint32_t nowMs, bool sensorsReady, bool queueReady, bool authenticatedHeartbeat);
  const char *status() const;
  State state() const { return state_; }
 private:
  SignedOtaBackend &backend_;
  State state_ = State::Inactive;
  uint32_t startedMs_ = 0;
  uint32_t healthySinceMs_ = 0;
  bool healthy_ = false;
};
