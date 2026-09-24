#include "StoreForwardQueue.h"
#include "UploadRecovery.h"
#if defined(ESP32)
#include <LittleFS.h>
#include <cstdio>

namespace {
constexpr uint32_t kRejectedMagic = 0x52454A31;
constexpr size_t kPayloadLimit = 4096;
constexpr char kTemporaryPath[] = "/upload-rejected.tmp";
struct Header { uint32_t magic, length, checksum, httpStatus; };
void slotPath(size_t slot, char *path, size_t capacity) {
  snprintf(path, capacity, "/upload-rejected-%02u.bin", unsigned(slot));
}
bool readFile(const char *path, String &payload, Header &header) {
  payload = "";
  File file = LittleFS.open(path, FILE_READ);
  if (!file || file.read(reinterpret_cast<uint8_t *>(&header), sizeof(header)) != sizeof(header) ||
      header.magic != kRejectedMagic || !header.length || header.length > kPayloadLimit ||
      file.size() != sizeof(header) + header.length) return false;
  payload.reserve(header.length);
  while (payload.length() < header.length) {
    char chunk[256];
    const size_t count = min(sizeof(chunk), size_t(header.length - payload.length()));
    const int received = file.read(reinterpret_cast<uint8_t *>(chunk), count);
    if (received <= 0) return false;
    payload.concat(chunk, received);
  }
  return true;
}
}
#endif

bool StoreForwardQueue::rejectedSlotPresent(size_t slot) const {
#if defined(ESP32)
  if (!ready_ || slot >= kRejectedSlots) return false;
  char path[40]; slotPath(slot, path, sizeof(path));
  return LittleFS.exists(path);
#else
  (void)slot; return false;
#endif
}

bool StoreForwardQueue::readRejected(size_t slot, String &payload, int &httpStatus) const {
#if defined(ESP32)
  if (!ready_ || slot >= kRejectedSlots) return false;
  char path[40]; slotPath(slot, path, sizeof(path));
  Header header{};
  if (!readFile(path, payload, header) ||
      checksum(reinterpret_cast<const uint8_t *>(payload.c_str()), payload.length()) != header.checksum)
    return false;
  httpStatus = int(header.httpStatus);
  return true;
#else
  (void)slot; (void)payload; (void)httpStatus; return false;
#endif
}

bool StoreForwardQueue::preserveRejected(const String &payload, int httpStatus) {
#if defined(ESP32)
  if (!ready_ || payload.isEmpty() || payload.length() > kPayloadLimit ||
      !UploadRecovery::payloadRejected(httpStatus)) {
    lastError_ = "Rejected upload cannot be archived; replay retained";
    return false;
  }
  size_t freeSlot = kRejectedSlots;
  for (size_t slot = 0; slot < kRejectedSlots; ++slot) {
    if (!rejectedSlotPresent(slot)) { if (freeSlot == kRejectedSlots) freeSlot = slot; continue; }
    String saved; int savedStatus = 0;
    // A power loss after archive commit but before queue pop must be idempotent.
    if (readRejected(slot, saved, savedStatus) && saved == payload) { lastError_ = ""; return true; }
  }
  if (freeSlot == kRejectedSlots) {
    lastError_ = "Rejected upload archive full; replay retained; export recovery records";
    return false;
  }
  const Header header{kRejectedMagic, uint32_t(payload.length()),
      checksum(reinterpret_cast<const uint8_t *>(payload.c_str()), payload.length()), uint32_t(httpStatus)};
  File file = LittleFS.open(kTemporaryPath, FILE_WRITE);
  bool written = file && file.write(reinterpret_cast<const uint8_t *>(&header), sizeof(header)) == sizeof(header) &&
      file.write(reinterpret_cast<const uint8_t *>(payload.c_str()), payload.length()) == payload.length();
  file.flush(); file.close();
  Header verified{}; String saved;
  written = written && readFile(kTemporaryPath, saved, verified) && saved == payload &&
      verified.checksum == header.checksum && verified.httpStatus == header.httpStatus;
  char path[40]; slotPath(freeSlot, path, sizeof(path));
  if (!written || !LittleFS.rename(kTemporaryPath, path)) {
    lastError_ = "Rejected upload archive write failed; replay retained";
    return false;
  }
  ++rejectedRecords_;
  lastError_ = "";
  return true;
#else
  (void)payload; (void)httpStatus;
  lastError_ = "Rejected upload archive requires ESP32";
  return false;
#endif
}
