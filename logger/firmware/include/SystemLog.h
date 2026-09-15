#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <SD.h>
#include "SystemEvents.h"
#include "Timekeeper.h"

class SystemLog {
 public:
  void begin(Timekeeper &time,const char *identity);
  void add(const char *code,uint32_t value=0,uint8_t severity=1);
  void http(const char *route,uint8_t method,uint16_t status,uint32_t duration);
  bool acceptDash(const SystemEvents::Event &event,const String &device);
  void loop(Timekeeper &time,bool sdReady);
  String recentJson() const;
  String filesJson() const;
  File open(const String &name) const;
  uint32_t dropped() const { return pending_.dropped; }
  void detailedRequests(bool enabled) { detailedStarted_=millis(); detailed_=enabled; }
  bool detailedRequests() const { return detailed_ && uint32_t(millis()-detailedStarted_)<600000; }
 private:
  void remember(const SystemEvents::Event &event,const String &device);
  Timekeeper *time_=nullptr;
  String identity_;
  // Queues are only accessed from the Arduino loop, never BLE callbacks.
  SystemEvents::Queue<64> pending_;
  std::array<String,64> devices_{};
  std::array<String,64> lines_{};
  uint32_t pushed_=0,popped_=0,boot_=0,seq_=0,lastDrain_=0;
  uint32_t dashBoot_=0,dashSeq_=0;
  uint32_t dashPendingBoot_=0,dashPendingSeq_=0;
  String dashDevice_;
  bool ready_=false;
  String error_;
  bool detailed_=false;
  uint32_t detailedStarted_=0;
  String lastHttpCode_;
  uint32_t lastHttpMs_=0,httpRepeated_=0;
  uint16_t lastHttpStatus_=0;
  std::array<String,64> recent_{};
  uint32_t recentCount_=0;
  String active_[2];
  uint32_t segment_[2]{};
};
extern SystemLog systemLog;
