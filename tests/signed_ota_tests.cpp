#include "SignedOta.h"
#include <cassert>
#include <iostream>
#include <vector>

struct Backend : SignedOtaBackend {
  bool secure = true, prepare = true, writes = true, signature = true, selection = true;
  bool confirmation = true, canRollback = true, rollbackResult = true;
  Boot boot = Boot::Valid;
  int begins = 0, verifies = 0, selections = 0, aborts = 0, confirms = 0, rollbacks = 0;
  size_t capacity = 8192;
  bool securePosture() const override { return secure; }
  Boot bootState() const override { return boot; }
  size_t inactiveCapacity() const override { return capacity; }
  bool beginInactive(size_t) override { ++begins; return prepare; }
  bool write(const uint8_t *, size_t) override { return writes; }
  bool finishAndVerify() override { ++verifies; return signature; }
  bool selectVerified() override { assert(verifies > 0 && signature); ++selections; return selection; }
  void abort() override { ++aborts; }
  bool confirmHealthy() override { ++confirms; return confirmation; }
  bool rollbackPossible() const override { return canRollback; }
  bool rollback() override { ++rollbacks; return rollbackResult; }
};

int main() {
  assert(!OtaPolicy::legacyAllowed(true, true));
  assert(!OtaPolicy::legacyAllowed(true, false));
  assert(OtaPolicy::legacyAllowed(false, true));
  assert(!OtaPolicy::legacyAllowed(false, false));
  const uint8_t chunk[4]{1, 2, 3, 4};
  for (int rejected = 0; rejected < 4; ++rejected) {
    Backend backend;
    if (rejected == 0) backend.secure = false;
    if (rejected == 1) backend.boot = Backend::Boot::Pending;
    if (rejected == 2) backend.boot = Backend::Boot::Unknown;
    if (rejected == 3) backend.capacity = 3;
    SignedOta ota(backend);
    assert(!ota.begin(4) && backend.begins == 0 && backend.selections == 0);
  }
  for (int failure = 0; failure < 6; ++failure) {
    Backend backend;
    SignedOta ota(backend);
    assert(ota.begin(4));
    if (failure == 0) { assert(ota.append(chunk, 2)); assert(!ota.finish()); }
    if (failure == 1) assert(!ota.append(chunk, 5));
    if (failure == 2) { backend.writes = false; assert(!ota.append(chunk, 4)); }
    if (failure == 3) { assert(ota.append(chunk, 4)); backend.signature = false; assert(!ota.finish()); }
    if (failure == 4) { assert(ota.append(chunk, 4)); backend.secure = false; assert(!ota.finish()); }
    if (failure == 5) ota.cancel();
    assert(ota.state() == SignedOta::State::Failed && backend.selections == 0 && backend.aborts == 1);
  }
  {
    Backend backend;
    SignedOta ota(backend);
    assert(!ota.begin(0));
    assert(ota.begin(4));
    assert(!ota.begin(4));
    assert(ota.append(chunk, 4));
    assert(ota.finish() && backend.verifies == 1 && backend.selections == 1);
    assert(!ota.begin(4) && !ota.finish());
    assert(ota.state() == SignedOta::State::ReadyToReboot);
  }
  {
    Backend backend;
    backend.selection = false;
    SignedOta ota(backend);
    assert(ota.begin(4) && ota.append(chunk, 4) && !ota.finish());
    assert(ota.state() == SignedOta::State::Failed);
  }
  {
    Backend backend;
    backend.boot = Backend::Boot::Pending;
    OtaBootHealth health(backend);
    health.begin(0, true);
    health.poll(100, true, true, false);
    health.poll(20000, true, true, false);
    assert(backend.confirms == 0);  // Connection alone cannot confirm.
    health.poll(30000, true, true, true);
    health.poll(39999, true, true, true);
    assert(backend.confirms == 0);
    health.poll(40000, true, true, true);
    assert(backend.confirms == 1 && health.state() == OtaBootHealth::State::Confirmed);
    health.poll(120000, true, true, true);
    assert(backend.confirms == 1 && backend.rollbacks == 0);
  }
  for (int fault = 0; fault < 4; ++fault) {
    Backend backend;
    backend.boot = Backend::Boot::Pending;
    if (fault == 3) backend.secure = false;
    OtaBootHealth health(backend);
    health.begin(UINT32_MAX - 10000, true);
    health.poll(0, fault != 0, fault != 1, fault != 2);
    health.poll(50000, fault != 0, fault != 1, fault != 2);
    assert(backend.confirms == 0);
    health.poll(110000, true, true, true);
    assert(backend.confirms == 0 && backend.rollbacks == 1);
  }
  for (int failure = 0; failure < 4; ++failure) {
    Backend backend;
    backend.boot = Backend::Boot::Pending;
    if (failure == 0) backend.canRollback = false;
    if (failure == 1) backend.rollbackResult = false;
    if (failure >= 2) backend.confirmation = false;
    if (failure == 3) backend.rollbackResult = false;
    OtaBootHealth health(backend);
    health.begin(0, true);
    health.poll(1, failure >= 2, true, true);
    health.poll(failure >= 2 ? 10001 : 120000, failure >= 2, true, true);
    assert(health.state() == (failure == 2 ? OtaBootHealth::State::RolledBack : OtaBootHealth::State::RecoveryRequired));
    if (failure >= 2) assert(backend.confirms == 1 && backend.rollbacks == 1);
    const auto calls = backend.confirms + backend.rollbacks;
    health.poll(130000, true, true, true);
    assert(calls == backend.confirms + backend.rollbacks);
  }
  {
    Backend backend;
    backend.boot = Backend::Boot::Pending;
    OtaBootHealth health(backend);
    health.begin(0, false);
    health.poll(120000, true, true, true);
    assert(backend.confirms == 0 && backend.rollbacks == 0);
  }
  std::cout << "Signed OTA and boot health tests passed\n";
}
