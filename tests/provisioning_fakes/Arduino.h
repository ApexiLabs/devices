#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

class String {
 public:
  String() = default;
  String(const char *value) : value_(value ? value : "") {}
  String(const std::string &value) : value_(value) {}
  bool isEmpty() const { return value_.empty(); }
  size_t length() const { return value_.size(); }
  const char *c_str() const { return value_.c_str(); }
  void clear() { value_.clear(); }
  bool startsWith(const char *prefix) const { return value_.rfind(prefix, 0) == 0; }
  String substring(size_t offset) const { return value_.substr(offset); }
  bool concat(const char *value, size_t size) { value_.append(value, size); return true; }
  bool concat(const char *value) { value_ += value; return true; }
  bool operator==(const String &other) const { return value_ == other.value_; }
  bool operator!=(const String &other) const { return !(*this == other); }
 private:
  std::string value_;
};
struct FakeEsp {
  uint64_t getEfuseMac() const { return 0x123456789abcULL; }
};
inline FakeEsp ESP;
