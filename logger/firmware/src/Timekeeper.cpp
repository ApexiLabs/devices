#include "Timekeeper.h"

#include <time.h>

#include "Logic.h"

namespace {

constexpr uint8_t kRv3028TimeReg = 0x00;
constexpr uint8_t kRv3028StatusReg = 0x0E;
constexpr uint8_t kRv3028PorfMask = 0x01;

}  // namespace

void Timekeeper::disable() {
  backend_ = Backend::Disabled;
  wire_ = nullptr;
  address_ = 0;
  ready_ = false;
  networkReady_ = false;
  lastError_ = "RTC disabled";
}

bool Timekeeper::begin(TwoWire &wire, const AppConfig::RtcConfig &config) {
  wire_ = &wire;
  address_ = config.address;

  switch (config.kind) {
    case AppConfig::RtcKind::Ds3231:
      backend_ = Backend::Ds3231;
      return beginDs3231(wire);
    case AppConfig::RtcKind::Rv3028:
      backend_ = Backend::Rv3028;
      return beginRv3028(wire, config.address);
  }

  disable();
  return false;
}

bool Timekeeper::beginDs3231(TwoWire &wire) {
  if (!rtc_.begin(&wire)) {
    ready_ = false;
    lastError_ = "DS3231 not detected";
    return false;
  }

  if (rtc_.lostPower()) {
    ready_ = false;
    lastError_ = "RTC lost power or time invalid";
    return false;
  }

  const DateTime now = rtc_.now();
  if (now.year() < 2024) {
    ready_ = false;
    lastError_ = "RTC time out of range";
    return false;
  }

  ready_ = true;
  lastError_ = "";
  return true;
}

bool Timekeeper::beginRv3028(TwoWire &wire, const uint8_t address) {
  wire.beginTransmission(address);
  if (wire.endTransmission() != 0) {
    ready_ = false;
    lastError_ = "RV-3028 not detected";
    return false;
  }

  uint8_t status = 0;
  if (!readRv3028Register(kRv3028StatusReg, status)) {
    ready_ = false;
    lastError_ = "RV-3028 status read failed";
    return false;
  }

  if ((status & kRv3028PorfMask) != 0) {
    writeRv3028Register(kRv3028StatusReg, static_cast<uint8_t>(status & ~kRv3028PorfMask));
    ready_ = false;
    lastError_ = "RTC lost power or time invalid";
    return false;
  }

  CalendarTime time{};
  if (!readRv3028(time)) {
    ready_ = false;
    lastError_ = "RV-3028 time read failed";
    return false;
  }

  if (time.year < 2024) {
    ready_ = false;
    lastError_ = "RTC time out of range";
    return false;
  }

  ready_ = true;
  lastError_ = "";
  return true;
}

bool Timekeeper::isReady() const { return ready_; }

bool Timekeeper::setFromUnixTime(const uint32_t utcEpoch) {
  const time_t localEpoch = static_cast<time_t>(utcEpoch);
  struct tm localTime {};
  if (localtime_r(&localEpoch, &localTime) == nullptr || localTime.tm_year < 124) {
    ready_ = false;
    lastError_ = "Network time invalid";
    return false;
  }

  CalendarTime time{
      static_cast<uint16_t>(localTime.tm_year + 1900),
      static_cast<uint8_t>(localTime.tm_mon + 1),
      static_cast<uint8_t>(localTime.tm_mday),
      static_cast<uint8_t>(localTime.tm_hour),
      static_cast<uint8_t>(localTime.tm_min),
      static_cast<uint8_t>(localTime.tm_sec),
  };
  networkReady_ = true;

  bool written = false;
  switch (backend_) {
    case Backend::Ds3231:
      rtc_.adjust(DateTime(time.year, time.month, time.day, time.hour, time.minute, time.second));
      written = true;
      break;
    case Backend::Rv3028:
      written = writeRv3028Time(time);
      break;
    case Backend::Disabled:
      break;
  }

  ready_ = written;
  lastError_ = written ? "" : "RTC time write failed";
  return ready_;
}

String Timekeeper::logTimestamp(const uint32_t uptimeMs) {
  CalendarTime now{};
  if ((!ready_ || !readCurrentTime(now)) && !readNetworkTime(now)) {
    return String(Logic::fallbackTimestamp(uptimeMs).c_str());
  }

  return String(Logic::formatTimestamp(now.year,
                                       now.month,
                                       now.day,
                                       now.hour,
                                       now.minute,
                                       now.second)
                    .c_str());
}

String Timekeeper::transportTimestamp(const uint32_t uptimeMs) const {
  if (!networkReady_) {
    return String(Logic::fallbackTimestamp(uptimeMs).c_str());
  }
  const time_t epoch = ::time(nullptr);
  struct tm utcTime {};
  if (gmtime_r(&epoch, &utcTime) == nullptr || utcTime.tm_year < 124) {
    return String(Logic::fallbackTimestamp(uptimeMs).c_str());
  }
  String timestamp = String(Logic::formatTimestamp(static_cast<uint16_t>(utcTime.tm_year + 1900),
                                                    static_cast<uint8_t>(utcTime.tm_mon + 1),
                                                    static_cast<uint8_t>(utcTime.tm_mday),
                                                    static_cast<uint8_t>(utcTime.tm_hour),
                                                    static_cast<uint8_t>(utcTime.tm_min),
                                                    static_cast<uint8_t>(utcTime.tm_sec))
                                .c_str());
  timestamp.setCharAt(10, 'T');
  timestamp += 'Z';
  return timestamp;
}

