#pragma once

#include <Arduino.h>

#include "AppConfig.h"
#include "RemoteConfig.h"

class RuntimeSettings {
 public:
  bool begin(const AppConfig::UploadConfig &defaults,
             bool defaultUploadEnabled,
             bool defaultRemoteManagementEnabled = false);
  bool save(const String &host,
            uint16_t port,
            bool enabled,
            const String &ntpPrimary,
            const String &ntpSecondary,
            const String &timeZoneRule,
            const String &timeZoneLabel,
            bool remoteManagementEnabled,
            uint32_t appliedConfigVersion,
            const String &cloudflareAccessClientId = "",
            const String &cloudflareAccessClientSecret = "");
  bool applyRemoteConfig(const RemoteConfig &config);
  bool factoryReset();

  const AppConfig::UploadConfig &uploadConfig() const;
  bool liveUploadEnabled() const;
  String uploadServerLabel() const;
  const char *ntpPrimary() const;
  const char *ntpSecondary() const;
  const char *timeZoneRule() const;
  const char *timeZoneLabel() const;
  bool remoteManagementEnabled() const;
  uint32_t appliedConfigVersion() const;
  bool cloudflareAccessClientIdConfigured() const;
  bool cloudflareAccessClientSecretConfigured() const;

 private:
  static constexpr uint32_t kMagic = 0x4D444131UL;
  static constexpr uint16_t kVersion = 5;
  static constexpr size_t kHostCapacity = 64;
  static constexpr size_t kNtpCapacity = 64;
  static constexpr size_t kTimeZoneRuleCapacity = 64;
  static constexpr size_t kTimeZoneLabelCapacity = 32;
  static constexpr size_t kCloudflareCredentialCapacity = 128;

  struct LegacyRecord {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint8_t uploadEnabled;
    uint8_t reserved;
    uint16_t mqttPort;
    char mqttHost[kHostCapacity];
    uint32_t checksum;
  };

  struct Version2Record {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint8_t uploadEnabled;
    uint8_t reserved;
    uint16_t mqttPort;
    char mqttHost[kHostCapacity];
    char ntpPrimary[kNtpCapacity];
    char ntpSecondary[kNtpCapacity];
    char timeZoneRule[kTimeZoneRuleCapacity];
    char timeZoneLabel[kTimeZoneLabelCapacity];
    uint32_t checksum;
  };

  struct Version4Record {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint8_t uploadEnabled;
    uint8_t remoteManagementEnabled;
    uint8_t uploadProtocol;
    uint8_t reserved;
    uint16_t mqttPort;
    uint32_t appliedConfigVersion;
    char mqttHost[kHostCapacity];
    char ntpPrimary[kNtpCapacity];
    char ntpSecondary[kNtpCapacity];
    char timeZoneRule[kTimeZoneRuleCapacity];
    char timeZoneLabel[kTimeZoneLabelCapacity];
    uint32_t checksum;
  };

  struct Record {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint8_t uploadEnabled;
    uint8_t remoteManagementEnabled;
    uint8_t uploadProtocol;
    uint8_t reserved;
    uint16_t mqttPort;
    uint32_t appliedConfigVersion;
    char mqttHost[kHostCapacity];
    char ntpPrimary[kNtpCapacity];
    char ntpSecondary[kNtpCapacity];
    char timeZoneRule[kTimeZoneRuleCapacity];
    char timeZoneLabel[kTimeZoneLabelCapacity];
    char cloudflareAccessClientId[kCloudflareCredentialCapacity];
    char cloudflareAccessClientSecret[kCloudflareCredentialCapacity];
    uint32_t checksum;
  };

  static uint32_t checksum(const Record &record);
  static uint32_t checksum(const LegacyRecord &record);
  static uint32_t checksum(const Version2Record &record);
  static uint32_t checksum(const Version4Record &record);
  struct Version3Record {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint8_t uploadEnabled;
    uint8_t remoteManagementEnabled;
    uint16_t mqttPort;
    uint32_t appliedConfigVersion;
    char mqttHost[kHostCapacity];
    char ntpPrimary[kNtpCapacity];
    char ntpSecondary[kNtpCapacity];
    char timeZoneRule[kTimeZoneRuleCapacity];
    char timeZoneLabel[kTimeZoneLabelCapacity];
    uint32_t checksum;
  };
  static uint32_t checksum(const Version3Record &record);
  static bool valid(const Record &record);
  static bool valid(const LegacyRecord &record);
  static bool valid(const Version2Record &record);
  static bool valid(const Version3Record &record);
  static bool valid(const Version4Record &record);
  static bool validText(const String &value, size_t capacity);
  void populateDefaults(Record &record,
                        const AppConfig::UploadConfig &defaults,
                        bool defaultUploadEnabled,
                        bool defaultRemoteManagementEnabled);
  void apply(const Record &record);

  AppConfig::UploadConfig defaults_{};
  AppConfig::UploadConfig uploadConfig_{};
  char mqttHost_[kHostCapacity]{};
  char ntpPrimary_[kNtpCapacity]{};
  char ntpSecondary_[kNtpCapacity]{};
  char timeZoneRule_[kTimeZoneRuleCapacity]{};
  char timeZoneLabel_[kTimeZoneLabelCapacity]{};
  char cloudflareAccessClientId_[kCloudflareCredentialCapacity]{};
  char cloudflareAccessClientSecret_[kCloudflareCredentialCapacity]{};
  bool uploadEnabled_ = false;
  bool remoteManagementEnabled_ = false;
  uint32_t appliedConfigVersion_ = 0;
};
