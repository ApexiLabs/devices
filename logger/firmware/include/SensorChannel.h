#pragma once

#include <Arduino.h>
#include "AppConfig.h"
#include "Types.h"

class SensorChannel {
 public:
  SensorChannel() = default;
  explicit SensorChannel(const AppConfig::SensorConfig &config);
  void configure(const AppConfig::SensorConfig &config);

  void update(float sensedVoltage, bool adcAvailable);
  void clearLatchedFault();
  SensorSnapshot snapshot() const;

 private:
  const AppConfig::SensorConfig *config_ = nullptr;
  float rawVoltage_ = 0.0f;
  float currentmA_ = 0.0f;
  float engineeringValue_ = 0.0f;
  float filteredValue_ = 0.0f;
  float minValue_ = 0.0f;
  float maxValue_ = 0.0f;
  bool hasValidSample_ = false;
  SensorFault activeFault_ = SensorFault::None;
  SensorFault latchedFault_ = SensorFault::None;
};
