#include "StoreForwardQueue.h"
#include "BlankPartition.h"

#if defined(ESP32)
#include <LittleFS.h>
#include <esp_partition.h>

#include <cstddef>

namespace {

constexpr uint32_t kMetadataMagic = 0x5346514DUL;
constexpr uint16_t kMetadataVersion = 1;
constexpr uint32_t kRecordMagic = 0x53465131UL;
constexpr size_t kMaximumPayloadBytes = 4096;
constexpr size_t kFilesystemReserveBytes = 512UL * 1024UL;
constexpr char kMetadataPath[] = "/sfq.meta";
constexpr char kMetadataTemporaryPath[] = "/sfq.meta.tmp";
constexpr char kSegmentRepairPath[] = "/sfq.repair.tmp";
constexpr char kSegmentZeroPath[] = "/sfq-0.log";
constexpr char kSegmentOnePath[] = "/sfq-1.log";

}  // namespace
#endif

bool StoreForwardQueue::begin(const bool enabled, const size_t maximumBytes) {
  enabled_ = enabled;
  ready_ = false;
  lastError_ = "";
  if (!enabled_) {
    lastError_ = "Onboard store-and-forward disabled";
    return true;
  }

#if !defined(ESP32)
  (void)maximumBytes;
  lastError_ = "Onboard store-and-forward requires ESP32";
  return false;
#else
  // TinyC6's unchanged default partition table labels its filesystem spiffs.
  // Prefer the explicit logger partition; never touch arbitrary data partitions.
  const esp_partition_t *partition=esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,"littlefs");
  if(!partition) partition=esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,"spiffs");
  if(!partition) { lastError_="Filesystem partition missing"; return false; }
  if (!LittleFS.begin(false, "/littlefs", 10, partition->label)) {
    // Only initialise a wholly erased partition. A corrupt/foreign filesystem
    // may contain recoverable data and must never be autoformatted.
    if(!BlankPartition::verify(partition->size,[&](size_t offset,uint8_t *bytes,size_t length) {
      const bool ok=esp_partition_read(partition,offset,bytes,length)==ESP_OK;
      yield(); return ok;
    })) { lastError_="LittleFS mount failed; nonblank or unreadable data preserved"; return false; }
    if(!LittleFS.format() || !LittleFS.begin(false,"/littlefs",10,partition->label)) {
      lastError_="Blank LittleFS initialization failed"; return false;
    }
  }
  const size_t totalBytes = LittleFS.totalBytes();
  if (totalBytes <= kFilesystemReserveBytes + 2 * sizeof(RecordHeader)) {
    lastError_ = "LittleFS partition too small";
    return false;
  }
  const size_t usableBytes = min(maximumBytes, totalBytes - kFilesystemReserveBytes);
  segmentMaximumBytes_ = usableBytes / 2U;
  if (segmentMaximumBytes_ <= sizeof(RecordHeader) + 1U) {
    lastError_ = "Store-and-forward capacity too small";
    return false;
  }

  if (LittleFS.exists(kMetadataTemporaryPath)) LittleFS.remove(kMetadataTemporaryPath);
  if (LittleFS.exists(kSegmentRepairPath)) LittleFS.remove(kSegmentRepairPath);
  if (!loadMetadata()) {
    metadata_ = {};
    metadata_.magic = kMetadataMagic;
    metadata_.version = kMetadataVersion;
    metadata_.activeSegment = 0;
  }
  for (uint8_t segment = 0; segment < 2; ++segment) {
    if (!scanSegment(segment)) {
      return false;
    }
  }
  if (!saveMetadata()) {
    return false;
  }
  ready_ = true;
  return true;
#endif
}

