#pragma once

#include <Arduino.h>
#include <vector>

class StoreForwardQueue {
 public:
  bool begin(bool enabled, size_t maximumBytes);
  bool enqueue(const String &payload);
  bool peek(String &payload);
  bool peekBatch(std::vector<String> &payloads, size_t maximumRecords, size_t maximumBytes);
  bool pop(bool discarded = false);
  // An append can rotate away a submitted record before HTTP completes.
  // Never acknowledge the replacement head in that case.
  bool popIfMatches(const String &submitted, bool discarded = false);

  bool isEnabled() const;
  bool isReady() const;
  uint32_t pendingRecords() const;
  size_t pendingBytes() const;
  size_t capacityBytes() const;
  uint32_t droppedRecords() const;
  bool droppedRecordsKnown() const;
  uint32_t corruptionEvents() const;
  size_t quarantinedBytes() const;
  String lastError() const;

 private:
#if defined(ESP32)
  struct Metadata {
    uint32_t magic;
    uint16_t version;
    uint8_t activeSegment;
    uint8_t reserved;
    uint32_t head[2];
    uint32_t droppedRecords;
    uint32_t corruptionEvents;
    uint32_t quarantinedBytes;
    uint32_t checksum;
  };

  struct RecordHeader {
    uint32_t magic;
    uint32_t length;
    uint32_t checksum;
  };

  static uint32_t checksum(const uint8_t *data, size_t length);
  static uint32_t metadataChecksum(const Metadata &metadata);
  static const char *segmentPath(uint8_t segment);
  static const char *quarantinePath(uint8_t segment);
  bool loadMetadata();
  bool saveMetadata();
  bool scanSegment(uint8_t segment);
  bool rotateSegment();
  bool truncateSegment(uint8_t segment);
  uint8_t oldestSegment() const;

  Metadata metadata_{};
  uint32_t tail_[2]{};
  uint32_t count_[2]{};
  size_t segmentMaximumBytes_ = 0;
#endif
  bool enabled_ = false;
  bool ready_ = false;
  String lastError_;
};
