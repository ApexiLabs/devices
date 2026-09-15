#pragma once

#if defined(ESP32)
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "HttpsExchange.h"
#include "HttpsArbiter.h"

class HttpsWorker {
 public:
  using Owner=HttpsArbiter::Owner;
  static HttpsWorker &shared();
  bool begin(const char *rootCertificate);
  bool submit(Owner owner, const char *url, const char *payload, const char *bearer,
              const char *accessId, const char *accessSecret, bool get = false);
  const HttpsExchange::Result *result(Owner owner) const { return owner_==owner?exchange_.result():nullptr; }
  void release(Owner owner) { if(owner_==owner && exchange_.result()){exchange_.release();owner_=Owner::None;} }
  bool idle() const { return exchange_.idle(); }
  void cancelPending(Owner owner){arbiter_.cancel(owner);}
 private:
  static void run(void *context);
  HttpsExchange exchange_;
  TaskHandle_t task_ = nullptr;
  const char *rootCertificate_ = nullptr;
  Owner owner_=Owner::None;
  HttpsArbiter arbiter_;
};
#endif
