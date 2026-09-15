#include "DeviceProvisioning.h"

#include <ArduinoJson.h>

#if defined(ESP32)
#include <Preferences.h>
#include <esp_system.h>
#endif

#include "ProvisioningPolicy.h"

namespace {

constexpr const char *kCommandPrefix = "APEXI_PROVISION ";

String hardwareDeviceId() {
#if defined(ESP8266)
  return String(ProvisioningPolicy::deviceIdFromHardwareId(ESP.getChipId()).c_str());
#else
  return String(ProvisioningPolicy::deviceIdFromHardwareId(ESP.getEfuseMac()).c_str());
#endif
}

}  // namespace

bool DeviceProvisioning::begin(const AppConfig::WifiConfig &wifiDefaults,
                               const AppConfig::OtaConfig &otaDefaults,
                               const AppConfig::UploadConfig &uploadDefaults) {
  wifi_ = wifiDefaults;
  ota_ = otaDefaults;
  upload_ = uploadDefaults;
  deviceId_ = hardwareDeviceId();
  friendlyName_ = deviceId_;
  wifiSsid_ = wifiDefaults.stationSsid;
  wifiPassword_ = wifiDefaults.stationPassword;
  otaPassword_ = otaDefaults.password;
  uploadHost_ = uploadDefaults.mqttHost;
  mqttUsername_ = uploadDefaults.mqttUsername;
  mqttPassword_ = uploadDefaults.mqttPassword;
  cloudflareClientId_ = uploadDefaults.cloudflareAccessClientId;
  cloudflareClientSecret_ = uploadDefaults.cloudflareAccessClientSecret;
  appDeviceToken_ = uploadDefaults.appDeviceToken;

#if defined(ESP32)
  Preferences owner;
  if (!owner.begin("apexi_owner", true)) {
    status_ = "fault";
    lastError_ = "owner credential store unavailable";
    clearOwnerValues();
    bindConfigs();
    return false;
  }
  provisioned_ = owner.getBool("ready", false);
  uint8_t storedProtocol = 0xff;
  if (provisioned_) {
    const String recordedDeviceId = owner.getString("device_id", "");
    if (recordedDeviceId != deviceId_) {
      owner.end();
      status_ = "identity-mismatch";
      lastError_ = "provisioning record belongs to another device";
      clearOwnerValues();
      bindConfigs();
      return false;
    }
    friendlyName_ = owner.getString("name", deviceId_);
    wifiSsid_ = owner.getString("wifi_ssid", "");
    wifiPassword_ = owner.getString("wifi_pass", "");
    otaPassword_ = owner.getString("ota_pass", "");
    uploadHost_ = owner.getString("up_host", "");
    upload_.mqttPort = owner.getUShort("up_port", 0);
    storedProtocol = owner.getUChar("protocol", 0xff);
    upload_.protocol = storedProtocol == 0
                           ? AppConfig::UploadConfig::Protocol::Mqtt
                           : AppConfig::UploadConfig::Protocol::Https;
    mqttUsername_ = owner.getString("mqtt_user", "");
    mqttPassword_ = owner.getString("mqtt_pass", "");
    cloudflareClientId_ = owner.getString("cf_id", "");
    cloudflareClientSecret_ = owner.getString("cf_secret", "");
    appDeviceToken_ = owner.getString("app_token", "");
    appTokenSubject_ = owner.getString("app_subject", "");
    provisionedAt_ = owner.getString("prov_at", "");
    remoteManagementEnabled_ = owner.getBool("remote_mgmt", false);
    status_ = "provisioned";
  } else {
    // Production ESP32 images never inherit owner credentials compiled for a
    // development bench. A blank device can only expose its immutable ID over
    // USB until a complete, identity-bound record is accepted.
    clearOwnerValues();
  }
  owner.end();

  Preferences factory;
  if (factory.begin("apexi_factory", true)) {
    hardwareRevision_ = factory.getString("hw_rev", "unknown");
    factory.end();
  }
  if (provisioned_) {
    ProvisioningPolicy::Candidate stored{};
    stored.expectedDeviceId = deviceId_.c_str();
    stored.friendlyName = friendlyName_.c_str();
    stored.wifiSsid = wifiSsid_.c_str();
    stored.wifiPassword = wifiPassword_.c_str();
    stored.otaPassword = otaPassword_.c_str();
    stored.protocol = upload_.protocol == AppConfig::UploadConfig::Protocol::Mqtt
                          ? ProvisioningPolicy::Protocol::Mqtt
                          : ProvisioningPolicy::Protocol::Https;
    stored.uploadHost = uploadHost_.c_str();
    stored.uploadPort = upload_.mqttPort;
    stored.mqttUsername = mqttUsername_.c_str();
    stored.mqttPassword = mqttPassword_.c_str();
    stored.cloudflareClientId = cloudflareClientId_.c_str();
    stored.cloudflareClientSecret = cloudflareClientSecret_.c_str();
    stored.appDeviceToken = appDeviceToken_.c_str();
    stored.appTokenSubject = appTokenSubject_.c_str();
    stored.hardwareRevision = hardwareRevision_.c_str();
    stored.provisionedAt = provisionedAt_.c_str();
    std::string storedError;
    if (storedProtocol > 1 ||
        !ProvisioningPolicy::validate(stored, deviceId_.c_str(), storedError)) {
      status_ = "fault";
      lastError_ = storedError.empty() ? "stored upload protocol is invalid"
                                       : String(storedError.c_str());
      clearOwnerValues();
      bindConfigs();
      return false;
    }
  }
#else
  // ESP8266 remains a development target. Production provisioning is supported
  // on the ESP32 target whose hardware identity and NVS are used here.
  status_ = "development-defaults";
#endif

  bindConfigs();
  return provisioned_;
}