String Timekeeper::dateStamp() {
  CalendarTime now{};
  if ((!ready_ || !readCurrentTime(now)) && !readNetworkTime(now)) {
    return "boot";
  }

  return String(Logic::formatDateStamp(now.year, now.month, now.day).c_str());
}

String Timekeeper::lastError() const { return lastError_; }

bool Timekeeper::readNetworkTime(CalendarTime &time) const {
  if (!networkReady_) {
    return false;
  }
  const time_t epoch = ::time(nullptr);
  struct tm localTime {};
  if (localtime_r(&epoch, &localTime) == nullptr || localTime.tm_year < 124) {
    return false;
  }
  time.year = static_cast<uint16_t>(localTime.tm_year + 1900);
  time.month = static_cast<uint8_t>(localTime.tm_mon + 1);
  time.day = static_cast<uint8_t>(localTime.tm_mday);
  time.hour = static_cast<uint8_t>(localTime.tm_hour);
  time.minute = static_cast<uint8_t>(localTime.tm_min);
  time.second = static_cast<uint8_t>(localTime.tm_sec);
  return true;
}

bool Timekeeper::readCurrentTime(CalendarTime &time) {
  switch (backend_) {
    case Backend::Ds3231:
      return readDs3231(time);
    case Backend::Rv3028:
      return readRv3028(time);
    case Backend::Disabled:
      return false;
  }

  return false;
}

bool Timekeeper::readDs3231(CalendarTime &time) {
  const DateTime now = rtc_.now();
  time.year = static_cast<uint16_t>(now.year());
  time.month = static_cast<uint8_t>(now.month());
  time.day = static_cast<uint8_t>(now.day());
  time.hour = static_cast<uint8_t>(now.hour());
  time.minute = static_cast<uint8_t>(now.minute());
  time.second = static_cast<uint8_t>(now.second());
  return true;
}

bool Timekeeper::readRv3028(CalendarTime &time) {
  uint8_t buffer[7]{};
  if (!readRv3028Burst(kRv3028TimeReg, buffer, sizeof(buffer))) {
    return false;
  }

  time.second = bcdToDec(buffer[0] & 0x7F);
  time.minute = bcdToDec(buffer[1] & 0x7F);
  time.hour = bcdToDec(buffer[2] & 0x3F);
  time.day = bcdToDec(buffer[4] & 0x3F);
  time.month = bcdToDec(buffer[5] & 0x1F);
  time.year = static_cast<uint16_t>(2000 + bcdToDec(buffer[6]));
  return true;
}

bool Timekeeper::readRv3028Register(const uint8_t reg, uint8_t &value) {
  return readRv3028Burst(reg, &value, 1);
}

bool Timekeeper::writeRv3028Register(const uint8_t reg, const uint8_t value) {
  if (wire_ == nullptr) {
    return false;
  }

  wire_->beginTransmission(address_);
  wire_->write(reg);
  wire_->write(value);
  return wire_->endTransmission() == 0;
}

bool Timekeeper::writeRv3028Time(const CalendarTime &time) {
  if (wire_ == nullptr) {
    return false;
  }

  const uint8_t values[7]{
      decToBcd(time.second),
      decToBcd(time.minute),
      decToBcd(time.hour),
      0x01,
      decToBcd(time.day),
      decToBcd(time.month),
      decToBcd(static_cast<uint8_t>(time.year % 100U)),
  };

  wire_->beginTransmission(address_);
  wire_->write(kRv3028TimeReg);
  wire_->write(values, sizeof(values));
  if (wire_->endTransmission() != 0) {
    return false;
  }

  uint8_t status = 0;
  return readRv3028Register(kRv3028StatusReg, status) &&
         writeRv3028Register(kRv3028StatusReg,
                             static_cast<uint8_t>(status & ~kRv3028PorfMask));
}

bool Timekeeper::readRv3028Burst(const uint8_t startReg, uint8_t *buffer, const size_t length) {
  if (wire_ == nullptr || buffer == nullptr || length == 0) {
    return false;
  }

  wire_->beginTransmission(address_);
  wire_->write(startReg);
  if (wire_->endTransmission(false) != 0) {
    return false;
  }

  const size_t readCount = wire_->requestFrom(static_cast<int>(address_), static_cast<int>(length));
  if (readCount != length) {
    return false;
  }

  for (size_t index = 0; index < length; ++index) {
    buffer[index] = wire_->read();
  }
  return true;
}

uint8_t Timekeeper::bcdToDec(const uint8_t value) {
  return static_cast<uint8_t>(((value >> 4) * 10U) + (value & 0x0F));
}

uint8_t Timekeeper::decToBcd(const uint8_t value) {
  return static_cast<uint8_t>(((value / 10U) << 4) | (value % 10U));
}