bool StoreForwardQueue::enqueue(const String &payload) {
#if !defined(ESP32)
  (void)payload;
  return false;
#else
  if (!ready_ || payload.isEmpty() || payload.length() > kMaximumPayloadBytes) {
    if(ready_) lastError_="Queued payload size invalid";
    else if(lastError_.isEmpty()) lastError_="Store-and-forward unavailable";
    return false;
  }
  const size_t recordBytes = sizeof(RecordHeader) + payload.length();
  if (tail_[metadata_.activeSegment] + recordBytes > segmentMaximumBytes_ && !rotateSegment()) {
    return false;
  }

  const uint8_t segment = metadata_.activeSegment;
  File file = LittleFS.open(segmentPath(segment), FILE_APPEND);
  if (!file) {
    lastError_ = "Queue segment open failed";
    return false;
  }
  const RecordHeader header{kRecordMagic,
                            static_cast<uint32_t>(payload.length()),
                            checksum(reinterpret_cast<const uint8_t *>(payload.c_str()),
                                     payload.length())};
  const bool written = file.write(reinterpret_cast<const uint8_t *>(&header), sizeof(header)) ==
                           sizeof(header) &&
                       file.write(reinterpret_cast<const uint8_t *>(payload.c_str()),
                                  payload.length()) == payload.length();
  file.flush();
  file.close();
  if (!written) {
    lastError_ = "Queue append failed";
    return false;
  }
  tail_[segment] += recordBytes;
  ++count_[segment];
  lastError_ = "";
  return true;
#endif
}

bool StoreForwardQueue::peek(String &payload) {
#if !defined(ESP32)
  (void)payload;
  return false;
#else
  payload = "";
  if (!ready_ || pendingRecords() == 0) {
    return false;
  }
  const uint8_t segment = oldestSegment();
  File file = LittleFS.open(segmentPath(segment), FILE_READ);
  if (!file || !file.seek(metadata_.head[segment])) {
    lastError_ = "Queue read seek failed";
    return false;
  }
  RecordHeader header{};
  if (file.read(reinterpret_cast<uint8_t *>(&header), sizeof(header)) != sizeof(header) ||
      header.magic != kRecordMagic || header.length == 0 || header.length > kMaximumPayloadBytes) {
    lastError_ = "Queue record header invalid";
    return false;
  }
  payload.reserve(header.length);
  while (payload.length() < header.length && file.available()) {
    char chunk[256];
    const size_t wanted=min(size_t(sizeof(chunk)),size_t(header.length-payload.length()));
    const int received=file.read(reinterpret_cast<uint8_t *>(chunk),wanted);
    if(received<=0)break;
    payload.concat(chunk,static_cast<unsigned int>(received));
  }
  file.close();
  if (payload.length() != header.length ||
      checksum(reinterpret_cast<const uint8_t *>(payload.c_str()), payload.length()) !=
          header.checksum) {
    payload = "";
    lastError_ = "Queue record checksum invalid";
    return false;
  }
  lastError_ = "";
  return true;
#endif
}

bool StoreForwardQueue::peekBatch(std::vector<String> &payloads,size_t maximumRecords,size_t maximumBytes) {
  payloads.clear();
#if !defined(ESP32)
  (void)maximumRecords;(void)maximumBytes;return false;
#else
  if(!ready_ || !pendingRecords() || !maximumRecords)return false;
  maximumRecords=min(maximumRecords,size_t(8));payloads.reserve(maximumRecords);
  const uint8_t first=oldestSegment();size_t bytes=0;
  for(uint8_t pass=0;pass<2 && payloads.size()<maximumRecords;++pass) {
    const uint8_t segment=pass?uint8_t(1-first):first;
    if(!count_[segment])continue;
    File file=LittleFS.open(segmentPath(segment),FILE_READ);
    if(!file || !file.seek(metadata_.head[segment])){lastError_="Queue batch seek failed";break;}
    for(uint32_t index=0;index<count_[segment] && payloads.size()<maximumRecords;++index) {
      RecordHeader header{};
      if(file.read(reinterpret_cast<uint8_t *>(&header),sizeof(header))!=sizeof(header) || header.magic!=kRecordMagic || !header.length || header.length>kMaximumPayloadBytes) {
        lastError_="Queue batch header invalid";return !payloads.empty();
      }
      if(bytes+header.length+1>maximumBytes)return !payloads.empty();
      String payload;payload.reserve(header.length);
      while(payload.length()<header.length) {
        char chunk[256];const size_t wanted=min(sizeof(chunk),size_t(header.length-payload.length()));
        const int received=file.read(reinterpret_cast<uint8_t *>(chunk),wanted);
        if(received<=0)break;
        payload.concat(chunk,static_cast<unsigned int>(received));
      }
      if(payload.length()!=header.length || checksum(reinterpret_cast<const uint8_t *>(payload.c_str()),payload.length())!=header.checksum) {
        lastError_="Queue batch checksum invalid";return !payloads.empty();
      }
      bytes+=payload.length()+1;payloads.push_back(std::move(payload));
    }
  }
  if(!payloads.empty())lastError_="";
  return !payloads.empty();
#endif
}

