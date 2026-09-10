#pragma once
#include "AppConfig.h"
#if defined(ESP32)
#include <atomic>
#include <FS.h>
#endif
class RemoteLogs {
 public:
  void begin(const AppConfig::UploadConfig &config);
  void loop(bool enabled);
 private:
#if defined(ESP32)
  AppConfig::UploadConfig config_{};
  String host_,path_,identity_,accessId_,accessSecret_,token_;
  int state_=0;
  String request_,response_;
  bool success_=false;
  bool ready_=false;
  uint32_t lastRequest_=0;
  uint32_t interval_=15000;
  bool failed_=false;
  File file_;
  String job_;
  uint32_t size_=0;
#endif
};
