#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "LittleFS.h"
#include "StoreForwardQueue.h"

namespace {

int failures = 0;

void expect(const bool condition, const std::string &message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << "\n";
    ++failures;
  }
}

std::string text(const String &value) { return value.c_str(); }

uint32_t checksum(const uint8_t *data, const size_t length) {
  uint32_t value = 2166136261UL;
  for (size_t index = 0; index < length; ++index) {
    value ^= data[index];
    value *= 16777619UL;
  }
  return value;
}

void testAppendReplayAndReboot() {
  LittleFS.reset();
  StoreForwardQueue queue;
  expect(queue.begin(true, 256), "queue starts");
  expect(queue.droppedRecordsKnown() && queue.droppedRecords() == 0,
         "fresh queue has observed zero drops");
  expect(queue.enqueue("alpha"), "first append succeeds");
  expect(queue.enqueue("bravo"), "second append succeeds");

  StoreForwardQueue rebooted;
  expect(rebooted.begin(true, 256), "queue remounts");
  expect(rebooted.pendingRecords() == 2, "reboot preserves record count");
  String payload;
  expect(rebooted.peek(payload) && text(payload) == "alpha", "oldest record replays first");
  expect(rebooted.pop(), "first record acknowledges");
  expect(rebooted.peek(payload) && text(payload) == "bravo", "second record follows");
  expect(rebooted.pop(), "second record acknowledges");
  expect(rebooted.pendingRecords() == 0, "acknowledged queue is empty");
}

void testMountFailureNeverFormats() {
  LittleFS.reset();
  StoreForwardQueue queue;
  expect(queue.begin(true, 256) && queue.enqueue("preserve-me"), "fixture is queued");
  const auto before = LittleFS.bytes("/sfq-0.log");

  LittleFS.setMountResult(false);
  StoreForwardQueue failedBoot;
  expect(!failedBoot.begin(true, 256), "mount failure is reported");
  expect(!failedBoot.droppedRecordsKnown(), "unmounted queue has no observed drop total");
  expect(!LittleFS.lastFormatOnFail(), "mount does not request formatting");
  expect(LittleFS.bytes("/sfq-0.log") == before, "mount failure preserves bytes");

  LittleFS.setMountResult(true);
  StoreForwardQueue recovered;
  String payload;
  expect(recovered.begin(true, 256), "later mount recovers");
  expect(recovered.peek(payload) && text(payload) == "preserve-me",
         "preserved record remains replayable");
}

void testTornAppendRepairsOnlyInvalidTail() {
  LittleFS.reset();
  StoreForwardQueue queue;
  expect(queue.begin(true, 256) && queue.enqueue("complete"), "complete record queued");
  const size_t validSize = LittleFS.bytes("/sfq-0.log").size();
  LittleFS.appendRaw("/sfq-0.log", {0x31, 0x51, 0x46, 0x53, 0x04});

  StoreForwardQueue rebooted;
  String payload;
  expect(rebooted.begin(true, 256), "torn append is repairable");
  expect(LittleFS.bytes("/sfq-0.log").size() == validSize, "invalid tail is removed");
  expect(rebooted.corruptionEvents() == 1, "torn append repair is counted");
  expect(rebooted.quarantinedBytes() == 5, "torn bytes are measured");
  expect(LittleFS.bytes("/sfq-0.corrupt").size() == 5, "torn bytes are quarantined");
  expect(rebooted.peek(payload) && text(payload) == "complete", "valid prefix survives repair");
}

