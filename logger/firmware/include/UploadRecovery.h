#pragma once
#include <cstddef>

// Split a rejected envelope without treating any record as acknowledged.
// Authentication, transient errors and ambiguous successes retain the request.
class UploadRecovery {
 public:
  static bool payloadRejected(int status) {
    return status == 400 || status == 413 || status == 422;
  }
  void batchRejected(size_t count) { singlesRemaining_ = count; }
  bool allowBatch() const { return singlesRemaining_ == 0; }
  void recordResolved() { if (singlesRemaining_) --singlesRemaining_; }
  enum class Resolution { Retained, Archived, ArchiveFailed, AdvanceFailed };
  template<class Save, class Advance>
  Resolution resolveRejected(int status, bool permanent, Save save, Advance advance) {
    if (!permanent || !payloadRejected(status)) return Resolution::Retained;
    if (!save()) return Resolution::ArchiveFailed;
    if (!advance()) return Resolution::AdvanceFailed;
    recordResolved();
    return Resolution::Archived;
  }
 private:
  size_t singlesRemaining_ = 0;
};
