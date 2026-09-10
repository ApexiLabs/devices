#include <cstdlib>
#include <iostream>

#include "AppBearerRotation.h"
#if defined(ESP32)
#include <Preferences.h>
#endif

namespace {

int failures = 0;

void expect(const bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << "\n";
    ++failures;
  }
}

void testSafeRotationSequence() {
  AppBearerRotation rotation;
  rotation.factoryReset();
  expect(rotation.begin("old-bearer-token-0001"), "loads provisioned bearer");
  expect(rotation.stage(7, "nonce-0007", "new-bearer-token-0007",
                        "2026-09-05T12:00:00Z") ==
             AppBearerRotation::StageResult::Staged,
         "stages a valid candidate");
  expect(rotation.phase() == AppBearerRotation::Phase::Staged,
         "old bearer acknowledges staged candidate first");
  expect(rotation.hasAppliedAcknowledgement(), "staged heartbeat sends acknowledgement");
  expect(String(rotation.activeBearer()) == String("old-bearer-token-0001"),
         "old bearer remains active before acknowledgement");
  expect(rotation.markAcknowledged(), "persists acknowledgement boundary");
  expect(rotation.phase() == AppBearerRotation::Phase::Acknowledged,
         "candidate becomes eligible for proof");
  expect(rotation.promoteCandidate(), "promotes only after proof succeeds");
  expect(String(rotation.activeBearer()) == String("new-bearer-token-0007"),
         "new bearer becomes active");
  expect(!rotation.hasCandidate(), "candidate is cleared after promotion");
  expect(!rotation.hasAppliedAcknowledgement(), "completed heartbeat omits stale acknowledgement");
  expect(rotation.stage(7, "nonce-0007", "new-bearer-token-0007",
                        "2026-09-05T12:00:00Z") ==
             AppBearerRotation::StageResult::AlreadyApplied,
         "replayed applied delivery is idempotent");
}

void testReplayAndBypassProtection() {
  AppBearerRotation rotation;
  rotation.factoryReset();
  rotation.begin("old-bearer-token-0001");
  expect(rotation.stage(8, "nonce-0008", "new-bearer-token-0008",
                        "2026-09-05T12:00:00Z") ==
             AppBearerRotation::StageResult::Staged,
         "stages initial request");
  expect(!rotation.promoteCandidate(), "cannot bypass acknowledgement phase");
  expect(rotation.stage(8, "nonce-0008", "new-bearer-token-0008",
                        "2026-09-05T12:00:00Z") ==
             AppBearerRotation::StageResult::AlreadyPending,
         "duplicate delivery is idempotent");
  expect(rotation.stage(8, "different-nonce", "other-bearer-token-0008",
                        "2026-09-05T12:00:00Z") ==
             AppBearerRotation::StageResult::Rejected,
         "same version cannot replace a pending candidate");
  expect(rotation.stage(7, "nonce-0007", "older-bearer-token-0007",
                        "2026-09-05T12:00:00Z") ==
             AppBearerRotation::StageResult::Rejected,
         "stale versions are rejected");
  expect(rotation.stage(9, "nonce-0009", "newer-bearer-token-0009",
                        "2026-09-05T12:00:00Z") ==
             AppBearerRotation::StageResult::Rejected,
         "new request cannot replace an unproven candidate");
  expect(String(rotation.activeBearer()) == String("old-bearer-token-0001"),
         "rejected candidates never destroy old bearer");
}

void testMalformedRotationRejected() {
  AppBearerRotation rotation;
  rotation.factoryReset();
  rotation.begin("old-bearer-token-0001");
  expect(rotation.stage(1, "short", "new-bearer-token-0001",
                        "2026-09-05T12:00:00Z") ==
             AppBearerRotation::StageResult::Rejected,
         "short nonce rejected");
  expect(rotation.stage(1, "nonce-0001", "short",
                        "2026-09-05T12:00:00Z") ==
             AppBearerRotation::StageResult::Rejected,
         "short bearer rejected");
  expect(!rotation.hasCandidate(), "malformed requests leave no candidate");
}

void testAbandonKeepsRecoveryBearer() {
  AppBearerRotation rotation;
  rotation.factoryReset();
  rotation.begin("old-bearer-token-0001");
  rotation.stage(10, "nonce-0010", "new-bearer-token-0010",
                 "2026-09-05T12:00:00Z");
  rotation.abandonCandidate();
  expect(!rotation.hasCandidate(), "rejected delivery can be abandoned");
  expect(String(rotation.activeBearer()) == String("old-bearer-token-0001"),
         "abandon preserves the recovery bearer");
  expect(rotation.stage(11, "nonce-0011", "new-bearer-token-0011",
                        "2026-09-05T13:00:00Z") ==
             AppBearerRotation::StageResult::Staged,
         "a later authorized rotation can recover");
}

