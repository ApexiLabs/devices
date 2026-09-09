#pragma once
#include <cmath>
#include <cstdint>

namespace BatteryEstimate {
// Approximate resting-voltage curve for a conventional 1S 4.2V LiPo.
inline int percent(float v) {
  if (!std::isfinite(v) || v < 2.5f || v > 4.35f) return -1;
  constexpr float volts[] = {3.30f,3.50f,3.65f,3.70f,3.75f,3.80f,3.85f,3.95f,4.05f,4.15f,4.20f};
  constexpr int pct[] = {0,5,10,20,30,40,50,65,80,95,100};
  if(v <= volts[0]) return 0;
  for(unsigned i=1;i<11;++i) if(v < volts[i])
    return static_cast<int>(std::lround(pct[i-1]+(v-volts[i-1])/(volts[i]-volts[i-1])*(pct[i]-pct[i-1])));
  return 100;
}
class Trend {
 public:
  void update(float volts, bool external, uint32_t now) {
    if(percent(volts)<0) { initialized_=false; trend_="unavailable"; return; }
    if(!initialized_ || external!=external_ || now-lastSample_>15000) {
      initialized_=true; external_=external; baseline_=volts; since_=now; trend_="collecting";
    } else if(now-since_>=120000) {
      const float delta=volts-baseline_;
      trend_=delta>0.03f?"rising":(delta< -0.03f?"falling":"steady");
      baseline_=volts; since_=now;
    }
    lastSample_=now;
  }
  const char *value() const { return trend_; }
 private:
  bool initialized_=false, external_=false;
  float baseline_=0;
  uint32_t since_=0,lastSample_=0;
  const char *trend_="collecting";
};
}
