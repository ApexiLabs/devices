#include "StatusDiagnostics.h"
#include <cassert>
#include <iostream>
#include <limits>
#if defined(ESP32)
#include <Preferences.h>
#endif

int main() {
  StatusDiagnostics status;
  assert(status.json(false, 0, 0) == "{}");
  assert(status.json(true, 0, 0) ==
         "{\"store_forward_pending_records\":0,\"store_forward_dropped_records\":0}");
  assert(status.json(true, 5, 0, false) == "{\"store_forward_pending_records\":5}");
  status.observeClockFault(true);
  assert(status.json(false, 0, 0) == "{\"clock_fault\":true}");
  status.observeClockFault(false);
  status.configurationRejected();
  assert(status.json(false, 0, 0) == "{\"clock_fault\":false,\"config_rejected\":true}");
  status.observeDesired(4, 2);
  status.configurationAccepted();
  assert(status.json(false, 0, 0).find("\"config_stale\":true") != std::string::npos);
  status.configurationRejected();
  status.observeDesired(2, 2);  // An older replay must not clear unapplied v4.
  assert(status.json(false, 0, 0).find("\"config_stale\":true") != std::string::npos);
  status.configurationApplied(4);
  assert(status.json(false, 0, 0).find("\"config_stale\":false,\"config_rejected\":false") != std::string::npos);
  status.transportAttempt(true);  // Initial attempt excluded.
  assert(status.json(false, 0, 0).find("\"reconnects_total\":0") != std::string::npos);
  status.transportAttempt(false); // Healthy HTTPS request excluded.
  status.transportAttempt(true);
  status.transportAttempt(true);  // Failed retries each count.
  assert(status.json(false, 0, 0).find("\"reconnects_total\":2") != std::string::npos);
  const auto max = std::numeric_limits<uint32_t>::max();
  assert(StatusDiagnostics::saturatingAdd(max) == max);
  assert(StatusDiagnostics::saturatingAdd(max - 2, 3) == max);
  assert(StatusDiagnostics::saturatingAdd(4, 5) == 9);
#if defined(ESP32)
  Preferences::values.clear();
  Preferences::failOpen = true;
  StatusDiagnostics unavailable;
  unavailable.recordCompletedBoot();
  assert(unavailable.json(false, 0, 0) == "{}");
  Preferences::failOpen = false;
  status.recordCompletedBoot();
  status.recordCompletedBoot();
  assert(status.json(false, 0, 0).find("\"reboots_total\":1") != std::string::npos);
  StatusDiagnostics reboot;
  reboot.recordCompletedBoot();
  assert(reboot.json(false, 0, 0) == "{\"reboots_total\":2}");
  Preferences::failKey = "boots";
  StatusDiagnostics failed;
  failed.recordCompletedBoot();
  assert(failed.json(false, 0, 0) == "{}");
  Preferences::failKey.clear();
  StatusDiagnostics recovered;
  recovered.recordCompletedBoot();
  assert(recovered.json(false, 0, 0) == "{\"reboots_total\":3}");
  Preferences::cutAfterKey = "boots";
  StatusDiagnostics interrupted;
  try {
    interrupted.recordCompletedBoot();
    assert(false);
  } catch (const Preferences::PowerCut &) {}
  Preferences::cutAfterKey.clear();
  StatusDiagnostics afterCut;
  afterCut.recordCompletedBoot();
  assert(afterCut.json(false, 0, 0) == "{\"reboots_total\":5}");
  Preferences::values["boots"] = "0";
  StatusDiagnostics invalid;
  invalid.recordCompletedBoot();
  assert(invalid.json(false, 0, 0) == "{}");
  Preferences::values["boots"] = std::to_string(max);
  StatusDiagnostics saturated;
  saturated.recordCompletedBoot();
  assert(saturated.json(false, 0, 0) == "{\"reboots_total\":4294967295}");
#else
  status.recordCompletedBoot();
  assert(status.json(false, 0, 0).find("reboots_total") == std::string::npos);
#endif
  // Maximum diagnostics contributes fewer than 320 bytes to the status frame.
  assert(status.json(true, max, max).size() < 320);
  std::cout << "Status diagnostics tests passed\n";
}
