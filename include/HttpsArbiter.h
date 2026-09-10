#pragma once
#include <cstdint>

// Main-task scheduling only. Explicit authorization and infrequent management
// work must get a turn even when telemetry always has another queued record.
class HttpsArbiter {
 public:
  enum class Owner : uint8_t { None=0, Authorization=1, Logs=2, Telemetry=4 };
  bool request(Owner owner,bool idle) {
    const auto bit=static_cast<uint8_t>(owner);
    if(!bit)return false;
    pending_|=bit;
    return idle && (pending_ & static_cast<uint8_t>(0U-pending_))==bit;
  }
  void cancel(Owner owner){pending_ &= ~static_cast<uint8_t>(owner);}
 private:
  uint8_t pending_=0;
};
