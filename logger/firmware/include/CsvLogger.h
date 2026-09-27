#pragma once

#include <array>
#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include "AppConfig.h"
#include "Timekeeper.h"
#include "Types.h"

class CsvLogger {
 public:
  void disable();
  bool begin(uint8_t chipSelectPin, SPIClass &spi);
  bool logRow(Timekeeper &timekeeper,
              uint32_t uptimeMs,
              const std::array<SensorSnapshot, AppConfig::kSensorCount> &sensors);
  void flushIfNeeded(uint32_t uptimeMs);
  bool isReady() const;
  String currentFileName() const;
  String lastError() const;
  uint32_t rowsWritten() const { return rowsWritten_; }
  uint32_t lastWriteAgeMs(uint32_t now) const { return rowsWritten_ ? now - lastWriteMs_ : UINT32_MAX; }
  String listFilesJson() const;
 File openReadOnly(const String &userVisibleName) const;

 private:
  bool ensureFileOpen(const String &dateStamp,
                      const std::array<SensorSnapshot, AppConfig::kSensorCount> &sensors);
  String normalizeFileName(const String &userVisibleName) const;
  bool writeHeaderIfNeeded(const std::array<SensorSnapshot, AppConfig::kSensorCount> &sensors);

  bool ready_ = false;
  String currentFileName_;
  String lastError_;
  File file_;
  uint32_t lastFlushMs_ = 0;
  uint16_t rowsSinceFlush_ = 0;
  uint32_t rowsWritten_ = 0, lastWriteMs_ = 0;
  bool incompleteRow_ = false;
};