bool StoreForwardQueue::pop(const bool discarded) {
#if !defined(ESP32)
  (void)discarded;
  return false;
#else
  if (!ready_ || pendingRecords() == 0) {
    return false;
  }
  const uint8_t segment = oldestSegment();
  File file = LittleFS.open(segmentPath(segment), FILE_READ);
  if (!file || !file.seek(metadata_.head[segment])) {
    lastError_ = "Queue pop seek failed";
    return false;
  }
  RecordHeader header{};
  if (file.read(reinterpret_cast<uint8_t *>(&header), sizeof(header)) != sizeof(header) ||
      header.magic != kRecordMagic || header.length == 0 || header.length > kMaximumPayloadBytes) {
    lastError_ = "Queue pop header invalid";
    return false;
  }
  file.close();
  metadata_.head[segment] += sizeof(header) + header.length;
  --count_[segment];
  if (discarded) {
    ++metadata_.droppedRecords;
  }
  if (count_[segment] == 0 && !truncateSegment(segment)) {
    return false;
  }
  if (!saveMetadata()) {
    return false;
  }
  lastError_ = "";
  return true;
#endif
}

bool StoreForwardQueue::isEnabled() const { return enabled_; }
bool StoreForwardQueue::isReady() const { return ready_; }

uint32_t StoreForwardQueue::pendingRecords() const {
#if defined(ESP32)
  return count_[0] + count_[1];
#else
  return 0;
#endif
}

size_t StoreForwardQueue::pendingBytes() const {
#if defined(ESP32)
  return (tail_[0] - metadata_.head[0]) + (tail_[1] - metadata_.head[1]);
#else
  return 0;
#endif
}

size_t StoreForwardQueue::capacityBytes() const {
#if defined(ESP32)
  return segmentMaximumBytes_ * 2U;
#else
  return 0;
#endif
}

uint32_t StoreForwardQueue::droppedRecords() const {
#if defined(ESP32)
  return metadata_.droppedRecords;
#else
  return 0;
#endif
}

String StoreForwardQueue::lastError() const { return lastError_; }

#if defined(ESP32)
uint32_t StoreForwardQueue::checksum(const uint8_t *data, const size_t length) {
  uint32_t value = 2166136261UL;
  for (size_t index = 0; index < length; ++index) {
    value ^= data[index];
    value *= 16777619UL;
  }
  return value;
}

uint32_t StoreForwardQueue::metadataChecksum(const Metadata &metadata) {
  return checksum(reinterpret_cast<const uint8_t *>(&metadata), offsetof(Metadata, checksum));
}

const char *StoreForwardQueue::segmentPath(const uint8_t segment) {
  return segment == 0 ? kSegmentZeroPath : kSegmentOnePath;
}

bool StoreForwardQueue::loadMetadata() {
  File file = LittleFS.open(kMetadataPath, FILE_READ);
  if (!file || file.size() != sizeof(Metadata) ||
      file.read(reinterpret_cast<uint8_t *>(&metadata_), sizeof(metadata_)) != sizeof(metadata_)) {
    return false;
  }
  return metadata_.magic == kMetadataMagic && metadata_.version == kMetadataVersion &&
         metadata_.activeSegment < 2 && metadata_.checksum == metadataChecksum(metadata_);
}

