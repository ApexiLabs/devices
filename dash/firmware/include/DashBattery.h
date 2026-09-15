#pragma once
#include "BatteryEstimate.h"

// Waveshare ESP32-S3-Touch-LCD-1.28: GPIO1, R13=200k/R14=100k.
// No readable VBUS/charger-status signal: voltage cannot identify power source.
namespace DashBattery {
constexpr int kAdcPin=1;
constexpr uint32_t kSampleMs=1000,kStaleMs=5000;
constexpr float kDivider=3.0f;
struct Model {
  bool sampled=false,valid=false;
  uint32_t sampledMs=0;
  float voltage=0;
  BatteryEstimate::Trend trend;
  bool due(uint32_t now)const{return !sampled||uint32_t(now-sampledMs)>=kSampleMs;}
  bool fresh(uint32_t now)const{return sampled&&valid&&uint32_t(now-sampledMs)<kStaleMs;}
  void update(float adcMillivolts,float gain,uint32_t now){
    const float raw=adcMillivolts*0.001f*kDivider*gain;
    const bool next=std::isfinite(gain)&&gain>0&&BatteryEstimate::percent(raw)>=0;
    voltage=next&&fresh(now)?voltage+0.2f*(raw-voltage):raw;
    valid=next;sampled=true;sampledMs=now;
    // Fixed unknown power state; trend is voltage-only, never charging evidence.
    trend.update(valid?voltage:0,false,now);
  }
  int percent(uint32_t now)const{return fresh(now)?BatteryEstimate::percent(voltage):-1;}
};
}
