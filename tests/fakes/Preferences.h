#pragma once

#include <map>
#include <string>
#include "Arduino.h"

// Model individually committed NVS keys. Tests can fail or interrupt a write
// without replacing the production persistence implementation.
class Preferences {
 public:
  struct PowerCut {};
  inline static std::map<std::string, std::string> values;
  inline static std::string failKey;
  inline static std::string cutAfterKey;
  inline static bool failOpen = false;
  bool begin(const char *, bool) { return !failOpen; }
  bool isKey(const char *key) const { return values.count(key) != 0; }
  void end() {}
  bool clear() { values.clear(); return true; }
  bool remove(const char *key) { return values.erase(key) > 0; }
  size_t putString(const char *key, const String &value) {
    return put(key, value.c_str());
  }
  size_t putUInt(const char *key, uint32_t value) { return put(key, std::to_string(value)); }
  size_t putUChar(const char *key, uint8_t value) { return putUInt(key, value); }
  size_t putBool(const char *key, bool value) { return putUInt(key, value); }
  String getString(const char *key, const char *fallback) {
    const auto found = values.find(key);
    return found == values.end() ? String(fallback) : String(found->second);
  }
  uint32_t getUInt(const char *key, uint32_t fallback) {
    const auto found = values.find(key);
    return found == values.end() ? fallback : std::stoul(found->second);
  }
  uint8_t getUChar(const char *key, uint8_t fallback) { return getUInt(key, fallback); }
  bool getBool(const char *key, bool fallback) { return getUInt(key, fallback); }

 private:
  size_t put(const char *key, const std::string &value) {
    if (failKey == key) return 0;
    values[key] = value;
    if (cutAfterKey == key) throw PowerCut{};
    return value.size() + 1;
  }
};
