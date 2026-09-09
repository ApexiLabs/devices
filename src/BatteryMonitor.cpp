#include "BatteryMonitor.h"
#include <cstring>

bool BatteryMonitor::supported() const {
#if defined(ARDUINO_TINYC6)
  return true;
#else
  return false;
#endif
}
void BatteryMonitor::begin() {
#if defined(ARDUINO_TINYC6)
  pinMode(VBAT_SENSE, INPUT);
  pinMode(VBUS_SENSE, INPUT);
  analogSetPinAttenuation(VBAT_SENSE, ADC_11db);
  loop(millis());
#endif
}
void BatteryMonitor::loop(uint32_t now) {
#if defined(ARDUINO_TINYC6)
  if(sampled_ && now-lastSample_<1000) return;
  const bool external=digitalRead(VBUS_SENSE)==HIGH;
  uint32_t millivolts=0;
  for(unsigned i=0;i<8;++i) millivolts+=analogReadMilliVolts(VBAT_SENSE);
  // TinyC6 P1 schematic: R6=442k, R7=160k. Use calibrated ADC mV.
  const float raw=millivolts/8000.0f*((442.0f+160.0f)/160.0f);
  const bool valid=BatteryEstimate::percent(raw)>=0;
  voltage_=valid && valid_ && external==external_ ? voltage_+0.2f*(raw-voltage_) : raw;
  valid_=valid; external_=external; sampled_=true; lastSample_=now;
  trend_.update(valid_?voltage_:0, external_, now);
#else
  (void)now;
#endif
}
const char *BatteryMonitor::state() const {
  if(!supported()) return "unsupported";
  if(!valid_) return "unavailable";
  if(!external_) return "discharging (inferred)";
  if(std::strcmp(trend(),"rising")==0) return "likely charging";
  if(std::strcmp(trend(),"falling")==0) return "voltage falling on external power";
  return "external power; charge state unknown";
}
