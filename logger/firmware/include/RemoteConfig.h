#pragma once

#include <Arduino.h>

struct RemoteConfig {
  uint32_t version = 0;
  bool hasUploadEnabled = false;
  bool uploadEnabled = false;
  String ntpPrimary;
  String ntpSecondary;
  String timeZoneRule;
  String timeZoneLabel;
};
