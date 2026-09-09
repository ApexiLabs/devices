#pragma once

#include <array>
#include <Arduino.h>
#include "AppConfig.h"
#include "SensorTypes.h"

struct SensorSnapshot {
  const char *id;
  const char *name;
  const char *units;
  float rawVoltage;
  float loopCurrentmA;
  float engineeringValue;
  float filteredValue;
  float minValue;
  float maxValue;
  float warnLow;
  float warnHigh;
  float engMin;
  float engMax;
  SensorFault activeFault;
  SensorFault latchedFault;
  bool hasValidSample;
};

struct SystemStatus {
  bool batterySupported;
  bool batteryValid;
  float batteryVoltage;
  int batteryPercent;
  bool externalPower;
  String batteryTrend;
  String batteryState;
  bool adcReady;
  bool displayEnabled;
  bool rtcEnabled;
  bool rtcReady;
  bool rtcSynced;
  String rtcError;
  String rtcLastSync;
  String timeZone;
  bool sdEnabled;
  bool sdReady;
  bool wifiReady;
  bool uploadEnabled;
  bool uploadConnected;
  bool dashEnabled;
  bool dashConnected;
  String dashStatus;
  bool otaEnabled;
  bool otaReady;
  String wifiMode;
  String ipAddress;
  String currentLogFile;
  String lastLogError;
  String uploadProtocol;
  String uploadServer;
  String uploadSessionId;
  String lastUploadError;
  uint32_t lastUploadSequence;
  bool remoteManagementEnabled;
  uint32_t appliedConfigVersion;
  String remoteManagementStatus;
  String remoteManagementError;
  bool storeForwardEnabled;
  bool storeForwardReady;
  uint32_t storeForwardPendingRecords;
  size_t storeForwardPendingBytes;
  size_t storeForwardCapacityBytes;
  uint32_t storeForwardDroppedRecords;
  String storeForwardError;
};

struct AppState {
  std::array<SensorSnapshot, AppConfig::kSensorCount> sensors;
  SystemStatus system;
  uint32_t uptimeMs;
  String uptime;
  String timestamp;
  String transportTimestamp;
};
