#include "LittleFS.h"

#include <algorithm>
#include <stdexcept>

LittleFSClass LittleFS;

File::File(std::shared_ptr<std::vector<uint8_t>> bytes, const uint8_t mode)
    : bytes_(std::move(bytes)), mode_(mode), open_(bytes_ != nullptr) {
  if (mode_ == FILE_APPEND && open_) position_ = bytes_->size();
}

size_t File::size() const { return open_ ? bytes_->size() : 0; }

bool File::seek(const size_t position) {
  if (!open_ || position > bytes_->size()) return false;
  position_ = position;
  return true;
}

size_t File::read(uint8_t *output, const size_t length) {
  if (!open_ || output == nullptr) return 0;
  const size_t availableBytes = bytes_->size() - position_;
  const size_t count = std::min(length, availableBytes);
  std::copy_n(bytes_->data() + position_, count, output);
  position_ += count;
  return count;
}

int File::read() {
  if (!open_ || position_ >= bytes_->size()) return -1;
  return (*bytes_)[position_++];
}

int File::available() const {
  return open_ ? static_cast<int>(bytes_->size() - position_) : 0;
}

size_t File::write(const uint8_t *input, const size_t length) {
  if (!open_ || mode_ == FILE_READ || input == nullptr) return 0;
  if (position_ + length > bytes_->size()) bytes_->resize(position_ + length);
  std::copy_n(input, length, bytes_->data() + position_);
  position_ += length;
  return length;
}

bool LittleFSClass::begin(const bool formatOnFail, const char *, uint8_t, const char *) {
  lastFormatOnFail_ = formatOnFail;
  if (!mountResult_ && formatOnFail) files_.clear();
  return mountResult_;
}

bool LittleFSClass::exists(const char *path) const {
  return files_.find(path) != files_.end();
}

bool LittleFSClass::remove(const char *path) { return files_.erase(path) > 0; }

bool LittleFSClass::rename(const char *source, const char *destination) {
  if (failNextRename_) {
    failNextRename_ = false;
    return false;
  }
  const auto found = files_.find(source);
  if (found == files_.end()) return false;
  files_.erase(destination);
  files_[destination] = found->second;
  files_.erase(found);
  return true;
}

File LittleFSClass::open(const char *path, const uint8_t mode) {
  auto found = files_.find(path);
  if (mode == FILE_READ) {
    return found == files_.end() ? File() : File(found->second, mode);
  }
  if (mode == FILE_WRITE) {
    auto bytes = std::make_shared<std::vector<uint8_t>>();
    files_[path] = bytes;
    return File(bytes, mode);
  }
  if (found == files_.end()) {
    auto bytes = std::make_shared<std::vector<uint8_t>>();
    files_[path] = bytes;
    return File(bytes, mode);
  }
  return File(found->second, mode);
}

void LittleFSClass::reset() {
  files_.clear();
  mountResult_ = true;
  lastFormatOnFail_ = false;
  failNextRename_ = false;
}

const std::vector<uint8_t> &LittleFSClass::bytes(const std::string &path) const {
  const auto found = files_.find(path);
  if (found == files_.end()) throw std::runtime_error("missing fake LittleFS path");
  return *found->second;
}

void LittleFSClass::appendRaw(const std::string &path, const std::vector<uint8_t> &bytesToAppend) {
  auto &target = files_[path];
  if (!target) target = std::make_shared<std::vector<uint8_t>>();
  target->insert(target->end(), bytesToAppend.begin(), bytesToAppend.end());
}

void LittleFSClass::corruptByte(const std::string &path, const size_t offset) {
  auto &target = files_.at(path);
  if (offset >= target->size()) throw std::runtime_error("fake corruption offset out of range");
  (*target)[offset] ^= 0x5a;
}
