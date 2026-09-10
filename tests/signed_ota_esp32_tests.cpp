#include "SignedOtaEsp32.h"
#include <cassert>
#include <iostream>
extern "C" bool verifyRollbackLater();

int main() {
  assert(verifyRollbackLater());
  SignedOtaEsp32 backend;
  assert(backend.securePosture());
  FakeOta::keys.encrypted = false;
  assert(!backend.securePosture());
  FakeOta::keys.encrypted = true;
  FakeOta::queue.encrypted = false;
  assert(!backend.securePosture());
  FakeOta::queue.encrypted = true;
  FakeOta::secure = false;
  assert(!backend.beginInactive(8192));
  FakeOta::secure = true;
  FakeOta::next.address = FakeOta::running.address;
  assert(!backend.beginInactive(8192));
  FakeOta::next.address = 0x210000;
  FakeOta::state = ESP_OTA_IMG_PENDING_VERIFY;
  assert(!backend.beginInactive(8192));
  FakeOta::state = ESP_OTA_IMG_VALID;
  assert(!backend.selectVerified());
  for (int fault = 0; fault < 3; ++fault) {
    assert(backend.beginInactive(8192));
    FakeOta::signature = fault != 0;
    FakeOta::sameProject = fault != 1;
    FakeOta::candidateVersion = fault == 2 ? 1 : 0;
    assert(!backend.finishAndVerify());
    assert(!backend.selectVerified());
    backend.abort();
    assert(FakeOta::aborts == 0);  // end already consumed the handle.
    FakeOta::signature = true;
    FakeOta::sameProject = true;
    FakeOta::candidateVersion = 0;
  }
  assert(backend.beginInactive(8192));
  assert(backend.finishAndVerify());
  assert(backend.selectVerified());
  assert(!backend.selectVerified() && FakeOta::selections == 1);
  assert(!backend.confirmHealthy());
  FakeOta::state = ESP_OTA_IMG_PENDING_VERIFY;
  assert(backend.confirmHealthy() && FakeOta::confirms == 1);
  FakeOta::metadata = false;
  assert(backend.bootState() == SignedOtaBackend::Boot::Unknown);
  std::cout << "ESP32 OTA adapter tests passed (fake SDK only)\n";
}
