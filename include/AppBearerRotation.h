#pragma once

#include <Arduino.h>

class AppBearerRotation {
 public:
  enum class StageResult : uint8_t { Staged, AlreadyPending, AlreadyApplied, Rejected, StorageFault };
  enum class Phase : uint8_t { None = 0, Staged = 1, Acknowledged = 2 };

  bool begin(const char *provisionedBearer);
  StageResult stage(uint32_t version,
                    const String &nonce,
                    const String &token,
                    const String &overlapExpiresAt);
  bool markAcknowledged();
  bool promoteCandidate();
  void abandonCandidate();
  bool factoryReset();

  const char *activeBearer() const;
  const char *candidateBearer() const;
  uint32_t version() const;
  const String &nonce() const;
  const String &overlapExpiresAt() const;
  Phase phase() const;
  bool hasCandidate() const;
  bool hasAppliedAcknowledgement() const;
  const String &lastError() const;

 private:
  bool persistCandidate(Phase phase);
  void clearCandidatePersistent();
  void clearCandidateMemory();

  String activeBearer_;
  String candidateBearer_;
  String nonce_;
  String overlapExpiresAt_;
  String appliedNonce_;
  String lastError_;
  uint32_t version_ = 0;
  uint32_t appliedVersion_ = 0;
  Phase phase_ = Phase::None;
};
