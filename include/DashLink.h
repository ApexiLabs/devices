#pragma once

#include <Arduino.h>
#include "AppConfig.h"
#include "Types.h"
#include "DashTelemetry.h"
#include "SystemEvents.h"

#if defined(ESP32)
class BLEClient;
class BLEScanResults;
class BLERemoteCharacteristic;
#endif

class DashLink {
 public:
  void begin(const AppConfig::DashLinkConfig &config);
  void loop(uint32_t nowMs);
  void publish(const std::array<SensorSnapshot, AppConfig::kSensorCount> &sensors, uint32_t nowMs);

  bool isEnabled() const;
  bool isConnected() const;
  const String &status() const;

 private:
  void attemptConnection();
#if defined(ESP32)
  void finishScan();
  static void onScanComplete(BLEScanResults results);
  static void onEvent(BLERemoteCharacteristic *,uint8_t *,size_t,bool);
#endif

  AppConfig::DashLinkConfig config_{false, 0, 0};
  bool initialized_ = false;
  volatile bool connected_ = false;
  volatile bool scanComplete_ = false;
  uint32_t lastAttemptMs_ = 0;
  String status_ = "disabled";

#if defined(ESP32)
  class ClientCallbacks;
  static DashLink *activeInstance_;
  ClientCallbacks *callbacks_ = nullptr;
  BLEClient *client_ = nullptr;
  BLERemoteCharacteristic *telemetry_ = nullptr;
  BLERemoteCharacteristic *events_ = nullptr;
  uint32_t lastEventMs_ = 0;
  portMUX_TYPE eventMux_=portMUX_INITIALIZER_UNLOCKED;
  SystemEvents::Receiver eventReceiver_;
  SystemEvents::Event receivedEvent_{};
  bool eventReady_=false;
  String dashIdentity_;
  uint32_t lastFrameMs_ = 0;
  uint8_t sensorIndex_ = 0;
  uint8_t frameKind_ = DashTelemetry::Id;
#endif
};
