#include "RuntimeSettings.h"

#include <EEPROM.h>
#include <cstddef>
#include <cstring>

bool RuntimeSettings::begin(const AppConfig::UploadConfig &defaults,
                            const bool defaultUploadEnabled,
                            const bool defaultRemoteManagementEnabled) {
  defaults_ = defaults;
  EEPROM.begin(sizeof(Record));

  Record record{};
  EEPROM.get(0, record);
  if (!valid(record)) {
    Version4Record version4{};
    Version3Record version3{};
    Version2Record version2{};
    LegacyRecord legacy{};
    EEPROM.get(0, version2);
    EEPROM.get(0, version3);
    EEPROM.get(0, version4);
    EEPROM.get(0, legacy);
    populateDefaults(record, defaults, defaultUploadEnabled,
                     defaultRemoteManagementEnabled);
    if (valid(version4)) {
      record.uploadEnabled = version4.uploadEnabled;
      record.remoteManagementEnabled = version4.remoteManagementEnabled;
      record.uploadProtocol = version4.uploadProtocol;
      record.mqttPort = version4.mqttPort;
      record.appliedConfigVersion = version4.appliedConfigVersion;
      strncpy(record.mqttHost, version4.mqttHost, sizeof(record.mqttHost) - 1);
      strncpy(record.ntpPrimary, version4.ntpPrimary, sizeof(record.ntpPrimary) - 1);
      strncpy(record.ntpSecondary, version4.ntpSecondary, sizeof(record.ntpSecondary) - 1);
      strncpy(record.timeZoneRule, version4.timeZoneRule, sizeof(record.timeZoneRule) - 1);
      strncpy(record.timeZoneLabel, version4.timeZoneLabel, sizeof(record.timeZoneLabel) - 1);
    } else if (valid(version3)) {
      record.uploadEnabled = version3.uploadEnabled;
      record.remoteManagementEnabled = version3.remoteManagementEnabled;
      record.appliedConfigVersion = version3.appliedConfigVersion;
      strncpy(record.ntpPrimary, version3.ntpPrimary, sizeof(record.ntpPrimary) - 1);
      strncpy(record.ntpSecondary, version3.ntpSecondary, sizeof(record.ntpSecondary) - 1);
      strncpy(record.timeZoneRule, version3.timeZoneRule, sizeof(record.timeZoneRule) - 1);
      strncpy(record.timeZoneLabel, version3.timeZoneLabel, sizeof(record.timeZoneLabel) - 1);
      if (defaults.protocol == AppConfig::UploadConfig::Protocol::Mqtt) {
        record.mqttPort = version3.mqttPort;
        strncpy(record.mqttHost, version3.mqttHost, sizeof(record.mqttHost) - 1);
      }
    } else if (valid(version2)) {
      record.uploadEnabled = version2.uploadEnabled;
      record.mqttPort = version2.mqttPort;
      strncpy(record.mqttHost, version2.mqttHost, sizeof(record.mqttHost) - 1);
      strncpy(record.ntpPrimary, version2.ntpPrimary, sizeof(record.ntpPrimary) - 1);
      strncpy(record.ntpSecondary, version2.ntpSecondary, sizeof(record.ntpSecondary) - 1);
      strncpy(record.timeZoneRule, version2.timeZoneRule, sizeof(record.timeZoneRule) - 1);
      strncpy(record.timeZoneLabel, version2.timeZoneLabel, sizeof(record.timeZoneLabel) - 1);
    } else if (valid(legacy)) {
      record.uploadEnabled = legacy.uploadEnabled;
      record.mqttPort = legacy.mqttPort;
      strncpy(record.mqttHost, legacy.mqttHost, sizeof(record.mqttHost) - 1);
    }
    record.checksum = checksum(record);
    EEPROM.put(0, record);
    EEPROM.commit();
  }

  apply(record);
  return true;
}

