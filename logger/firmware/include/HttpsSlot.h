#pragma once
#include <atomic>

// ESP32-C6 cannot reliably allocate multiple concurrent TLS sessions alongside BLE.
// Callers defer/retry if the shared session budget is occupied.
class HttpsSlot {
 public:
  HttpsSlot(){bool expected=false;owned_=busy().compare_exchange_strong(expected,true);}
  ~HttpsSlot(){if(owned_)busy().store(false);}
  explicit operator bool() const{return owned_;}
  HttpsSlot(const HttpsSlot &)=delete;
  HttpsSlot &operator=(const HttpsSlot &)=delete;
 private:
  static std::atomic<bool> &busy(){static std::atomic<bool> value{false};return value;}
  bool owned_=false;
};
