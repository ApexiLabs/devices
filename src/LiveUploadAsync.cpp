#include "LiveUpload.h"
#if defined(ESP32)
#include "LoggerAuthorization.h"
#include "UploadAcknowledgement.h"
#include "Logic.h"
#include "QueueAge.h"
#include "BatchAcknowledgement.h"
#include <esp_system.h>
#include <WiFi.h>
#include <time.h>

bool LiveUpload::captureHttps(const AppState &state) {
  if(authorization_ && (authorization_->restartRequired() || !authorization_->bearer()[0] ||
      String(authorization_->uploadConfig().deviceId)!=deviceId_)) {
    lastPublishMs_=millis();lastError_="Device authorization required before upload capture";return false;
  }
  const uint32_t sequence=lastSequence_+1;
  const String payload=buildSnapshotJson(state,sequence);
  lastPublishMs_=millis();
  if(storeForwardQueue_.isReady()) {
    if(!storeForwardQueue_.enqueue(payload)){++performance_.captureRejected;lastError_="Onboard capture failed: "+storeForwardQueue_.lastError();return false;}
  } else {
    // Keep the oldest pending volatile sample, never overwrite an in-flight one.
    if(!volatilePayload_.isEmpty()){++performance_.captureRejected;lastError_="Upload capture full; SD logging remains separate";return false;}
    volatilePayload_=payload;
  }
  lastSequence_=sequence;++performance_.captured;
  if(queueOldestSession_.isEmpty())refreshQueueOldest();
  return true;
}

bool LiveUpload::submitHttps(Operation operation,const String &payload) {
  const bool status=operation!=Operation::Snapshot && operation!=Operation::Batch;
  const char *token=authorization_?authorization_->bearer(status):(bearerRotation_?
      (operation==Operation::RotationProof?bearerRotation_->candidateBearer():bearerRotation_->activeBearer()):config_.appDeviceToken);
  if(!token || !token[0]){lastError_="Device authorization required";return false;}
  const String url="https://"+String(config_.mqttHost)+":"+String(config_.mqttPort)+String(config_.httpsPath)+(status?"/status":operation==Operation::Batch?"/snapshot-batch":"/snapshot");
  if(!HttpsWorker::shared().submit(HttpsWorker::Owner::Telemetry,url.c_str(),payload.c_str(),token,
       config_.cloudflareAccessClientId,config_.cloudflareAccessClientSecret))return false;
  operation_=operation;lastHttpsAttemptMs_=millis();
  diagnostics_.transportAttempt(httpsRecoveryPending_);
  return true;
}