bool RuntimeSettings::save(const String &host,
                           const uint16_t port,
                           const bool enabled,
                           const String &ntpPrimary,
                           const String &ntpSecondary,
                           const String &timeZoneRule,
                           const String &timeZoneLabel,
                           const bool remoteManagementEnabled,
                           const uint32_t appliedConfigVersion,
                           const String &cloudflareAccessClientId,
                           const String &cloudflareAccessClientSecret) {
  if (!validText(host, kHostCapacity) || port == 0 ||
      !validText(ntpPrimary, kNtpCapacity) ||
      !validText(ntpSecondary, kNtpCapacity) ||
      !validText(timeZoneRule, kTimeZoneRuleCapacity) ||
      !validText(timeZoneLabel, kTimeZoneLabelCapacity) ||
      (!cloudflareAccessClientId.isEmpty() &&
       !validText(cloudflareAccessClientId, kCloudflareCredentialCapacity)) ||
      (!cloudflareAccessClientSecret.isEmpty() &&
       !validText(cloudflareAccessClientSecret, kCloudflareCredentialCapacity))) {
    return false;
  }

  Record record{};
  record.magic = kMagic;
  record.version = kVersion;
  record.size = sizeof(Record);
  record.uploadEnabled = enabled ? 1 : 0;
  record.remoteManagementEnabled = remoteManagementEnabled ? 1 : 0;
  record.uploadProtocol = static_cast<uint8_t>(defaults_.protocol);
  record.mqttPort = port;
  record.appliedConfigVersion = appliedConfigVersion;
  host.toCharArray(record.mqttHost, sizeof(record.mqttHost));
  ntpPrimary.toCharArray(record.ntpPrimary, sizeof(record.ntpPrimary));
  ntpSecondary.toCharArray(record.ntpSecondary, sizeof(record.ntpSecondary));
  timeZoneRule.toCharArray(record.timeZoneRule, sizeof(record.timeZoneRule));
  timeZoneLabel.toCharArray(record.timeZoneLabel, sizeof(record.timeZoneLabel));
  const String clientId = cloudflareAccessClientId.isEmpty()
                              ? String(cloudflareAccessClientId_)
                              : cloudflareAccessClientId;
  const String clientSecret = cloudflareAccessClientSecret.isEmpty()
                                  ? String(cloudflareAccessClientSecret_)
                                  : cloudflareAccessClientSecret;
  clientId.toCharArray(record.cloudflareAccessClientId,
                       sizeof(record.cloudflareAccessClientId));
  clientSecret.toCharArray(record.cloudflareAccessClientSecret,
                           sizeof(record.cloudflareAccessClientSecret));
  record.checksum = checksum(record);

  EEPROM.put(0, record);
  if (!EEPROM.commit()) {
    return false;
  }

  apply(record);
  return true;
}

bool RuntimeSettings::applyRemoteConfig(const RemoteConfig &config) {
  if (!remoteManagementEnabled_ || config.version <= appliedConfigVersion_) {
    return false;
  }
  const bool uploadEnabled = config.hasUploadEnabled ? config.uploadEnabled : uploadEnabled_;
  const String primary = config.ntpPrimary.isEmpty() ? String(ntpPrimary_) : config.ntpPrimary;
  const String secondary = config.ntpSecondary.isEmpty() ? String(ntpSecondary_) : config.ntpSecondary;
  const String rule = config.timeZoneRule.isEmpty() ? String(timeZoneRule_) : config.timeZoneRule;
  const String label = config.timeZoneLabel.isEmpty() ? String(timeZoneLabel_) : config.timeZoneLabel;
  return save(String(mqttHost_), uploadConfig_.mqttPort, uploadEnabled, primary, secondary,
              rule, label, true, config.version);
}

bool RuntimeSettings::factoryReset() {
  EEPROM.begin(sizeof(Record));
  for (size_t index = 0; index < sizeof(Record); ++index) {
    EEPROM.write(index, 0xFF);
  }
  return EEPROM.commit();
}

const AppConfig::UploadConfig &RuntimeSettings::uploadConfig() const { return uploadConfig_; }

bool RuntimeSettings::liveUploadEnabled() const { return uploadEnabled_; }

String RuntimeSettings::uploadServerLabel() const {
  if (strlen(mqttHost_) == 0) {
    return "Not configured";
  }
  String label = String(mqttHost_) + ":" + String(uploadConfig_.mqttPort);
  if (uploadConfig_.protocol == AppConfig::UploadConfig::Protocol::Https) {
    label += uploadConfig_.httpsPath;
  }
  return label;
}

const char *RuntimeSettings::ntpPrimary() const { return ntpPrimary_; }

const char *RuntimeSettings::ntpSecondary() const { return ntpSecondary_; }

const char *RuntimeSettings::timeZoneRule() const { return timeZoneRule_; }

const char *RuntimeSettings::timeZoneLabel() const { return timeZoneLabel_; }

bool RuntimeSettings::remoteManagementEnabled() const { return remoteManagementEnabled_; }

uint32_t RuntimeSettings::appliedConfigVersion() const { return appliedConfigVersion_; }

bool RuntimeSettings::cloudflareAccessClientIdConfigured() const {
  return cloudflareAccessClientId_[0] != '\0';
}

bool RuntimeSettings::cloudflareAccessClientSecretConfigured() const {
  return cloudflareAccessClientSecret_[0] != '\0';
}

uint32_t RuntimeSettings::checksum(const Record &record) {
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&record);
  uint32_t value = 2166136261UL;
  for (size_t index = 0; index < offsetof(Record, checksum); ++index) {
    value ^= bytes[index];
    value *= 16777619UL;
  }
  return value;
}

