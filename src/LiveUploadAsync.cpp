#include "LiveUpload.h"
#if defined(ESP32)
#include "LoggerAuthorization.h"
#include "UploadAcknowledgement.h"
#include "Logic.h"
#include "QueueAge.h"
#include "BatchAcknowledgement.h"
#include <esp_system.h>
#include <WiFi.h>

bool LiveUpload::captureHttps(const AppState &state) {
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
  return true;
}

bool LiveUpload::submitHttps(Operation operation,const String &payload) {
  const bool status=operation!=Operation::Snapshot && operation!=Operation::Batch;
  const char *token=authorization_?authorization_->bearer(status):config_.appDeviceToken;
  if(!token || !token[0]){lastError_="Device authorization required";return false;}
  const String url="https://"+String(config_.mqttHost)+":"+String(config_.mqttPort)+String(config_.httpsPath)+(status?"/status":operation==Operation::Batch?"/snapshot-batch":"/snapshot");
  if(!HttpsWorker::shared().submit(HttpsWorker::Owner::Telemetry,url.c_str(),payload.c_str(),token,
       config_.cloudflareAccessClientId,config_.cloudflareAccessClientSecret))return false;
  operation_=operation;lastHttpsAttemptMs_=millis();
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
          String head;
          if(storeForwardQueue_.peek(head) && head==inFlightPayload_ && !storeForwardQueue_.pop()) {
            lastError_="Queue acknowledgement persistence failed";backoff_=true;
          }
        } else if(volatilePayload_==inFlightPayload_)volatilePayload_="";
      }
      inFlightPayload_="";
    } else {
      pacing_.statusCompleted(now);statusRequested_=false;
      if(operation_==Operation::Fallback) {
        if(authorization_)authorization_->finishRotationFallback(accepted);
        fallbackRequested_=false;
      } else if(!accepted && authorization_ && authorization_->hasRotation() && result->status>=400 && result->status<500 && !Logic::isRetryableHttpStatus(result->status)) {
        fallbackRequested_=true;
      } else if(authorization_)authorization_->statusResult(accepted);
      if(accepted){lastStatusPublishMs_=now;consumeHttpsDesiredConfig(result->body);}
      // A staged rotation progresses without waiting a full heartbeat interval.
      if(accepted && authorization_ && authorization_->hasRotation())statusRequested_=true;
    }
    httpsConnected_=accepted;
    if(!accepted)lastError_="HTTPS request failed ("+String(result->status)+"); queued data retained";
    else if(!backoff_)lastError_=storeForwardQueue_.pendingRecords()?"Replaying onboard queue: "+String(storeForwardQueue_.pendingRecords())+" pending":"";
    worker.release(HttpsWorker::Owner::Telemetry);operation_=Operation::None;
  }
  if(batchAcknowledged_) {
    // Spread durable head updates over main-loop turns so capture/UI can run.
    if(batchAckIndex_<batchRecords_.size()) {
      String head;
      if(storeForwardQueue_.peek(head) && head==batchRecords_[batchAckIndex_]) {
        if(!storeForwardQueue_.pop()){lastError_="Batch queue acknowledgement persistence failed";return;}
      }
      StaticJsonDocument<96> filter;filter["timestamp"]=true;StaticJsonDocument<192> stamp;
      if(!deserializeJson(stamp,batchRecords_[batchAckIndex_],DeserializationOption::Filter(filter)))performance_.lastSampleEpoch=QueueAge::epoch(stamp["timestamp"]|"");
      ++batchAckIndex_;return;
    }
    batchAcknowledged_=false;batchRecords_.clear();batchPayload_="";batchId_="";
  }
  if(authorization_ && authorization_->restartRequired())return;
  if(remoteManagementEnabled_ && uint32_t(now-pairingCodeGeneratedMs_)>=600000){rotatePairingCode(now);statusRequested_=true;}
  const bool hasSnapshot=storeForwardQueue_.pendingRecords()>0 || !volatilePayload_.isEmpty();
  const auto work=pacing_.next(now,!worker.idle(),WiFi.status()==WL_CONNECTED,backoff_,completedMs_,config_.reconnectIntervalMs,
                               statusRequested_||fallbackRequested_,hasSnapshot);
  if(work==HttpsPacing::Work::Wait)return;
  if(work==HttpsPacing::Work::Status) {
    if(fallbackRequested_ && authorization_)authorization_->beginRotationFallback();
    if(!submitHttps(fallbackRequested_?Operation::Fallback:Operation::Status,buildStatusJson(true)) && fallbackRequested_ && authorization_)
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
#endif