void LiveUpload::serviceHttps(uint32_t now) {
  if(!workerReady_)return;
  auto &worker=HttpsWorker::shared();
  if(const auto *result=worker.result(HttpsWorker::Owner::Telemetry)) {
    const bool accepted=operation_==Operation::Batch?BatchAcknowledgement::accepted(result->status,result->body,batchId_.c_str(),batchRecords_.size()):UploadEvidence::acceptedResponse(result->status,result->body);
    ++performance_.requests;if(result->reused)++performance_.reused;
    performance_.lastRequestMs=result->durationMs;
    lastHttpStatus_=result->status;completedMs_=now;
    lastPostRetryable_=Logic::isRetryableHttpStatus(result->status) || (result->status>=200 && result->status<300 && !accepted);
    backoff_=!accepted;
    httpsRecoveryPending_=!accepted;
    if(operation_==Operation::Batch) {
      ++performance_.batchRequests;
      if(accepted)++performance_.batchAccepted;
      uploadEvidence_.record(accepted,now);
      if(accepted){performance_.accepted+=batchRecords_.size();batchAcknowledged_=true;batchAckIndex_=0;}
      else if(result->status==404 || result->status==405) {
        // Old servers retain the original per-record path. No data is removed.
        batchEnabled_=false;batchRecords_.clear();batchPayload_="";batchId_="";backoff_=false;
        performance_.batchEnabled=false;
      }
    } else if(operation_==Operation::Snapshot) {
      uploadEvidence_.record(accepted,now);
      if(accepted) {
        ++performance_.accepted;
        StaticJsonDocument<96> filter;filter["timestamp"]=true;
        StaticJsonDocument<192> stamp;
        if(!deserializeJson(stamp,inFlightPayload_,DeserializationOption::Filter(filter)))performance_.lastSampleEpoch=QueueAge::epoch(stamp["timestamp"]|"");
        if(durableInFlight_) {
          // Capture may rotate queue segments while HTTP is pending. An ACK
          // may remove only the exact submitted head, never a newer record.
          if(!storeForwardQueue_.popIfMatches(inFlightPayload_)) {
            lastError_="Queue acknowledgement persistence failed";backoff_=true;
          }
        } else if(volatilePayload_==inFlightPayload_)volatilePayload_="";
      }
      inFlightPayload_="";
    } else {
      pacing_.statusCompleted(now);statusRequested_=false;
      if(!authorization_ && bearerRotation_) {
        if(operation_==Operation::RotationAck) {
          if(accepted) {
            if(!bearerRotation_->markAcknowledged()){managementError_=bearerRotation_->lastError();backoff_=true;}
            else statusRequested_=true;
          } else if(result->status>=400 && result->status<500 && !lastPostRetryable_) {
            bearerRotation_->abandonCandidate();fallbackRequested_=true;statusRequested_=true;
          }
        } else if(operation_==Operation::RotationProof) {
          if(accepted) {
            if(!bearerRotation_->promoteCandidate()){managementError_=bearerRotation_->lastError();backoff_=true;}
          } else {fallbackRequested_=true;statusRequested_=true;}
        }
      }
      if(operation_==Operation::Fallback) {
        if(authorization_)authorization_->finishRotationFallback(accepted);
        fallbackRequested_=false;
      } else if(!accepted && authorization_ && authorization_->hasRotation() && result->status>=400 && result->status<500 && !Logic::isRetryableHttpStatus(result->status)) {
        fallbackRequested_=true;
      } else if(authorization_)authorization_->statusResult(accepted);
      if(accepted){lastStatusPublishMs_=now;authenticatedHeartbeatObserved_=true;lastAuthenticatedHeartbeatMs_=now;consumeHttpsDesiredConfig(result->body);}
      // A staged rotation progresses without waiting a full heartbeat interval.
      if(accepted && authorization_ && authorization_->hasRotation())statusRequested_=true;
      if(accepted && !authorization_ && bearerRotation_ && bearerRotation_->hasCandidate())statusRequested_=true;
    }
    httpsConnected_=accepted;
    if(!accepted)lastError_="HTTPS request failed ("+String(result->status)+"); queued data retained";
    else if(!backoff_)lastError_=storeForwardQueue_.pendingRecords()?"Replaying onboard queue: "+String(storeForwardQueue_.pendingRecords())+" pending":"";
    worker.release(HttpsWorker::Owner::Telemetry);operation_=Operation::None;
    refreshQueueOldest();
  }
  if(batchAcknowledged_) {
    // Spread durable head updates over main-loop turns so capture/UI can run.
    if(batchAckIndex_<batchRecords_.size()) {
      if(!storeForwardQueue_.popIfMatches(batchRecords_[batchAckIndex_])){lastError_="Batch queue acknowledgement persistence failed";return;}
      StaticJsonDocument<96> filter;filter["timestamp"]=true;StaticJsonDocument<192> stamp;
      if(!deserializeJson(stamp,batchRecords_[batchAckIndex_],DeserializationOption::Filter(filter)))performance_.lastSampleEpoch=QueueAge::epoch(stamp["timestamp"]|"");
      ++batchAckIndex_;return;
    }
    batchAcknowledged_=false;batchRecords_.clear();batchPayload_="";batchId_="";
    refreshQueueOldest();
  }
  if(authorization_ && authorization_->restartRequired())return;
  if(WiFi.status()!=WL_CONNECTED){httpsConnected_=false;httpsRecoveryPending_=true;lastError_="Wi-Fi disconnected";return;}
  if(remoteManagementEnabled_ && uint32_t(now-pairingCodeGeneratedMs_)>=600000){rotatePairingCode(now);statusRequested_=true;}
  const bool hasSnapshot=storeForwardQueue_.pendingRecords()>0 || !volatilePayload_.isEmpty();
  const auto work=pacing_.next(now,!worker.idle(),WiFi.status()==WL_CONNECTED,backoff_,completedMs_,config_.reconnectIntervalMs,
                               statusRequested_||fallbackRequested_,hasSnapshot);
  if(work==HttpsPacing::Work::Wait)return;
  if(work==HttpsPacing::Work::Status) {
    if(fallbackRequested_ && authorization_)authorization_->beginRotationFallback();
    Operation next=fallbackRequested_?Operation::Fallback:Operation::Status;
    if(!fallbackRequested_ && !authorization_ && bearerRotation_ && bearerRotation_->hasCandidate())
      next=bearerRotation_->phase()==AppBearerRotation::Phase::Staged?Operation::RotationAck:Operation::RotationProof;
    if(!submitHttps(next,buildStatusJson(true)) && fallbackRequested_ && authorization_)
      authorization_->finishRotationFallback(false);
    return;
  }
  if(!batchPayload_.isEmpty()){submitHttps(Operation::Batch,batchPayload_);return;}
  if(batchEnabled_ && storeForwardQueue_.pendingRecords()>1) {
    if(storeForwardQueue_.peekBatch(batchRecords_,8,HttpsExchange::kBodyLimit-256) && batchRecords_.size()>1) {
      char id[33];snprintf(id,sizeof(id),"%08lx%08lx%08lx%08lx",static_cast<unsigned long>(esp_random()),static_cast<unsigned long>(esp_random()),static_cast<unsigned long>(esp_random()),static_cast<unsigned long>(esp_random()));
      batchId_=id;batchPayload_="{\"schema_version\":1,\"device_id\":\""+jsonEscape(deviceId_)+"\",\"batch_id\":\""+batchId_+"\",\"snapshots\":[";
      for(size_t i=0;i<batchRecords_.size();++i){if(i)batchPayload_+=',';batchPayload_+=batchRecords_[i];}
      batchPayload_+="]}";
      if(batchPayload_.length()<=HttpsExchange::kBodyLimit){submitHttps(Operation::Batch,batchPayload_);return;}
      lastError_="Batch exceeds bounded request size";batchPayload_="";batchRecords_.clear();
    }
  }
  durableInFlight_=storeForwardQueue_.pendingRecords()>0;
  if(durableInFlight_){if(!storeForwardQueue_.peek(inFlightPayload_)){lastError_=storeForwardQueue_.lastError();return;}}
  else inFlightPayload_=volatilePayload_;
  if(!submitHttps(Operation::Snapshot,inFlightPayload_))inFlightPayload_="";
}
void LiveUpload::refreshQueueOldest() {
  queueOldestSession_ = "";
  queueOldestTimestamp_ = "";
  queueOldestSequence_ = 0;
  queueOldestEpoch_ = 0;
  String payload;
  if (!storeForwardQueue_.isReady() || !storeForwardQueue_.peek(payload)) return;
  StaticJsonDocument<192> filter;
  filter["session_id"] = true;
  filter["sequence"] = true;
  filter["timestamp"] = true;
  StaticJsonDocument<512> metadata;
  if (deserializeJson(metadata, payload, DeserializationOption::Filter(filter)) != DeserializationError::Ok ||
      !metadata["session_id"].is<const char *>() || !metadata["sequence"].is<uint32_t>() ||
      !metadata["timestamp"].is<const char *>()) return;
  const char *session = metadata["session_id"];
  const char *timestamp = metadata["timestamp"];
  if (strlen(session) == 0 || strlen(session) > 128 ||
      Logic::normalizeTopicSegment(session) != session || strlen(timestamp) > 32) return;
  queueOldestSession_ = session;
  queueOldestSequence_ = metadata["sequence"];
  queueOldestTimestamp_ = timestamp;
  // Transport timestamps are UTC. Never interpret the uptime fallback or
  // malformed dates as wall time, and never apply the user's local timezone.
  queueOldestEpoch_ = QueueAge::epoch(timestamp);
}

#endif

String LiveUpload::queueOldestDiagnostics() const {
#if defined(ESP32)
  if (queueOldestSession_.isEmpty()) return "null";
  String json = "{\"session_id\":\"" + jsonEscape(queueOldestSession_) + "\",\"sequence\":" +
                String(queueOldestSequence_) + ",\"timestamp\":\"" + jsonEscape(queueOldestTimestamp_) + "\",\"age_seconds\":";
  const time_t now = time(nullptr);
  uint32_t age = 0;
  json += QueueAge::age(queueOldestEpoch_, now, age) ? String(age) : "null";
  return json + "}";
#else
  return "null";
#endif
}

bool LiveUpload::hasAuthenticatedHeartbeat() const {
  return config_.protocol == AppConfig::UploadConfig::Protocol::Https &&
         authenticatedHeartbeatObserved_ && httpsConnected_ &&
         uint32_t(millis() - lastAuthenticatedHeartbeatMs_) < 60000;
}