void testCorruptRecordIsQuarantined() {
  LittleFS.reset();
  StoreForwardQueue queue;
  expect(queue.begin(true, 256) && queue.enqueue("damaged"), "record queued before corruption");
  const size_t recordBytes = LittleFS.bytes("/sfq-0.log").size();
  LittleFS.corruptByte("/sfq-0.log", 12);

  StoreForwardQueue rebooted;
  expect(rebooted.begin(true, 256), "corrupt record is repaired deterministically");
  expect(rebooted.pendingRecords() == 0, "corrupt record is excluded from replay");
  expect(rebooted.corruptionEvents() == 1, "corrupt record repair is counted");
  expect(rebooted.quarantinedBytes() == recordBytes, "corrupt record bytes are measured");
  expect(LittleFS.bytes("/sfq-0.corrupt").size() == recordBytes,
         "corrupt record remains available for service recovery");
  expect(text(rebooted.lastError()).find("quarantined") != std::string::npos,
         "diagnostics explain corruption recovery");

  StoreForwardQueue secondReboot;
  expect(secondReboot.begin(true, 256), "queue remounts after corruption recovery");
  expect(secondReboot.corruptionEvents() == 1, "corruption count persists across reboot");
  expect(secondReboot.quarantinedBytes() == recordBytes,
         "quarantine usage persists across reboot");
}

void testInterruptedMetadataCommitReplaysAtLeastOnce() {
  LittleFS.reset();
  StoreForwardQueue queue;
  expect(queue.begin(true, 256) && queue.enqueue("retry-after-reset"), "record queued");
  String payload;
  expect(queue.peek(payload), "record prepared for replay");
  LittleFS.failNextRename();
  expect(!queue.pop(), "interrupted acknowledgement is reported");

  StoreForwardQueue rebooted;
  expect(rebooted.begin(true, 256), "queue rebuilds after metadata interruption");
  expect(rebooted.peek(payload) && text(payload) == "retry-after-reset",
         "uncommitted acknowledgement is replayed at least once");
}

void testCorruptMetadataSalvagesRecords() {
  LittleFS.reset();
  StoreForwardQueue queue;
  expect(queue.begin(true, 256) && queue.enqueue("salvage"), "record queued before metadata fault");
  LittleFS.corruptByte("/sfq.meta", 0);

  StoreForwardQueue rebooted;
  String payload;
  expect(rebooted.begin(true, 256), "corrupt metadata is rebuilt");
  expect(rebooted.peek(payload) && text(payload) == "salvage", "record survives metadata rebuild");
  expect(!rebooted.droppedRecordsKnown(), "metadata loss cannot invent a zero drop total");
  StoreForwardQueue secondReboot;
  expect(secondReboot.begin(true, 256), "recovered queue remounts");
  expect(!secondReboot.droppedRecordsKnown(), "drop uncertainty persists after restart");
}

void testVersionOneMetadataMigratesWithoutLosingCounters() {
  struct LegacyMetadataV1 {
    uint32_t magic;
    uint16_t version;
    uint8_t activeSegment;
    uint8_t reserved;
    uint32_t head[2];
    uint32_t droppedRecords;
    uint32_t checksum;
  } metadata{0x5346514DUL, 1, 0, 0, {0, 0}, 7, 0};
  metadata.checksum = checksum(reinterpret_cast<const uint8_t *>(&metadata),
                               offsetof(LegacyMetadataV1, checksum));
  LittleFS.reset();
  LittleFS.appendRaw(
      "/sfq.meta",
      std::vector<uint8_t>(reinterpret_cast<const uint8_t *>(&metadata),
                           reinterpret_cast<const uint8_t *>(&metadata) + sizeof(metadata)));

  StoreForwardQueue queue;
  expect(queue.begin(true, 256), "version one metadata migrates");
  expect(queue.droppedRecords() == 7, "metadata migration preserves drop counter");
  expect(LittleFS.bytes("/sfq.meta").size() > sizeof(metadata),
         "metadata migration writes the current format");
}

void testRotationDropsOldestSegmentVisibly() {
  LittleFS.reset();
  StoreForwardQueue queue;
  expect(queue.begin(true, 128), "small queue starts");
  const String first("11111111111111111111");
  const String second("22222222222222222222");
  const String third("33333333333333333333");
  const String fourth("44444444444444444444");
  const String fifth("55555555555555555555");
  expect(queue.enqueue(first) && queue.enqueue(second) && queue.enqueue(third) &&
             queue.enqueue(fourth) && queue.enqueue(fifth),
         "records rotate across segments");
  expect(queue.droppedRecords() == 2, "rotation reports dropped oldest segment");
  StoreForwardQueue rebooted;
  expect(rebooted.begin(true, 128), "rotated queue remounts");
  String payload;
  expect(rebooted.peek(payload) && text(payload) == text(third),
         "oldest retained record replays first");
  expect(rebooted.pop() && rebooted.peek(payload) && text(payload) == text(fourth),
         "second retained record replays next");
  expect(rebooted.pop() && rebooted.peek(payload) && text(payload) == text(fifth),
         "newest retained record replays last");
}

