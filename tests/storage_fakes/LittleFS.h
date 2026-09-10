#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

constexpr uint8_t FILE_READ = 0;
constexpr uint8_t FILE_WRITE = 1;
constexpr uint8_t FILE_APPEND = 2;

class File {
 public:
  File() = default;
  File(std::shared_ptr<std::vector<uint8_t>> bytes, uint8_t mode);

  explicit operator bool() const { return open_; }
  size_t size() const;
  bool seek(size_t position);
  size_t read(uint8_t *output, size_t length);
  int read();
  int available() const;
  size_t write(const uint8_t *input, size_t length);
  void flush() {}
  void close() { open_ = false; }

 private:
  std::shared_ptr<std::vector<uint8_t>> bytes_;
  size_t position_ = 0;
  uint8_t mode_ = FILE_READ;
  bool open_ = false;
};

class LittleFSClass {
 public:
  bool begin(bool formatOnFail, const char *, uint8_t, const char *);
  bool format(){files_.clear();mountResult_=true;return true;}
  size_t totalBytes() const { return totalBytes_; }
  bool exists(const char *path) const;
  bool remove(const char *path);
  bool rename(const char *source, const char *destination);
  File open(const char *path, uint8_t mode);

  void reset();
  void setMountResult(bool result) { mountResult_ = result; }
  bool lastFormatOnFail() const { return lastFormatOnFail_; }
  const std::vector<uint8_t> &bytes(const std::string &path) const;
  void appendRaw(const std::string &path, const std::vector<uint8_t> &bytes);
  void corruptByte(const std::string &path, size_t offset);
  void failNextRename() { failNextRename_ = true; }

 private:
  std::unordered_map<std::string, std::shared_ptr<std::vector<uint8_t>>> files_;
  size_t totalBytes_ = 1024U * 1024U;
  bool mountResult_ = true;
  bool lastFormatOnFail_ = false;
  bool failNextRename_ = false;
};

extern LittleFSClass LittleFS;
