#include "DeviceProvisioning.h"
#include "OwnerReset.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <cassert>
#include <iostream>

namespace {
int callbackCalls = 0;
OwnerReset::Coordinator reset;
struct Latch {
  bool getPending(bool &pending) {
    Preferences p; if (!p.begin("reset_latch", true)) return false;
    pending = p.getBool("pending", false); return true;
  }
  bool markPending(bool pending) {
    Preferences p; return p.begin("reset_latch", false) && p.putBool("pending", pending);
  }
} latch;
bool clearNamespace(const char *name) {
  Preferences p; return p.begin(name, false) && p.clear();
}
// Exercise the real reset coordinator at DeviceProvisioning's callback boundary.
// Namespace contents stand in for credential consumers, not their implementations.
bool prepare() {
  ++callbackCalls;
  return reset.request(latch, [] { return clearNamespace("runtime"); },
      [] { return clearNamespace("rotation"); }, [] { return clearNamespace("auth"); },
      [] { return clearNamespace("apexi_owner"); }) == OwnerReset::Result::Completed;
}
bool deny() { ++callbackCalls; return false; }
bool rebootReset() {
  reset = OwnerReset::Coordinator{};
  reset.resume(latch, [] { return clearNamespace("runtime"); },
      [] { return clearNamespace("rotation"); }, [] { return clearNamespace("auth"); },
      [] { return clearNamespace("apexi_owner"); });
  return reset.networkAllowed();
}
DeviceProvisioning boot() {
  DeviceProvisioning device;
  device.begin(AppConfig::WifiConfig{}, AppConfig::OtaConfig{}, AppConfig::UploadConfig{});
  return device;
}
std::string command(const char *token) {
  DynamicJsonDocument doc(4096);
  doc["expected_device_id"] = "mda-123456789abc";
  doc["friendly_name"] = "Test logger";
  doc["wifi"]["ssid"] = "test-network";
  doc["wifi"]["password"] = "test-password";
  doc["ota_password"] = "test-ota-password";
  doc["upload"]["protocol"] = "https";
  doc["upload"]["host"] = "example.test";
  doc["upload"]["port"] = 443;
  doc["upload"]["cloudflare_client_id"] = "test-client";
  doc["upload"]["cloudflare_client_secret"] = "test-secret";
  doc["upload"]["app_device_token"] = token;
  doc["upload"]["app_token_subject"] = "logger:mda-123456789abc";
  doc["hardware_revision"] = "test-rev";
  doc["provisioned_at"] = "2026-09-10T00:00:00Z";
  doc["remote_management_enabled"] = true;
  std::string result = "APEXI_PROVISION "; serializeJson(doc, result); return result;
}
Preferences::Store oldState;
void restore() {
  Preferences::values = oldState; Preferences::resetInjection();
  reset = OwnerReset::Coordinator{}; callbackCalls = 0;
}
void assertNoStaleConsumers() {
  assert(Preferences::values["runtime"].empty());
  assert(Preferences::values["rotation"].empty());
  assert(Preferences::values["auth"].empty());
}
}

int main() {
  auto initial = boot();
  assert(initial.acceptSerialCommand(command("old-bearer").c_str(), prepare));
  Preferences::values["runtime"]["token"] = "old-runtime";
  Preferences::values["rotation"]["token"] = "old-rotated";
  Preferences::values["auth"]["token"] = "old-authorized";
  oldState = Preferences::values;
  const std::string valid = command("new-bearer");
  std::vector<std::string> invalid = {"wrong prefix", "APEXI_PROVISION {", "APEXI_PROVISION {}"};
  for (const auto &replacement : std::vector<std::pair<std::string, std::string>>{
      {"mda-123456789abc", "mda-ffffffffffff"}, {"test-rev", "other-rev"},
      {"test-password", "short"}, {"new-bearer", ""}, {"https", "invalid"},
      {"\"remote_management_enabled\":true", "\"remote_management_enabled\":\"yes\""}}) {
    std::string bad = valid;
    const auto pos = bad.find(replacement.first); assert(pos != std::string::npos);
    bad.replace(pos, replacement.first.size(), replacement.second); invalid.push_back(bad);
  }
  for (const auto &bad : invalid) {
    restore(); auto device = boot();
    assert(!device.acceptSerialCommand(bad.c_str(), prepare));
    assert(callbackCalls == 0); assert(Preferences::mutations.empty());
    assert(Preferences::values == oldState);
  }
  for (auto callback : {deny, static_cast<bool (*)()>(nullptr)}) {
    restore(); auto device = boot();
    assert(!device.acceptSerialCommand(valid.c_str(), callback));
    assert(Preferences::mutations.empty()); assert(Preferences::values == oldState);
  }
  restore(); auto inaccessibleFactory = boot();
  Preferences::failOpen = "apexi_factory";
  assert(!inaccessibleFactory.acceptSerialCommand(valid.c_str(), prepare));
  assert(callbackCalls == 0); assert(Preferences::values == oldState);
  restore(); auto device = boot();
  assert(device.acceptSerialCommand(valid.c_str(), prepare));
  const auto operations = Preferences::mutations;
  assert(callbackCalls == 1); assertNoStaleConsumers();
  assert(std::string(boot().uploadConfig().appDeviceToken) == "new-bearer");
  assert(operations.back() == "apexi_owner/ready");

  // Every durable mutation, including either side of ready, is a cut boundary.
  for (size_t point = 0; point < operations.size(); ++point) {
    for (bool after : {false, true}) {
      restore(); auto beforeCut = boot();
      Preferences::cutAt = int(point); Preferences::cutAfter = after;
      bool interrupted = false;
      try { beforeCut.acceptSerialCommand(valid.c_str(), prepare); }
      catch (const Preferences::PowerCut &) { interrupted = true; }
      assert(interrupted);
      Preferences::resetInjection(); assert(rebootReset());
      auto restarted = boot();
      if (point == 0 && !after) {
        // No durable reset request existed yet: only the unchanged old owner survives.
        assert(Preferences::values == oldState);
      } else {
        assertNoStaleConsumers();
        if (restarted.isProvisioned()) {
          assert(std::string(restarted.uploadConfig().appDeviceToken) == "new-bearer");
          assert(point == operations.size() - 1 && after);
        } else assert(std::string(restarted.uploadConfig().appDeviceToken).empty());
      }
    }
  }
  // A failed durable owner-reset operation must prevent ALL candidate writes.
  for (const char *space : {"runtime", "rotation", "auth", "apexi_owner"}) {
    restore(); auto failing = boot();
    Preferences::failMutation = std::string(space) + "/*";
    assert(!failing.acceptSerialCommand(valid.c_str(), prepare));
    assert(!reset.networkAllowed());
    for (const auto &entry : Preferences::mutations)
      assert(entry != "apexi_owner/ready" && entry != "apexi_owner/app_token");
    Preferences::resetInjection(); assert(rebootReset()); assertNoStaleConsumers();
    assert(!boot().isProvisioned());
  }
  // Ordinary candidate-write failures roll back, including failure to commit
  // the final ready flag. They must not resurrect the previous owner on reboot.
  for (const auto &operation : operations) {
    if (operation.rfind("apexi_owner/", 0) != 0 || operation == "apexi_owner/*") continue;
    restore(); auto failing = boot(); Preferences::failMutation = operation;
    assert(!failing.acceptSerialCommand(valid.c_str(), prepare));
    Preferences::resetInjection(); assert(rebootReset()); assertNoStaleConsumers();
    assert(!boot().isProvisioned());
  }
  std::cout << "Provisioning transition tests passed (" << operations.size() * 2
            << " power-cut boundaries)\n";
}
