#pragma once
#include <array>
#include <cmath>
#include <cstring>
#include <stdint.h>
#include "SensorTypes.h"

// Version 1: exactly 20 bytes, so no negotiated MTU or fragmentation is needed.
namespace DashTelemetry {
inline constexpr char kUuid[] = "8f771002-6d7a-4f48-9f8a-67a8c14b6c01";
inline constexpr size_t kMaxSensors = 8;
inline constexpr uint32_t kPublishMs = 250;
inline constexpr uint32_t kStaleMs = 3000;
using Frame = std::array<uint8_t, 20>;
enum Kind : uint8_t { Id = 1, Name = 2, Units = 3, Sample = 4 };
inline Frame text(Kind kind, uint8_t index, uint8_t count, const char *value) {
  Frame f{}; f[0] = 1; f[1] = kind; f[2] = index; f[3] = count;
  std::strncpy(reinterpret_cast<char *>(f.data() + 4), value, 15);
  return f;
}
inline Frame sample(uint8_t index, uint8_t count, float value, bool valid,
                    SensorFault fault, bool warning) {
  static_assert(sizeof(float) == 4, "Protocol requires 32-bit float");
  Frame f{}; f[0] = 1; f[1] = Sample; f[2] = index; f[3] = count;
  f[4] = valid && std::isfinite(value); f[5] = static_cast<uint8_t>(fault); f[6] = warning;
  uint32_t bits; std::memcpy(&bits, &value, 4);
  for (int i = 0; i < 4; ++i) f[8+i] = (bits >> (8*i)) & 255;
  return f;
}
struct Sensor {
  char id[16]{}, name[16]{}, units[16]{};
  uint8_t metadata = 0;
  float value = 0;
  bool valid = false, warning = false, received = false;
  SensorFault fault = SensorFault::None;
  uint32_t receivedMs = 0;
  bool hasLastGood = false;
  float lastGoodValue = 0;
  uint32_t lastGoodMs = 0;
  bool fresh(bool connected, uint32_t now) const {
    return connected && received && metadata == 7 && uint32_t(now-receivedMs) < kStaleMs;
  }
};
struct Model {
  std::array<Sensor,kMaxSensors> sensors{};
  uint8_t count = 0;
  void clear() { *this = Model{}; }
  bool accept(const Frame &f, uint32_t now) {
    if (f[0] != 1 || f[1] < Id || f[1] > Sample || f[3] == 0 ||
        f[3] > kMaxSensors || f[2] >= f[3]) return false;
    if (f[1] != Sample) {
      if (f[19] != 0 || (f[1] == Id && f[4] == 0)) return false;
      for (int i=4; i<19 && f[i]; ++i)
        if (f[i] < 32 || f[i] > 126) return false;
    } else if (f[4] > 1 || f[5] > static_cast<uint8_t>(SensorFault::AdcUnavailable) || f[6] > 1) return false;
    if (count != f[3]) { clear(); count = f[3]; }
    Sensor &s = sensors[f[2]];
    if (f[1] != Sample) {
      const char *v = reinterpret_cast<const char *>(f.data()+4);
      if (f[1] == Id && std::strcmp(s.id, v)) s = Sensor{};
      char *dest = f[1] == Id ? s.id : f[1] == Name ? s.name : s.units;
      std::memcpy(dest,v,16); s.metadata |= 1 << (f[1]-1);
    } else {
      uint32_t bits=0; for (int i=0;i<4;++i) bits |= uint32_t(f[8+i]) << (8*i);
      std::memcpy(&s.value,&bits,4);
      s.valid = f[4] && std::isfinite(s.value);
      s.fault = static_cast<SensorFault>(f[5]); s.warning = f[6];
      s.received = true; s.receivedMs = now;
      if(s.valid && s.fault==SensorFault::None){s.hasLastGood=true;s.lastGoodValue=s.value;s.lastGoodMs=now;}
    }
    return true;
  }
};
inline bool validRefresh(uint32_t ms) { return ms >= 250 && ms <= 5000 && ms % 250 == 0; }
}  // namespace DashTelemetry