uint32_t RuntimeSettings::checksum(const LegacyRecord &record) {
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&record);
  uint32_t value = 2166136261UL;
  for (size_t index = 0; index < offsetof(LegacyRecord, checksum); ++index) {
    value ^= bytes[index];
    value *= 16777619UL;
  }
  return value;
}

uint32_t RuntimeSettings::checksum(const Version2Record &record) {
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&record);
  uint32_t value = 2166136261UL;
  for (size_t index = 0; index < offsetof(Version2Record, checksum); ++index) {
    value ^= bytes[index];
    value *= 16777619UL;
  }
  return value;
}

uint32_t RuntimeSettings::checksum(const Version4Record &record) {
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&record);
  uint32_t value = 2166136261UL;
  for (size_t index = 0; index < offsetof(Version4Record, checksum); ++index) {
    value ^= bytes[index];
    value *= 16777619UL;
  }
  return value;
}

uint32_t RuntimeSettings::checksum(const Version3Record &record) {
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&record);
  uint32_t value = 2166136261UL;
  for (size_t index = 0; index < offsetof(Version3Record, checksum); ++index) {
    value ^= bytes[index];
    value *= 16777619UL;
  }
  return value;
}

bool RuntimeSettings::valid(const Record &record) {
  return record.magic == kMagic && record.version == kVersion &&
         record.size == sizeof(Record) && record.mqttPort != 0 &&
         record.uploadProtocol <= static_cast<uint8_t>(AppConfig::UploadConfig::Protocol::Https) &&
         record.mqttHost[0] != '\0' &&
         record.mqttHost[kHostCapacity - 1] == '\0' &&
         record.ntpPrimary[0] != '\0' &&
         record.ntpPrimary[kNtpCapacity - 1] == '\0' &&
         record.ntpSecondary[0] != '\0' &&
         record.ntpSecondary[kNtpCapacity - 1] == '\0' &&
         record.timeZoneRule[0] != '\0' &&
         record.timeZoneRule[kTimeZoneRuleCapacity - 1] == '\0' &&
         record.timeZoneLabel[0] != '\0' &&
         record.timeZoneLabel[kTimeZoneLabelCapacity - 1] == '\0' &&
         record.cloudflareAccessClientId[kCloudflareCredentialCapacity - 1] == '\0' &&
         record.cloudflareAccessClientSecret[kCloudflareCredentialCapacity - 1] == '\0' &&
         record.checksum == checksum(record);
}

bool RuntimeSettings::valid(const Version3Record &record) {
  return record.magic == kMagic && record.version == 3 &&
         record.size == sizeof(Version3Record) && record.mqttPort != 0 &&
         record.mqttHost[0] != '\0' && record.mqttHost[kHostCapacity - 1] == '\0' &&
         record.ntpPrimary[0] != '\0' && record.ntpPrimary[kNtpCapacity - 1] == '\0' &&
         record.ntpSecondary[0] != '\0' && record.ntpSecondary[kNtpCapacity - 1] == '\0' &&
         record.timeZoneRule[0] != '\0' &&
         record.timeZoneRule[kTimeZoneRuleCapacity - 1] == '\0' &&
         record.timeZoneLabel[0] != '\0' &&
         record.timeZoneLabel[kTimeZoneLabelCapacity - 1] == '\0' &&
         record.checksum == checksum(record);
}

bool RuntimeSettings::valid(const Version4Record &record) {
  return record.magic == kMagic && record.version == 4 &&
         record.size == sizeof(Version4Record) && record.mqttPort != 0 &&
         record.uploadProtocol <= static_cast<uint8_t>(AppConfig::UploadConfig::Protocol::Https) &&
         record.mqttHost[0] != '\0' && record.mqttHost[kHostCapacity - 1] == '\0' &&
         record.ntpPrimary[0] != '\0' && record.ntpPrimary[kNtpCapacity - 1] == '\0' &&
         record.ntpSecondary[0] != '\0' && record.ntpSecondary[kNtpCapacity - 1] == '\0' &&
         record.timeZoneRule[0] != '\0' &&
         record.timeZoneRule[kTimeZoneRuleCapacity - 1] == '\0' &&
         record.timeZoneLabel[0] != '\0' &&
         record.timeZoneLabel[kTimeZoneLabelCapacity - 1] == '\0' &&
         record.checksum == checksum(record);
}

bool RuntimeSettings::valid(const LegacyRecord &record) {
  return record.magic == kMagic && record.version == 1 &&
         record.size == sizeof(LegacyRecord) && record.mqttPort != 0 &&
         record.mqttHost[0] != '\0' &&
         record.mqttHost[kHostCapacity - 1] == '\0' &&
         record.checksum == checksum(record);
}

