#pragma once

#include <Arduino.h>

#include "AppConfig.h"

class DeviceProvisioning {
 public:
  bool begin(const AppConfig::WifiConfig &wifiDefaults,
             const AppConfig::OtaConfig &otaDefaults,
             const AppConfig::UploadConfig &uploadDefaults);
  bool acceptSerialCommand(const String &line, bool (*prepareOwnerChange)());
  bool factoryResetOwnerCredentials();

  const AppConfig::WifiConfig &wifiConfig() const;
  const AppConfig::OtaConfig &otaConfig() const;
  const AppConfig::UploadConfig &uploadConfig() const;
  const char *deviceId() const;
  const char *friendlyName() const;
  const char *hardwareRevision() const;
  const char *provisionedAt() const;
  const char *status() const;
  const char *lastError() const;
  bool isProvisioned() const;
  bool remoteManagementEnabled() const;

 private:
  void bindConfigs();
  void clearOwnerValues();

  AppConfig::WifiConfig wifi_{};
  AppConfig::OtaConfig ota_{};
  AppConfig::UploadConfig upload_{};
  String deviceId_;
  String friendlyName_;
  String wifiSsid_;
  String wifiPassword_;
  String otaPassword_;
  String uploadHost_;
  String mqttUsername_;
  String mqttPassword_;
  String cloudflareClientId_;
  String cloudflareClientSecret_;
  String appDeviceToken_;
  String appTokenSubject_;
  String hardwareRevision_;
  String provisionedAt_;
  String status_ = "unprovisioned";
  String lastError_;
  bool provisioned_ = false;
  bool remoteManagementEnabled_ = false;
};
