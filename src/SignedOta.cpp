#include "SignedOta.h"

bool SignedOta::fail(const char *error) {
  if (state_ == State::Receiving) backend_.abort();
  state_ = State::Failed;
  error_ = error;
  return false;
}

bool SignedOta::begin(const size_t bytes) {
  if (state_ == State::Receiving || state_ == State::ReadyToReboot) return false;
  if (!backend_.securePosture()) return fail("secure production posture required");
  if (backend_.bootState() != SignedOtaBackend::Boot::Valid) return fail("running image is not confirmed");
  if (bytes == 0 || bytes > backend_.inactiveCapacity()) return fail("image exceeds inactive slot capacity");
  if (!backend_.beginInactive(bytes)) return fail("inactive slot preparation failed");
  expected_ = bytes;
  received_ = 0;
  state_ = State::Receiving;
  error_ = "";
  return true;
}

bool SignedOta::append(const uint8_t *data, const size_t bytes) {
  if (state_ != State::Receiving) return false;
  if (data == nullptr || bytes == 0 || bytes > 4096 || bytes > expected_ - received_) {
    return fail("invalid image chunk or length");
  }
  if (!backend_.write(data, bytes)) return fail("inactive slot write failed");
  received_ += bytes;
  return true;
}

bool SignedOta::finish() {
  if (state_ != State::Receiving) return false;
  if (received_ != expected_) return fail("incomplete image");
  if (!backend_.securePosture()) return fail("security posture changed");
  if (!backend_.finishAndVerify()) return fail("image signature or release policy rejected");
  if (!backend_.selectVerified()) return fail("verified image selection failed");
  state_ = State::ReadyToReboot;
  return true;
}

void SignedOta::cancel() {
  if (state_ == State::Receiving) fail("update cancelled; current slot preserved");
}

void OtaBootHealth::begin(const uint32_t nowMs, const bool productionRequired) {
  startedMs_ = nowMs;
  healthy_ = false;
  state_ = State::Inactive;
  if (!productionRequired) return;
  const auto boot = backend_.bootState();
  if (boot == SignedOtaBackend::Boot::Pending) state_ = State::Pending;
  else if (boot == SignedOtaBackend::Boot::Unknown) state_ = State::RecoveryRequired;
}

void OtaBootHealth::poll(const uint32_t nowMs, const bool sensorsReady,
                         const bool queueReady, const bool authenticatedHeartbeat) {
  if (state_ != State::Pending) return;
  // Never confirm a candidate after the deadline, even if it becomes healthy
  // on that iteration. A failed rollback remains a visible recovery fault.
  if (uint32_t(nowMs - startedMs_) >= 120000) {
    state_ = backend_.rollbackPossible() && backend_.rollback() ? State::RolledBack : State::RecoveryRequired;
    return;
  }
  if (!backend_.securePosture() || !sensorsReady || !queueReady || !authenticatedHeartbeat) {
    healthy_ = false;
    return;
  }
  if (!healthy_) { healthy_ = true; healthySinceMs_ = nowMs; }
  if (uint32_t(nowMs - healthySinceMs_) >= 10000) {
    if (backend_.confirmHealthy()) state_ = State::Confirmed;
    else state_ = backend_.rollbackPossible() && backend_.rollback() ? State::RolledBack : State::RecoveryRequired;
  }
}

const char *OtaBootHealth::status() const {
  switch (state_) {
    case State::Pending: return "pending-health";
    case State::Confirmed: return "confirmed";
    case State::RolledBack: return "rolled-back";
    case State::RecoveryRequired: return "recovery-required";
    default: return "not-pending";
  }
}
