#pragma once

#include <stdint.h>

#include <string>
#include <string_view>

namespace ProvisioningPolicy {

enum class Protocol : uint8_t { Mqtt, Https };

struct Candidate {
  std::string expectedDeviceId;
  std::string friendlyName;
  std::string wifiSsid;
  std::string wifiPassword;
  std::string otaPassword;
  Protocol protocol = Protocol::Https;
  std::string uploadHost;
  uint16_t uploadPort = 0;
  std::string mqttUsername;
  std::string mqttPassword;
  std::string cloudflareClientId;
  std::string cloudflareClientSecret;
  std::string appDeviceToken;
  std::string appTokenSubject;
  std::string hardwareRevision;
  std::string provisionedAt;
};

std::string deviceIdFromHardwareId(uint64_t hardwareId);
bool isCanonicalDeviceId(std::string_view deviceId);
bool networkAllowed(bool productionEsp32Target, bool provisioned);
bool validate(const Candidate &candidate,
              std::string_view immutableDeviceId,
              std::string &error);

}  // namespace ProvisioningPolicy