bool DeviceProvisioning::acceptSerialCommand(const String &line, bool (*prepareOwnerChange)()) {
#if !defined(ESP32)
  lastError_ = "USB provisioning requires the ESP32 production target";
  return false;
#else
  if (!line.startsWith(kCommandPrefix)) {
    lastError_ = "unknown provisioning command";
    return false;
  }

  DynamicJsonDocument document(4096);
  const DeserializationError jsonError =
      deserializeJson(document, line.substring(strlen(kCommandPrefix)));
  if (jsonError) {
    lastError_ = "malformed provisioning JSON";
    return false;
  }

  ProvisioningPolicy::Candidate candidate{};
  candidate.expectedDeviceId = document["expected_device_id"] | "";
  candidate.friendlyName = document["friendly_name"] | "";
  candidate.wifiSsid = document["wifi"]["ssid"] | "";
  candidate.wifiPassword = document["wifi"]["password"] | "";
  candidate.otaPassword = document["ota_password"] | "";
  const String protocol = document["upload"]["protocol"] | "";
  if (protocol != "mqtt" && protocol != "https") {
    lastError_ = "upload protocol must be mqtt or https";
    return false;
  }
  candidate.protocol = protocol == "mqtt" ? ProvisioningPolicy::Protocol::Mqtt
                                           : ProvisioningPolicy::Protocol::Https;
  candidate.uploadHost = document["upload"]["host"] | "";
  candidate.uploadPort = document["upload"]["port"] | 0;
  candidate.mqttUsername = document["upload"]["mqtt_username"] | "";
  candidate.mqttPassword = document["upload"]["mqtt_password"] | "";
  candidate.cloudflareClientId = document["upload"]["cloudflare_client_id"] | "";
  candidate.cloudflareClientSecret = document["upload"]["cloudflare_client_secret"] | "";
  candidate.appDeviceToken = document["upload"]["app_device_token"] | "";
  candidate.appTokenSubject = document["upload"]["app_token_subject"] | "";
  candidate.hardwareRevision = document["hardware_revision"] | "";
  candidate.provisionedAt = document["provisioned_at"] | "";
  if (!document["remote_management_enabled"].isNull() &&
      !document["remote_management_enabled"].is<bool>()) {
    lastError_ = "remote_management_enabled must be boolean";
    return false;
  }
  const bool remoteManagementEnabled = document["remote_management_enabled"] | false;

  std::string validationError;
  if (!ProvisioningPolicy::validate(candidate, deviceId_.c_str(), validationError)) {
    lastError_ = validationError.c_str();
    return false;
  }

  Preferences factory;
  if (!factory.begin("apexi_factory", false)) {
    lastError_ = "factory metadata store unavailable";
    return false;
  }
  const String existingRevision = factory.getString("hw_rev", "");
  if (!existingRevision.isEmpty() && existingRevision != candidate.hardwareRevision.c_str()) {
    factory.end();
    lastError_ = "hardware_revision conflicts with factory record";
    return false;
  }
  // Validate first: malformed commands must never erase an existing owner.
  // Clear old credentials durably before the new owner can be marked ready.
  if (prepareOwnerChange == nullptr || !prepareOwnerChange()) {
    factory.end();
    lastError_ = "owner transition incomplete; networking disabled";
    return false;
  }
  if (existingRevision.isEmpty() &&
      factory.putString("hw_rev", candidate.hardwareRevision.c_str()) == 0) {
    factory.end();
    lastError_ = "failed to persist factory hardware revision";
    return false;
  }
  factory.end();

  Preferences owner;
  if (!owner.begin("apexi_owner", false)) {
    lastError_ = "owner credential store unavailable";
    return false;
  }
  bool saved = owner.clear();
  saved = saved && owner.putString("device_id", candidate.expectedDeviceId.c_str()) > 0;
  saved = saved && owner.putString("name", candidate.friendlyName.c_str()) > 0;
  saved = saved && owner.putString("wifi_ssid", candidate.wifiSsid.c_str()) > 0;
  saved = saved && owner.putString("wifi_pass", candidate.wifiPassword.c_str()) > 0;
  saved = saved && owner.putString("ota_pass", candidate.otaPassword.c_str()) > 0;
  saved = saved && owner.putString("up_host", candidate.uploadHost.c_str()) > 0;
  saved = saved && owner.putUShort("up_port", candidate.uploadPort) > 0;
  saved = saved && owner.putUChar("protocol", candidate.protocol == ProvisioningPolicy::Protocol::Mqtt ? 0 : 1) > 0;
  if (candidate.protocol == ProvisioningPolicy::Protocol::Mqtt) {
    saved = saved && owner.putString("mqtt_user", candidate.mqttUsername.c_str()) > 0;
    saved = saved && owner.putString("mqtt_pass", candidate.mqttPassword.c_str()) > 0;
  } else {
    saved = saved && owner.putString("cf_id", candidate.cloudflareClientId.c_str()) > 0;
    saved = saved && owner.putString("cf_secret", candidate.cloudflareClientSecret.c_str()) > 0;
    saved = saved && owner.putString("app_token", candidate.appDeviceToken.c_str()) > 0;
    saved = saved && owner.putString("app_subject", candidate.appTokenSubject.c_str()) > 0;
  }
  saved = saved && owner.putString("prov_at", candidate.provisionedAt.c_str()) > 0;
  saved = saved && owner.putBool("remote_mgmt", remoteManagementEnabled) > 0;
  saved = saved && owner.putBool("ready", true) > 0;
  owner.end();

  if (!saved) {
    Preferences rollback;
    if (rollback.begin("apexi_owner", false)) {
      rollback.clear();
      rollback.end();
    }
    if (lastError_.isEmpty()) {
      lastError_ = "failed to persist complete provisioning record";
    }
    return false;
  }
  lastError_.clear();
  return true;
#endif
}

