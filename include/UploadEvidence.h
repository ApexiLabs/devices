#pragma once
#include <cstdint>

// Snapshot evidence only: heartbeats and local queue writes are not delivery.
namespace UploadEvidence {
enum State : uint8_t { Unknown=0, Disabled=1, Failed=2, ServerAccepted=3, TransportSent=4 };
struct Status {
  uint8_t state=Unknown;
  uint8_t transport=0; // 1 HTTPS, 2 MQTT
  uint32_t ageMs=UINT32_MAX;
  uint32_t intervalMs=0;
};
class Tracker {
 public:
  void record(bool success,uint32_t now) {
    attempted_=true; failed_=!success;
    if(success) { succeeded_=true; successMs_=now; }
  }
  Status status(bool enabled,bool available,bool https,uint32_t now,uint32_t interval) const {
    Status s;
    s.transport=https?1:2; s.intervalMs=interval;
    s.ageMs=succeeded_?uint32_t(now-successMs_):UINT32_MAX;
    s.state=!enabled?Disabled:(!available||failed_)?Failed:
      !attempted_?Unknown:https?ServerAccepted:TransportSent;
    return s;
  }
 private:
  bool attempted_=false,failed_=false,succeeded_=false;
  uint32_t successMs_=0;
};
}