bool RuntimeSettings::valid(const Version2Record &record) {
  return record.magic == kMagic && record.version == 2 &&
         record.size == sizeof(Version2Record) && record.mqttPort != 0 &&
         record.mqttHost[0] != '\0' && record.mqttHost[kHostCapacity - 1] == '\0' &&
         record.ntpPrimary[0] != '\0' && record.ntpPrimary[kNtpCapacity - 1] == '\0' &&
         record.ntpSecondary[0] != '\0' && record.ntpSecondary[kNtpCapacity - 1] == '\0' &&
         record.timeZoneRule[0] != '\0' &&
         record.timeZoneRule[kTimeZoneRuleCapacity - 1] == '\0' &&
         record.timeZoneLabel[0] != '\0' &&
         record.timeZoneLabel[kTimeZoneLabelCapacity - 1] == '\0' &&
         record.checksum == checksum(record);
}

bool RuntimeSettings::validText(const String &value, const size_t capacity) {
  if (value.isEmpty() || value.length() >= capacity) {
    return false;
  }
  for (size_t index = 0; index < value.length(); ++index) {
    const uint8_t character = static_cast<uint8_t>(value[index]);
    if (character < 0x20 || character == 0x7F) {
      return false;
    }
  }
  return true;
}

void RuntimeSettings::populateDefaults(Record &record,
                                       const AppConfig::UploadConfig &defaults,
                                       const bool defaultUploadEnabled,
                                       const bool defaultRemoteManagementEnabled) {
  record = {};
  record.magic = kMagic;
  record.version = kVersion;
  record.size = sizeof(Record);
  record.uploadEnabled = defaultUploadEnabled ? 1 : 0;
  record.remoteManagementEnabled = defaultRemoteManagementEnabled ? 1 : 0;
  record.uploadProtocol = static_cast<uint8_t>(defaults.protocol);
  record.mqttPort = defaults.mqttPort;
  record.appliedConfigVersion = 0;
  strncpy(record.mqttHost, defaults.mqttHost, sizeof(record.mqttHost) - 1);
  strncpy(record.ntpPrimary, "pool.ntp.org", sizeof(record.ntpPrimary) - 1);
  strncpy(record.ntpSecondary, "time.google.com", sizeof(record.ntpSecondary) - 1);
  strncpy(record.timeZoneRule, "AWST-8", sizeof(record.timeZoneRule) - 1);
  strncpy(record.timeZoneLabel, "AWST", sizeof(record.timeZoneLabel) - 1);
  strncpy(record.cloudflareAccessClientId, defaults.cloudflareAccessClientId,
          sizeof(record.cloudflareAccessClientId) - 1);
  strncpy(record.cloudflareAccessClientSecret, defaults.cloudflareAccessClientSecret,
          sizeof(record.cloudflareAccessClientSecret) - 1);
}

void RuntimeSettings::apply(const Record &record) {
  memset(mqttHost_, 0, sizeof(mqttHost_));
  memset(ntpPrimary_, 0, sizeof(ntpPrimary_));
  memset(ntpSecondary_, 0, sizeof(ntpSecondary_));
  memset(timeZoneRule_, 0, sizeof(timeZoneRule_));
  memset(timeZoneLabel_, 0, sizeof(timeZoneLabel_));
  memset(cloudflareAccessClientId_, 0, sizeof(cloudflareAccessClientId_));
  memset(cloudflareAccessClientSecret_, 0, sizeof(cloudflareAccessClientSecret_));
  strncpy(mqttHost_, record.mqttHost, sizeof(mqttHost_) - 1);
  strncpy(ntpPrimary_, record.ntpPrimary, sizeof(ntpPrimary_) - 1);
  strncpy(ntpSecondary_, record.ntpSecondary, sizeof(ntpSecondary_) - 1);
  strncpy(timeZoneRule_, record.timeZoneRule, sizeof(timeZoneRule_) - 1);
  strncpy(timeZoneLabel_, record.timeZoneLabel, sizeof(timeZoneLabel_) - 1);
  strncpy(cloudflareAccessClientId_, record.cloudflareAccessClientId,
          sizeof(cloudflareAccessClientId_) - 1);
  strncpy(cloudflareAccessClientSecret_, record.cloudflareAccessClientSecret,
          sizeof(cloudflareAccessClientSecret_) - 1);
  uploadConfig_ = defaults_;
  uploadConfig_.mqttHost = mqttHost_;
  uploadConfig_.mqttPort = record.mqttPort;
  uploadConfig_.cloudflareAccessClientId = cloudflareAccessClientId_;
  uploadConfig_.cloudflareAccessClientSecret = cloudflareAccessClientSecret_;
  uploadEnabled_ = record.uploadEnabled != 0;
  remoteManagementEnabled_ = record.remoteManagementEnabled != 0;
  appliedConfigVersion_ = record.appliedConfigVersion;
}