void testRecordCannotExceedConfiguredCapacity() {
  LittleFS.reset();
  StoreForwardQueue queue;
  expect(queue.begin(true, 30), "minimum-capacity queue starts");
  expect(!queue.enqueue("payload-that-does-not-fit"), "oversized record is rejected");
  expect(queue.pendingRecords() == 0, "oversized record cannot overrun its segment");
  expect(text(queue.lastError()).find("exceeds") != std::string::npos,
         "capacity rejection is observable");
}

void testInterruptedCapacityDropKeepsOldestSegment() {
  LittleFS.reset();
  StoreForwardQueue queue;
  expect(queue.begin(true, 128), "small queue starts for rotation fault");
  expect(queue.enqueue("11111111111111111111") &&
             queue.enqueue("22222222222222222222") &&
             queue.enqueue("33333333333333333333") &&
             queue.enqueue("44444444444444444444"),
         "both segments fill before rotation fault");
  LittleFS.failNextRename();
  expect(!queue.enqueue("55555555555555555555"), "failed drop commit aborts rotation");
  expect(queue.droppedRecords() == 0, "uncommitted capacity drop is not claimed");

  StoreForwardQueue rebooted;
  String payload;
  expect(rebooted.begin(true, 128), "queue remounts after interrupted rotation");
  expect(rebooted.pendingRecords() == 4, "interrupted rotation preserves all records");
  expect(rebooted.peek(payload) && text(payload) == "11111111111111111111",
         "oldest segment remains replayable after interrupted rotation");
}

}  // namespace

int main() {
  {
    LittleFS.reset();
    StoreForwardQueue queue;
    expect(queue.begin(true, 128), "async queue starts");
    const String submitted("11111111111111111111");
    expect(queue.enqueue(submitted), "capture submitted head");
    for (const char *record : {"22222222222222222222", "33333333333333333333",
                              "44444444444444444444", "55555555555555555555"}) {
      expect(queue.enqueue(record), "capture while request is in flight");
    }
    const auto pending = queue.pendingRecords();
    String current;
    expect(queue.peek(current) && current != submitted, "capacity rotation replaced in-flight head");
    expect(queue.popIfMatches(submitted), "late HTTP success is harmless");
    expect(queue.pendingRecords() == pending, "late success cannot pop newer head");
    expect(queue.popIfMatches(current), "matching HTTP success acknowledges exact record");
    expect(queue.pendingRecords() == pending - 1, "only matching head removed");
    expect(queue.peek(current), "next record exists");
    LittleFS.failNextRename();
    expect(!queue.popIfMatches(current), "metadata failure retains record for retry");
    String retry;
    expect(queue.peek(retry) && retry == current, "retry has exact original identity");
  }
  testAppendReplayAndReboot();
  testMountFailureNeverFormats();
  testTornAppendRepairsOnlyInvalidTail();
  testCorruptRecordIsQuarantined();
  testCorruptMetadataSalvagesRecords();
  testVersionOneMetadataMigratesWithoutLosingCounters();
  testInterruptedMetadataCommitReplaysAtLeastOnce();
  testRotationDropsOldestSegmentVisibly();
  testRecordCannotExceedConfiguredCapacity();
  testInterruptedCapacityDropKeepsOldestSegment();
  if (failures == 0) {
    std::cout << "All store-forward host tests passed\n";
    return EXIT_SUCCESS;
  }
  std::cerr << failures << " store-forward host test(s) failed\n";
  return EXIT_FAILURE;
}