void testRotationSurvivesRebootBoundaries() {
  AppBearerRotation firstBoot;
  firstBoot.factoryReset();
  firstBoot.begin("old-bearer-token-0001");
  firstBoot.stage(12, "nonce-0012", "new-bearer-token-0012",
                  "2026-09-05T14:00:00Z");

  AppBearerRotation afterStageReboot;
  afterStageReboot.begin("old-bearer-token-0001");
  expect(afterStageReboot.phase() == AppBearerRotation::Phase::Staged,
         "staged candidate survives reboot");
  expect(afterStageReboot.overlapExpiresAt() == String("2026-09-05T14:00:00Z"),
         "server overlap boundary survives reboot for recovery evidence");
  expect(afterStageReboot.markAcknowledged(), "acknowledgement persists after reboot");

  AppBearerRotation afterAckReboot;
  afterAckReboot.begin("old-bearer-token-0001");
  expect(afterAckReboot.phase() == AppBearerRotation::Phase::Acknowledged,
         "acknowledged candidate survives reboot");
  expect(!afterAckReboot.hasAppliedAcknowledgement(),
         "candidate proof retries omit acknowledgement even after lost response");
  expect(afterAckReboot.promoteCandidate(), "candidate promotes after reboot proof");

  AppBearerRotation afterPromotionReboot;
  afterPromotionReboot.begin("old-bearer-token-0001");
  expect(String(afterPromotionReboot.activeBearer()) == String("new-bearer-token-0012"),
         "promoted bearer survives reboot");
  expect(!afterPromotionReboot.hasAppliedAcknowledgement(),
         "completed acknowledgement stays suppressed after reboot");
}

}  // namespace

#if defined(ESP32)
void testNvsAcknowledgementFailures() {
  AppBearerRotation rotation;
  rotation.factoryReset();
  rotation.begin("old-bearer-token-0001");
  rotation.stage(1, "nonce-0001", "new-bearer-token-0001", "2026-09-05T14:00:00Z");
  Preferences::failKey = "phase";
  expect(!rotation.markAcknowledged(), "failed NVS phase commit is reported");
  expect(rotation.phase() == AppBearerRotation::Phase::Staged,
         "failed commit cannot advance in-memory phase");
  expect(!rotation.promoteCandidate(), "failed commit cannot permit promotion");
  AppBearerRotation reboot;
  reboot.begin("old-bearer-token-0001");
  expect(reboot.hasCandidate() && reboot.hasAppliedAcknowledgement(),
         "failed acknowledgement write preserves durable candidate and ack retry");
  Preferences::failKey.clear();
  Preferences::cutAfterKey = "phase";
  try {
    reboot.markAcknowledged();
    expect(false, "power cut must interrupt phase commit");
  } catch (const Preferences::PowerCut &) {}
  Preferences::cutAfterKey.clear();
  AppBearerRotation afterCut;
  afterCut.begin("old-bearer-token-0001");
  expect(afterCut.hasCandidate() && afterCut.phase() == AppBearerRotation::Phase::Acknowledged,
         "cut after atomic phase commit retains candidate for proof");
  expect(!afterCut.hasAppliedAcknowledgement(), "proof retry after reboot omits stale ack");
  Preferences::failKey = "active";
  expect(!afterCut.promoteCandidate(), "failed promotion retains candidate");
  Preferences::failKey.clear();
  AppBearerRotation afterPromotionFailure;
  afterPromotionFailure.begin("old-bearer-token-0001");
  expect(afterPromotionFailure.hasCandidate() && !afterPromotionFailure.hasAppliedAcknowledgement(),
         "lost proof response or failed promotion retries without stale ack");
  expect(afterPromotionFailure.promoteCandidate(), "promotion can recover after write failure");
}
#endif

int main() {
  testSafeRotationSequence();
  testReplayAndBypassProtection();
  testMalformedRotationRejected();
  testAbandonKeepsRecoveryBearer();
  testRotationSurvivesRebootBoundaries();
#if defined(ESP32)
  testNvsAcknowledgementFailures();
#endif
  if (failures == 0) {
    std::cout << "All app bearer rotation host tests passed\n";
    return EXIT_SUCCESS;
  }
  return EXIT_FAILURE;
}
