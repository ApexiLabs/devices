#pragma once
#include "Arduino.h"
#include <map>
#include <vector>

// Individually durable NVS operations, with namespace isolation. Injection can
// interrupt immediately before or after any mutation; reboot retains values.
class Preferences {
 public:
  struct PowerCut {};
  using Store = std::map<std::string, std::map<std::string, std::string>>;
  inline static Store values;
  inline static std::vector<std::string> mutations;
  inline static int cutAt = -1;
  inline static bool cutAfter = false;
  inline static std::string failMutation;
  inline static std::string failOpen;
  static void resetInjection() {
    mutations.clear(); cutAt = -1; cutAfter = false;
    failMutation.clear(); failOpen.clear();
  }
  bool begin(const char *name, bool readOnly) {
    name_ = name; readOnly_ = readOnly; return failOpen != name_;
  }
  void end() {}
  bool clear() {
    return mutate("*", [&] { values[name_].clear(); });
  }
  size_t putString(const char *key, const String &value) {
    return mutate(key, [&] { values[name_][key] = value.c_str(); }) ? value.length() + 1 : 0;
  }
  size_t putBool(const char *key, bool value) { return putUShort(key, value); }
  size_t putUChar(const char *key, uint8_t value) { return putUShort(key, value); }
  size_t putUShort(const char *key, uint16_t value) {
    return putString(key, String(std::to_string(value)));
  }
  String getString(const char *key, const String &fallback) const {
    const auto space = values.find(name_);
    if (space == values.end()) return fallback;
    const auto found = space->second.find(key);
    return found == space->second.end() ? fallback : String(found->second);
  }
  uint16_t getUShort(const char *key, uint16_t fallback) const {
    const String value = getString(key, "");
    return value.isEmpty() ? fallback : std::stoul(value.c_str());
  }
  uint8_t getUChar(const char *key, uint8_t fallback) const { return getUShort(key, fallback); }
  bool getBool(const char *key, bool fallback) const { return getUShort(key, fallback); }
 private:
  template<class Change> bool mutate(const char *key, Change change) {
    const std::string operation = name_ + "/" + key;
    mutations.push_back(operation);
    const bool interrupted = int(mutations.size()) - 1 == cutAt;
    if (interrupted && !cutAfter) throw PowerCut{};
    if (readOnly_ || failMutation == operation) return false;
    change();
    if (interrupted && cutAfter) throw PowerCut{};
    return true;
  }
  std::string name_;
  bool readOnly_ = true;
};
