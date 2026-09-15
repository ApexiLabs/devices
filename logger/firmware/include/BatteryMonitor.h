#pragma once
#include <Arduino.h>
#include "BatteryEstimate.h"

class BatteryMonitor {
 public:
  void begin();
  void loop(uint32_t now);
  bool supported() const;
  bool valid() const { return valid_; }
  float voltage() const { return voltage_; }
  int percent() const { return valid_ ? BatteryEstimate::percent(voltage_) : -1; }
  bool externalPower() const { return external_; }
  const char *trend() const { return trend_.value(); }
  const char *state() const;
 private:
  bool sampled_=false, valid_=false, external_=false;
  float voltage_=0;
  uint32_t lastSample_=0;
  BatteryEstimate::Trend trend_;
};