bool DeviceProvisioning::factoryResetOwnerCredentials() {
#if !defined(ESP32)
  lastError_ = "factory reset requires the ESP32 production target";
  return false;
#else
  Preferences owner;
  if (!owner.begin("apexi_owner", false)) {
    lastError_ = "owner credential store unavailable";
    return false;
  }
  const bool cleared = owner.clear();
  owner.end();
  if (!cleared) {
    lastError_ = "failed to clear owner credentials";
  }
  return cleared;
#endif
}

const AppConfig::WifiConfig &DeviceProvisioning::wifiConfig() const { return wifi_; }
const AppConfig::OtaConfig &DeviceProvisioning::otaConfig() const { return ota_; }
const AppConfig::UploadConfig &DeviceProvisioning::uploadConfig() const { return upload_; }
const char *DeviceProvisioning::deviceId() const { return deviceId_.c_str(); }
const char *DeviceProvisioning::friendlyName() const { return friendlyName_.c_str(); }
const char *DeviceProvisioning::hardwareRevision() const { return hardwareRevision_.c_str(); }
const char *DeviceProvisioning::provisionedAt() const { return provisionedAt_.c_str(); }
const char *DeviceProvisioning::status() const { return status_.c_str(); }
const char *DeviceProvisioning::lastError() const { return lastError_.c_str(); }
bool DeviceProvisioning::isProvisioned() const { return provisioned_; }
bool DeviceProvisioning::remoteManagementEnabled() const { return remoteManagementEnabled_; }

void DeviceProvisioning::bindConfigs() {
  wifi_.stationSsid = wifiSsid_.c_str();
  wifi_.stationPassword = wifiPassword_.c_str();
  ota_.hostname = deviceId_.c_str();
  ota_.password = otaPassword_.c_str();
  upload_.deviceId = deviceId_.c_str();
  upload_.mqttHost = uploadHost_.c_str();
  upload_.mqttUsername = mqttUsername_.c_str();
  upload_.mqttPassword = mqttPassword_.c_str();
  upload_.cloudflareAccessClientId = cloudflareClientId_.c_str();
  upload_.cloudflareAccessClientSecret = cloudflareClientSecret_.c_str();
  upload_.appDeviceToken = appDeviceToken_.c_str();
}

void DeviceProvisioning::clearOwnerValues() {
  provisioned_ = false;
  friendlyName_ = deviceId_;
  wifiSsid_.clear();
  wifiPassword_.clear();
  otaPassword_.clear();
  uploadHost_.clear();
  upload_.mqttPort = 0;
  mqttUsername_.clear();
  mqttPassword_.clear();
  cloudflareClientId_.clear();
  cloudflareClientSecret_.clear();
  appDeviceToken_.clear();
  appTokenSubject_.clear();
  provisionedAt_.clear();
  remoteManagementEnabled_ = false;
}
