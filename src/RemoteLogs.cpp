#include "RemoteLogs.h"
#include "HttpsSlot.h"
#include "HttpsWorker.h"
#include <memory>
#if defined(ESP32) && !defined(MDA_WAVESHARE_DASH)
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "LiveUpload.h"
#include "SystemLog.h"
#include "AppBearerRotation.h"
void RemoteLogs::begin(const AppConfig::UploadConfig &config) {
  ready_=false;
  config_=config;
  host_=config.mqttHost; path_=config.httpsPath; identity_=config.deviceId;
  accessId_=config.cloudflareAccessClientId; accessSecret_=config.cloudflareAccessClientSecret; token_=config.appDeviceToken;
  config_.mqttHost=host_.c_str(); config_.httpsPath=path_.c_str(); config_.deviceId=identity_.c_str();
  config_.cloudflareAccessClientId=accessId_.c_str(); config_.cloudflareAccessClientSecret=accessSecret_.c_str(); config_.appDeviceToken=token_.c_str();
  const char *bearer=bearerRotation_?bearerRotation_->activeBearer():token_.c_str();
  if(config.protocol!=AppConfig::UploadConfig::Protocol::Https || !bearer || !bearer[0]) return;
  ready_=HttpsWorker::shared().begin(LiveUpload::trustRoot());
}
void RemoteLogs::loop(bool enabled) {
  if(!ready_)return;
  if(state_==1){
    const auto *result=HttpsWorker::shared().result(HttpsWorker::Owner::Logs);
    if(!result)return;
    success_=result->status==200;response_=success_?String(result->body):String("");
    HttpsWorker::shared().release(HttpsWorker::Owner::Logs);state_=2;
  }
  if(!enabled) { HttpsWorker::shared().cancelPending(HttpsWorker::Owner::Logs);file_.close(); job_=""; request_=""; response_=""; state_=0; return; }
  if(state_==2) {
    if(failed_==success_) { systemLog.add(success_?"remote_logs_recovered":"remote_logs_unavailable",0,success_?1:2); failed_=!success_; }
    // Unsupported/unavailable log endpoints must not starve telemetry TLS.
    interval_=success_?5000:60000;
    if(success_) {
      DynamicJsonDocument response(2048);
      if(deserializeJson(response,response_)!=DeserializationError::Ok) { state_=0;interval_=60000;return; }
      DynamicJsonDocument out(24576);
      out["device_id"]=config_.deviceId; out["enabled"]=true;
      const JsonObject cmd=response["command"];
      if(cmd.isNull()) { file_.close(); job_=""; interval_=15000; }
      else {
        interval_=500;
        const String id=cmd["id"] | "";
        out["id"]=id;
        if(cmd["kind"]=="refresh") {
          DynamicJsonDocument files(24576); deserializeJson(files,systemLog.filesJson()); out["files"]=files;
        } else if(cmd["kind"]=="download") {
          if(job_!=id) {
            file_.close(); job_=id;
            file_=systemLog.open(cmd["name"] | ""); size_=file_?file_.size():0;
          }
          const uint32_t offset=cmd["offset"] | 0U;
          // If firmware rebooted mid-transfer, fail instead of silently mixing snapshots.
          const bool changed=!cmd["size"].isNull() && cmd["size"].as<uint32_t>()!=size_;
          if(!file_ || changed || size_>16*1024*1024 || offset>size_ || file_.size()<size_ || !file_.seek(offset)) out["error"]="read_failed";
          else {
            std::unique_ptr<uint8_t[]> bytes(new(std::nothrow) uint8_t[2048]); const size_t length=std::min(uint32_t(2048),size_-offset);
            if(!bytes || file_.read(bytes.get(),length)!=int(length)) out["error"]="read_failed";
            else {
              String hex; hex.reserve(length*2); constexpr char digits[]="0123456789abcdef";
              for(size_t i=0;i<length;++i) { hex+=digits[bytes[i]>>4]; hex+=digits[bytes[i]&15]; }
              out["offset"]=offset; out["size"]=size_; out["hex"]=hex;
            }
          }
        } else out["error"]="unsupported";
      }
      request_=""; serializeJson(out,request_);
    }
    state_=0;
  }
  if(WiFi.status()!=WL_CONNECTED || uint32_t(millis()-lastRequest_)<interval_) return;
  if(request_.isEmpty()) {
    StaticJsonDocument<256> out; out["device_id"]=config_.deviceId; out["enabled"]=true; serializeJson(out,request_);
  }
  const String url="https://"+host_+":"+String(config_.mqttPort)+path_+"/logs";
  // Resolve at submission, not boot: rotation must not strand remote logs on
  // a revoked bootstrap token. The shared worker copies this immutable request.
  const char *bearer=bearerRotation_?bearerRotation_->activeBearer():token_.c_str();
  if(!bearer || !bearer[0]){HttpsWorker::shared().cancelPending(HttpsWorker::Owner::Logs);return;}
  if(HttpsWorker::shared().submit(HttpsWorker::Owner::Logs,url.c_str(),request_.c_str(),bearer,accessId_.c_str(),accessSecret_.c_str())){
    lastRequest_=millis();state_=1;
  }
}
#else
void RemoteLogs::begin(const AppConfig::UploadConfig &) {}
void RemoteLogs::loop(bool) {}
#endif