bool StoreForwardQueue::saveMetadata() {
  metadata_.magic = kMetadataMagic;
  metadata_.version = kMetadataVersion;
  metadata_.checksum = metadataChecksum(metadata_);
  if (LittleFS.exists(kMetadataTemporaryPath)) LittleFS.remove(kMetadataTemporaryPath);
  File file = LittleFS.open(kMetadataTemporaryPath, FILE_WRITE);
  if (!file || file.write(reinterpret_cast<const uint8_t *>(&metadata_), sizeof(metadata_)) !=
                   sizeof(metadata_)) {
    lastError_ = "Queue metadata write failed";
    return false;
  }
  file.flush();
  file.close();
  if (LittleFS.exists(kMetadataPath)) LittleFS.remove(kMetadataPath);
  if (!LittleFS.rename(kMetadataTemporaryPath, kMetadataPath)) {
    lastError_ = "Queue metadata commit failed";
    return false;
  }
  return true;
}

bool StoreForwardQueue::scanSegment(const uint8_t segment) {
  File file = LittleFS.open(segmentPath(segment), FILE_READ);
  if (!file) {
    File created = LittleFS.open(segmentPath(segment), FILE_WRITE);
    if (!created) {
      lastError_ = "Queue segment create failed";
      return false;
    }
    created.close();
    metadata_.head[segment] = 0;
    tail_[segment] = 0;
    count_[segment] = 0;
    return true;
  }

  const uint32_t fileSize = file.size();
  uint32_t offset = min(metadata_.head[segment], fileSize);
  metadata_.head[segment] = offset;
  count_[segment] = 0;
  while (offset + sizeof(RecordHeader) <= fileSize) {
    if (!file.seek(offset)) {
      break;
    }
    RecordHeader header{};
    if (file.read(reinterpret_cast<uint8_t *>(&header), sizeof(header)) != sizeof(header) ||
        header.magic != kRecordMagic || header.length == 0 || header.length > kMaximumPayloadBytes ||
        offset + sizeof(header) + header.length > fileSize) {
      break;
    }
    uint32_t value = 2166136261UL;
    for (uint32_t index = 0; index < header.length; ++index) {
      const int byte = file.read();
      if (byte < 0) {
        value = 0;
        break;
      }
      value ^= static_cast<uint8_t>(byte);
      value *= 16777619UL;
    }
    if (value != header.checksum) {
      break;
    }
    offset += sizeof(header) + header.length;
    ++count_[segment];
  }
  file.close();
  tail_[segment] = offset;
  if (tail_[segment] < fileSize) {
    File source = LittleFS.open(segmentPath(segment), FILE_READ);
    if (LittleFS.exists(kSegmentRepairPath)) LittleFS.remove(kSegmentRepairPath);
    File repair = LittleFS.open(kSegmentRepairPath, FILE_WRITE);
    uint8_t buffer[256]{};
    uint32_t remaining = tail_[segment];
    bool repaired = source && repair;
    while (repaired && remaining > 0) {
      const size_t chunk = min(remaining, static_cast<uint32_t>(sizeof(buffer)));
      const size_t read = source.read(buffer, chunk);
      repaired = read == chunk && repair.write(buffer, chunk) == chunk;
      remaining -= chunk;
    }
    repair.flush();
    source.close();
    repair.close();
    if (!repaired || !LittleFS.remove(segmentPath(segment)) ||
        !LittleFS.rename(kSegmentRepairPath, segmentPath(segment))) {
      lastError_ = "Queue tail repair failed";
      return false;
    }
  }
  if (count_[segment] == 0) {
    return truncateSegment(segment);
  }
  return true;
}

bool StoreForwardQueue::rotateSegment() {
  const uint8_t next = metadata_.activeSegment == 0 ? 1 : 0;
  metadata_.droppedRecords += count_[next];
  if (!truncateSegment(next)) {
    return false;
  }
  metadata_.activeSegment = next;
  return saveMetadata();
}

bool StoreForwardQueue::truncateSegment(const uint8_t segment) {
  File file = LittleFS.open(segmentPath(segment), FILE_WRITE);
  if (!file) {
    lastError_ = "Queue segment reset failed";
    return false;
  }
  file.close();
  metadata_.head[segment] = 0;
  tail_[segment] = 0;
  count_[segment] = 0;
  return true;
}

uint8_t StoreForwardQueue::oldestSegment() const {
  const uint8_t inactive = metadata_.activeSegment == 0 ? 1 : 0;
  return count_[inactive] > 0 ? inactive : metadata_.activeSegment;
}
#endif
