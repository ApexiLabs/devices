#include "CsvLogger.h"

#include "Logic.h"

void CsvLogger::disable() {
  ready_ = false;
  currentFileName_ = "";
  lastError_ = "microSD logging disabled";
  if (file_) {
    file_.flush();
    file_.close();
  }
}

bool CsvLogger::begin(const uint8_t chipSelectPin, SPIClass &spi) {
#if defined(ESP8266)
  (void)spi;
  ready_ = SD.begin(chipSelectPin, SPI_HALF_SPEED);
#else
  ready_ = SD.begin(chipSelectPin, spi, 25000000);
#endif
  if (!ready_) {
    lastError_ = "microSD mount failed";
    return false;
  }
  lastError_ = "";
  return true;
}

bool CsvLogger::logRow(Timekeeper &timekeeper,
                       const uint32_t uptimeMs,
                       const std::array<SensorSnapshot, AppConfig::kSensorCount> &sensors) {
  if (!ready_) {
    if (lastError_.isEmpty()) {
      lastError_ = "microSD logging disabled";
    }
    return false;
  }

  if (!ensureFileOpen(timekeeper.dateStamp(), sensors)) {
    return false;
  }

  String row = timekeeper.logTimestamp(uptimeMs) + "," + String(uptimeMs);
  for (const SensorSnapshot &sensor : sensors) {
    row += "," + String(sensor.filteredValue, 3);
    row += "," + String(sensor.loopCurrentmA, 3);
    row += "," + String(sensorFaultToString(sensor.activeFault));
  }
  row += "\n";

  if (file_.print(row) == 0) {
    lastError_ = "CSV write failed";
    return false;
  }

  rowsSinceFlush_++;
  lastError_ = "";
  return true;
}

void CsvLogger::flushIfNeeded(const uint32_t uptimeMs) {
  if (!ready_ || !file_) {
    return;
  }

  if ((uptimeMs - lastFlushMs_) >= AppConfig::kTiming.loggerFlushIntervalMs ||
      rowsSinceFlush_ >= AppConfig::kTiming.loggerFlushRows) {
    file_.flush();
    lastFlushMs_ = uptimeMs;
    rowsSinceFlush_ = 0;
  }
}

bool CsvLogger::isReady() const { return ready_; }

String CsvLogger::currentFileName() const { return currentFileName_; }

String CsvLogger::lastError() const { return lastError_; }

String CsvLogger::listFilesJson() const {
  if (!ready_) {
    return "[]";
  }

  File root = SD.open("/");
  if (!root) {
    return "[]";
  }

  String json = "[";
  bool first = true;
  File entry = root.openNextFile();
  while (entry) {
    if (!entry.isDirectory()) {
      const String name = entry.name();
      if (name.endsWith(".csv")) {
        if (!first) {
          json += ",";
        }
        json += "{\"name\":\"" + name + "\",\"size\":" + String(entry.size()) + "}";
        first = false;
      }
    }
    entry.close();
    entry = root.openNextFile();
  }

  json += "]";
  return json;
}

File CsvLogger::openReadOnly(const String &userVisibleName) const {
  if (!ready_) {
    return File();
  }
  const String normalized = normalizeFileName(userVisibleName);
  if (normalized.isEmpty()) {
    return File();
  }
  return SD.open(normalized, FILE_READ);
}

bool CsvLogger::ensureFileOpen(
    const String &dateStamp,
    const std::array<SensorSnapshot, AppConfig::kSensorCount> &sensors) {
  const String desiredName = "/logs-" + dateStamp + ".csv";
  if (file_ && currentFileName_ == desiredName) {
    return true;
  }

  if (file_) {
    file_.flush();
    file_.close();
  }

  const bool newFile = !SD.exists(desiredName);
  file_ = SD.open(desiredName.c_str(), "a");
  if (!file_) {
    lastError_ = "Open log file failed";
    return false;
  }

  currentFileName_ = desiredName;
  if (newFile || file_.size() == 0) {
    writeHeaderIfNeeded(sensors);
  }
  return true;
}

String CsvLogger::normalizeFileName(const String &userVisibleName) const {
  return String(Logic::normalizeLogFileName(userVisibleName.c_str()).c_str());
}

void CsvLogger::writeHeaderIfNeeded(
    const std::array<SensorSnapshot, AppConfig::kSensorCount> &sensors) {
  if (!file_) {
    return;
  }

  String header = "timestamp,uptime_ms";
  for (const SensorSnapshot &sensor : sensors) {
    header += "," + String(sensor.id) + "_value";
    header += "," + String(sensor.id) + "_mA";
    header += "," + String(sensor.id) + "_fault";
  }
  file_.println(header);
  file_.flush();
  lastFlushMs_ = 0;
  rowsSinceFlush_ = 0;
}
