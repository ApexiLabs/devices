#pragma once

#include <Arduino.h>
#include <RTClib.h>
#include <Wire.h>

#include "AppConfig.h"

class Timekeeper {
 public:
  void disable();
  bool begin(TwoWire &wire, const AppConfig::RtcConfig &config);
  bool setFromUnixTime(uint32_t utcEpoch);
  bool isReady() const;
  String logTimestamp(uint32_t uptimeMs);
  String transportTimestamp(uint32_t uptimeMs) const;
  String dateStamp();
  String lastError() const;

 private:
  struct CalendarTime {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
  };

  enum class Backend : uint8_t {
    Disabled,
    Ds3231,
    Rv3028,
  };

  bool beginDs3231(TwoWire &wire);
  bool beginRv3028(TwoWire &wire, uint8_t address);
  bool readCurrentTime(CalendarTime &time);
  bool readDs3231(CalendarTime &time);
  bool readRv3028(CalendarTime &time);
  bool readRv3028Register(uint8_t reg, uint8_t &value);
  bool writeRv3028Register(uint8_t reg, uint8_t value);
  bool writeRv3028Time(const CalendarTime &time);
  bool readNetworkTime(CalendarTime &time) const;
  bool readRv3028Burst(uint8_t startReg, uint8_t *buffer, size_t length);
  static uint8_t bcdToDec(uint8_t value);
  static uint8_t decToBcd(uint8_t value);

  RTC_DS3231 rtc_;
  TwoWire *wire_ = nullptr;
  Backend backend_ = Backend::Disabled;
  uint8_t address_ = 0;
  bool ready_ = false;
  bool networkReady_ = false;
  String lastError_;
};
