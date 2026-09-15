#include "AppBearerRotation.h"

#if defined(ESP32)
#include <Preferences.h>
#endif

namespace {

#if defined(ESP32)
constexpr const char *kNamespace = "apexi_auth";
#endif
constexpr size_t kMaximumBearerLength = 511;
constexpr size_t kMaximumNonceLength = 96;
constexpr size_t kMaximumExpiryLength = 40;

#if defined(APEXI_HOST_TEST)
struct HostRecord {
  String active;
  String candidate;
  String nonce;
  String overlap;
  String appliedNonce;
  uint32_t version = 0;
  uint32_t appliedVersion = 0;
  AppBearerRotation::Phase phase = AppBearerRotation::Phase::None;
  bool ready = false;
};
HostRecord hostRecord;
#endif

bool validSecret(const String &value, const size_t maximum, const size_t minimum) {
  if (value.length() < minimum || value.length() > maximum) {
    return false;
  }
  for (size_t index = 0; index < value.length(); ++index) {
    const uint8_t character = static_cast<uint8_t>(value[index]);
    if (character < 0x21 || character == 0x7f) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool AppBearerRotation::begin(const char *provisionedBearer) {
  activeBearer_ = provisionedBearer == nullptr ? "" : provisionedBearer;
  clearCandidateMemory();
  appliedVersion_ = 0;
  appliedNonce_ = "";
  lastError_ = "";
#if defined(ESP32)
  Preferences auth;
  if (!auth.begin(kNamespace, true)) {
    lastError_ = "credential rotation store unavailable";
    return false;
  }
  const String persistedActive = auth.getString("active", "");
  if (!persistedActive.isEmpty()) {
    activeBearer_ = persistedActive;
  }
  appliedVersion_ = auth.getUInt("applied_ver", 0);
  appliedNonce_ = auth.getString("applied_nonce", "");
  const bool ready = auth.getBool("cand_ready", false);
  if (ready) {
    version_ = auth.getUInt("cand_ver", 0);
    nonce_ = auth.getString("cand_nonce", "");
    candidateBearer_ = auth.getString("candidate", "");
    overlapExpiresAt_ = auth.getString("overlap_at", "");
    phase_ = static_cast<Phase>(auth.getUChar("phase", 0));
  }
  auth.end();
  if (ready && (version_ == 0 ||
                !validSecret(nonce_, kMaximumNonceLength, 8) ||
                !validSecret(candidateBearer_, kMaximumBearerLength, 16) ||
                !validSecret(overlapExpiresAt_, kMaximumExpiryLength, 20) ||
                (phase_ != Phase::Staged && phase_ != Phase::Acknowledged))) {
    clearCandidatePersistent();
    clearCandidateMemory();
    lastError_ = "credential rotation record is invalid";
    return false;
  }
#elif defined(APEXI_HOST_TEST)
  if (!hostRecord.active.isEmpty()) {
    activeBearer_ = hostRecord.active;
  }
  appliedVersion_ = hostRecord.appliedVersion;
  appliedNonce_ = hostRecord.appliedNonce;
  if (hostRecord.ready) {
    candidateBearer_ = hostRecord.candidate;
    nonce_ = hostRecord.nonce;
    overlapExpiresAt_ = hostRecord.overlap;
    version_ = hostRecord.version;
    phase_ = hostRecord.phase;
  }
#endif
  return !activeBearer_.isEmpty();
}

AppBearerRotation::StageResult AppBearerRotation::stage(
    const uint32_t version,
    const String &nonce,
    const String &token,
    const String &overlapExpiresAt) {
  if (version == 0 || !validSecret(nonce, kMaximumNonceLength, 8) ||
      !validSecret(token, kMaximumBearerLength, 16) ||
      !validSecret(overlapExpiresAt, kMaximumExpiryLength, 20)) {
    lastError_ = "invalid credential rotation request";
    return StageResult::Rejected;
  }
  if (version == appliedVersion_) {
    if (nonce == appliedNonce_ && token == activeBearer_) {
      return StageResult::AlreadyApplied;
    }
    lastError_ = "credential rotation version was reused";
    return StageResult::Rejected;
  }
  if (hasCandidate()) {
    if (version == version_ && nonce == nonce_ && token == candidateBearer_) {
      return StageResult::AlreadyPending;
    }
    lastError_ = version <= version_ ? "credential rotation is stale"
                                     : "credential rotation already pending";
    return StageResult::Rejected;
  } else if (version < appliedVersion_) {
    lastError_ = "credential rotation is stale";
    return StageResult::Rejected;
  }
  if (token == activeBearer_) {
    lastError_ = "credential rotation token is already active";
    return StageResult::Rejected;
  }
  version_ = version;
  nonce_ = nonce;
  candidateBearer_ = token;
  overlapExpiresAt_ = overlapExpiresAt;
  phase_ = Phase::Staged;
  if (!persistCandidate(phase_)) {
    clearCandidateMemory();
    return StageResult::StorageFault;
  }
  lastError_ = "";
  return StageResult::Staged;
}

bool AppBearerRotation::markAcknowledged() {
  if (!hasCandidate()) {
    return false;
  }
  // The complete candidate is already durable. Change only its phase; clearing
  // cand_ready here would lose the candidate on a power cut after server ack.
#if defined(ESP32)
  Preferences auth;
  if (!auth.begin(kNamespace, false)) {
    lastError_ = "credential rotation store unavailable";
    return false;
  }
  const bool saved = auth.putUChar("phase", static_cast<uint8_t>(Phase::Acknowledged)) > 0;
  auth.end();
  if (!saved) {
    lastError_ = "credential rotation acknowledgement persistence failed";
    return false;
  }
#elif defined(APEXI_HOST_TEST)
  hostRecord.phase = Phase::Acknowledged;
#endif
  phase_ = Phase::Acknowledged;
  lastError_ = "";
  return true;
}

bool AppBearerRotation::promoteCandidate() {
  if (!hasCandidate() || phase_ != Phase::Acknowledged) {
    return false;
  }
#if defined(ESP32)
  Preferences auth;
  if (!auth.begin(kNamespace, false)) {
    lastError_ = "credential rotation store unavailable";
    return false;
  }
  // Write the usable credential first. If power is lost before the remaining
  // metadata commits, reboot still retries the same candidate safely.
  bool saved = auth.putString("active", candidateBearer_) > 0;
  saved = saved && auth.putUInt("applied_ver", version_) > 0;
  saved = saved && auth.putString("applied_nonce", nonce_) > 0;
  saved = saved && auth.remove("cand_ready");
  if (saved) {
    auth.remove("candidate");
    auth.remove("cand_ver");
    auth.remove("cand_nonce");
    auth.remove("overlap_at");
    auth.remove("phase");
  }
  auth.end();
  if (!saved) {
    lastError_ = "credential rotation promotion failed";
    return false;
  }
#elif defined(APEXI_HOST_TEST)
  hostRecord.active = candidateBearer_;
  hostRecord.appliedVersion = version_;
  hostRecord.appliedNonce = nonce_;
  hostRecord.ready = false;
#endif
  activeBearer_ = candidateBearer_;
  appliedVersion_ = version_;
  appliedNonce_ = nonce_;
  clearCandidateMemory();
  lastError_ = "";
  return true;
}

void AppBearerRotation::abandonCandidate() {
  clearCandidatePersistent();
  clearCandidateMemory();
}

bool AppBearerRotation::factoryReset() {
#if defined(ESP32)
  Preferences auth;
  if (!auth.begin(kNamespace, false)) {
    return false;
  }
  const bool cleared = auth.clear();
  auth.end();
  return cleared;
#elif defined(APEXI_HOST_TEST)
  hostRecord = {};
  return true;
#else
  return true;
#endif
}

const char *AppBearerRotation::activeBearer() const { return activeBearer_.c_str(); }
const char *AppBearerRotation::candidateBearer() const { return candidateBearer_.c_str(); }
uint32_t AppBearerRotation::version() const { return hasCandidate() ? version_ : appliedVersion_; }
const String &AppBearerRotation::nonce() const {
  return hasCandidate() ? nonce_ : appliedNonce_;
}
const String &AppBearerRotation::overlapExpiresAt() const { return overlapExpiresAt_; }
AppBearerRotation::Phase AppBearerRotation::phase() const { return phase_; }
bool AppBearerRotation::hasCandidate() const {
  return phase_ != Phase::None && !candidateBearer_.isEmpty();
}
bool AppBearerRotation::hasAppliedAcknowledgement() const {
  // Candidate proof and later heartbeats must omit the ack: the server can
  // already be complete if its proof response was lost or promotion failed.
  return hasCandidate() && phase_ == Phase::Staged;
}
const String &AppBearerRotation::lastError() const { return lastError_; }

bool AppBearerRotation::persistCandidate(const Phase phase) {
#if defined(ESP32)
  Preferences auth;
  if (!auth.begin(kNamespace, false)) {
    lastError_ = "credential rotation store unavailable";
    return false;
  }
  bool saved = auth.putBool("cand_ready", false) > 0;
  saved = saved && auth.putUInt("cand_ver", version_) > 0;
  saved = saved && auth.putString("cand_nonce", nonce_) > 0;
  saved = saved && auth.putString("candidate", candidateBearer_) > 0;
  saved = saved && auth.putString("overlap_at", overlapExpiresAt_) > 0;
  saved = saved && auth.putUChar("phase", static_cast<uint8_t>(phase)) > 0;
  saved = saved && auth.putBool("cand_ready", true) > 0;
  auth.end();
  if (!saved) {
    lastError_ = "credential rotation persistence failed";
  }
  return saved;
#elif defined(APEXI_HOST_TEST)
  hostRecord.candidate = candidateBearer_;
  hostRecord.nonce = nonce_;
  hostRecord.overlap = overlapExpiresAt_;
  hostRecord.version = version_;
  hostRecord.phase = phase;
  hostRecord.ready = true;
  return true;
#else
  (void)phase;
  lastError_ = "credential rotation requires ESP32 encrypted NVS";
  return false;
#endif
}

void AppBearerRotation::clearCandidateMemory() {
  candidateBearer_ = "";
  nonce_ = "";
  overlapExpiresAt_ = "";
  version_ = 0;
  phase_ = Phase::None;
}

void AppBearerRotation::clearCandidatePersistent() {
#if defined(ESP32)
  Preferences auth;
  if (!auth.begin(kNamespace, false)) {
    return;
  }
  auth.remove("cand_ready");
  auth.remove("candidate");
  auth.remove("cand_ver");
  auth.remove("cand_nonce");
  auth.remove("overlap_at");
  auth.remove("phase");
  auth.end();
#elif defined(APEXI_HOST_TEST)
  hostRecord.candidate = "";
  hostRecord.nonce = "";
  hostRecord.overlap = "";
  hostRecord.version = 0;
  hostRecord.phase = Phase::None;
  hostRecord.ready = false;
#endif
}
