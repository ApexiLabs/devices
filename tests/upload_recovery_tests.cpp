#include "StoreForwardQueue.h"
#include "UploadRecovery.h"
#include "LittleFS.h"
#include <cassert>
#include <iostream>

using Resolution = UploadRecovery::Resolution;

int main() {
  LittleFS.reset();
  StoreForwardQueue queue;
  assert(queue.begin(true, 32768));
  const String bad("{\"sequence\":1,\"timestamp\":\"invalid\"}");
  assert(queue.enqueue(bad)); assert(queue.enqueue("good"));
  UploadRecovery recovery;
  recovery.batchRejected(2);
  assert(!recovery.allowBatch() && queue.pendingRecords() == 2);
  auto resolve = [&](int status, bool permanent) {
    return recovery.resolveRejected(status, permanent,
        [&]{ return queue.preserveRejected(bad,status); },
        [&]{ return queue.popIfMatches(bad); });
  };
  for (int status : {-1, 200, 401, 403, 408, 429, 500, 503}) {
    assert(resolve(status,true) == Resolution::Retained);
    assert(queue.pendingRecords() == 2 && queue.rejectedRecords() == 0);
  }
  assert(resolve(422,false) == Resolution::Retained); // No explicit server signal.
  nextFileWriteLimit=3;
  assert(resolve(422,true) == Resolution::ArchiveFailed);
  assert(queue.pendingRecords()==2 && queue.rejectedRecords()==0);
  LittleFS.failNextRename();
  assert(resolve(422,true) == Resolution::ArchiveFailed);
  assert(queue.pendingRecords() == 2 && queue.rejectedRecords() == 0);
  assert(resolve(422,true) == Resolution::Archived);
  assert(queue.pendingRecords() == 1 && queue.rejectedRecords() == 1);
  String saved; int status=0;
  assert(queue.readRejected(0,saved,status) && saved==bad && status==422);
  assert(queue.peek(saved) && saved=="good");
  assert(queue.popIfMatches(saved)); recovery.recordResolved();
  assert(recovery.allowBatch()); // A later good record can upload, batching recovers.

  // A reset after archive commit but before queue pop duplicates neither data nor slots.
  assert(queue.enqueue(bad));
  assert(queue.preserveRejected(bad,422));
  StoreForwardQueue rebooted; assert(rebooted.begin(true,32768));
  assert(rebooted.rejectedRecords()==1);
  assert(rebooted.preserveRejected(bad,422));
  assert(rebooted.rejectedRecords()==1 && rebooted.pendingRecords()==1);
  assert(rebooted.popIfMatches(bad));

  // Archive success followed by queue metadata failure retains the original.
  assert(rebooted.enqueue("next"));
  assert(rebooted.preserveRejected("next",400));
  LittleFS.failNextRename();
  assert(!rebooted.popIfMatches("next"));
  assert(rebooted.pendingRecords()==1);
  assert(rebooted.readRejected(1,saved,status) && saved=="next");
  assert(rebooted.popIfMatches("next"));

  // Archive capacity never rolls over or overwrites earlier payloads.
  for(size_t i=2;i<StoreForwardQueue::kRejectedSlots;++i)
    assert(rebooted.preserveRejected(String("rejected-"+std::to_string(i)),413));
  assert(!rebooted.preserveRejected("overflow",422));
  assert(rebooted.rejectedRecords()==StoreForwardQueue::kRejectedSlots);
  assert(rebooted.readRejected(0,saved,status) && saved==bad);
  assert(rebooted.droppedRecords()==0); // Archival is not queue overflow or acceptance.
  LittleFS.corruptByte("/upload-rejected-00.bin",16);
  assert(!rebooted.readRejected(0,saved,status));
  assert(!rebooted.preserveRejected("replacement",422)); // Preserve corrupt evidence too.
  assert(!rebooted.readRejected(StoreForwardQueue::kRejectedSlots,saved,status));
  assert(!rebooted.preserveRejected(String(std::string(4097,'x')),422));
  // Capacity rotation during a rejected request must not remove a newer head.
  LittleFS.reset();StoreForwardQueue rotating;assert(rotating.begin(true,128));
  assert(rotating.enqueue("submitted"));
  for(int i=0;i<20;++i)assert(rotating.enqueue(String("record-"+std::to_string(i))));
  String replacement;assert(rotating.peek(replacement) && replacement!="submitted");
  const auto pending=rotating.pendingRecords();
  assert(rotating.preserveRejected("submitted",422));
  assert(rotating.popIfMatches("submitted"));
  assert(rotating.pendingRecords()==pending && rotating.peek(saved) && saved==replacement);
  assert(rotating.readRejected(0,saved,status) && saved=="submitted");
  std::cout << "Rejected upload recovery tests passed\n";
}
