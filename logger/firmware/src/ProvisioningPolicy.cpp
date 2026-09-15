#include "ProvisioningPolicy.h"

#include <cstdio>

namespace ProvisioningPolicy {
namespace {

bool validText(const std::string_view value, const size_t maximum, const bool required) {
  if ((required && value.empty()) || value.size() > maximum) {
    return false;
  }
  for (const unsigned char character : value) {
    if (character < 0x20 || character == 0x7f) {
      return false;
    }
  }
  return true;
}

}  // namespace

std::string deviceIdFromHardwareId(const uint64_t hardwareId) {
  char value[17]{};
  std::snprintf(value, sizeof(value), "mda-%012llx",
                static_cast<unsigned long long>(hardwareId & 0xffffffffffffULL));
  return value;
}

bool isCanonicalDeviceId(const std::string_view deviceId) {
  if (deviceId.size() != 16 || deviceId.substr(0, 4) != "mda-") {
    return false;
  }
  bool hasNonZeroNibble = false;
  for (size_t index = 4; index < deviceId.size(); ++index) {
    const char character = deviceId[index];
    if (!((character >= '0' && character <= '9') ||
          (character >= 'a' && character <= 'f'))) {
      return false;
    }
    hasNonZeroNibble = hasNonZeroNibble || character != '0';
  }
  return hasNonZeroNibble;
}

bool networkAllowed(const bool productionEsp32Target, const bool provisioned) {
  return !productionEsp32Target || provisioned;
}

bool validate(const Candidate &candidate,
              const std::string_view immutableDeviceId,
              std::string &error) {
  if (!isCanonicalDeviceId(immutableDeviceId)) {
    error = "hardware identity is malformed";
    return false;
  }
  if (candidate.expectedDeviceId.empty()) {
    error = "expected_device_id is required";
    return false;
  }
  if (candidate.expectedDeviceId != immutableDeviceId) {
    error = "expected_device_id does not match this device";
    return false;
  }
  if (!validText(candidate.friendlyName, 63, true) ||
      !validText(candidate.wifiSsid, 32, true) ||
      !validText(candidate.wifiPassword, 63, true) ||
      !validText(candidate.otaPassword, 127, true) ||
      !validText(candidate.uploadHost, 127, true) || candidate.uploadPort == 0 ||
      !validText(candidate.hardwareRevision, 31, true) ||
      !validText(candidate.provisionedAt, 40, true)) {
    error = "required provisioning field is blank, malformed, or too long";
    return false;
  }
  if (candidate.otaPassword.size() < 12 || candidate.wifiPassword.size() < 8) {
    error = "Wi-Fi and OTA credentials do not meet minimum lengths";
    return false;
  }
  if (candidate.protocol == Protocol::Mqtt) {
    if (candidate.mqttUsername != immutableDeviceId) {
      error = "MQTT username must equal the immutable device ID";
      return false;
    }
    if (!validText(candidate.mqttPassword, 127, true)) {
      error = "MQTT password is required";
      return false;
    }
  } else {
    if (!validText(candidate.cloudflareClientId, 127, true) ||
        !validText(candidate.cloudflareClientSecret, 127, true) ||
        !validText(candidate.appDeviceToken, 511, true)) {
      error = "HTTPS credentials are incomplete";
      return false;
    }
    if (candidate.appTokenSubject != "logger:" + std::string(immutableDeviceId)) {
      error = "app bearer subject must equal logger:<immutable device ID>";
      return false;
    }
  }
  error.clear();
  return true;
}

}  // namespace ProvisioningPolicy
