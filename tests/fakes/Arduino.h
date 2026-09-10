#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

class String {
 public:
  String() = default;
  String(const char *value) : value_(value == nullptr ? "" : value) {}
  String(const std::string &value) : value_(value) {}

  bool isEmpty() const { return value_.empty(); }
  size_t length() const { return value_.length(); }
  const char *c_str() const { return value_.c_str(); }
  void reserve(size_t capacity) { value_.reserve(capacity); }
  bool concat(const char *value,size_t length) { value_.append(value,length);return true; }
  String &operator+=(char value) {
    value_ += value;
    return *this;
  }
  String &operator=(const char *value) {
    value_ = value == nullptr ? "" : value;
    return *this;
  }
  char operator[](size_t index) const { return value_[index]; }
  bool operator==(const String &other) const { return value_ == other.value_; }
  bool operator!=(const String &other) const { return !(*this == other); }

 private:
  std::string value_;
};

template <typename T>
constexpr T min(const T left, const T right) {
  return left < right ? left : right;
}
inline void yield() {}
