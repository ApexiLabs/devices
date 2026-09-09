#include "RemoteLogs.h"
#if defined(ESP32) && !defined(MDA_WAVESHARE_DASH)
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "LiveUpload.h"
#include "SystemLog.h"
void RemoteLogs::begin(const AppConfig::UploadConfig &config) {
  config_=config;
  host_=config.mqttHost; path_=config.httpsPath; identity_=config.deviceId;
  accessId_=config.cloudflareAccessClientId; accessSecret_=config.cloudflareAccessClientSecret; token_=config.appDeviceToken;
  config_.mqttHost=host_.c_str(); config_.httpsPath=path_.c_str(); config_.deviceId=identity_.c_str();
  config_.cloudflareAccessClientId=accessId_.c_str(); config_.cloudflareAccessClientSecret=accessSecret_.c_str(); config_.appDeviceToken=token_.c_str();
  if(config.protocol!=AppConfig::UploadConfig::Protocol::Https) return;
  if(xTaskCreate(worker,"remote-logs",8192,this,1,&task_)!=pdPASS) task_=nullptr;
}
void RemoteLogs::worker(void *context) {
  auto &self=*static_cast<RemoteLogs *>(context);
  for(;;) {
    ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
    WiFiClientSecure client; client.setCACert(LiveUpload::trustRoot());
    HTTPClient http; http.setTimeout(3000); http.setConnectTimeout(3000);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    http.setUserAgent("ApexiLabs-Logger/1.0");
    const auto &c=self.config_;
    String url="https://"+String(c.mqttHost)+":"+String(c.mqttPort)+String(c.httpsPath)+"/logs";
    self.success_=false; self.response_="";
    if(http.begin(client,url)) {
      http.addHeader("Content-Type","application/json");
      http.addHeader("Authorization","Bearer "+String(c.appDeviceToken));
      http.addHeader("CF-Access-Client-Id",c.cloudflareAccessClientId);
      http.addHeader("CF-Access-Client-Secret",c.cloudflareAccessClientSecret);
      const int code=http.POST(self.request_);
      if(code==200 && http.getSize()>=0 && http.getSize()<=2048) {
        self.response_=http.getString(); self.success_=true;
      }
      http.end();
    }
    self.state_.store(2);
  }
}
void RemoteLogs::loop(bool enabled) {
  if(!task_ || state_.load()==1) return;
  if(!enabled) { file_.close(); job_=""; request_=""; response_=""; state_.store(0); return; }
  if(state_.load()==2) {
    if(failed_==success_) { systemLog.add(success_?"remote_logs_recovered":"remote_logs_unavailable",0,success_?1:2); failed_=!success_; }
    interval_=5000;
    if(success_) {
      StaticJsonDocument<2048> response;
      if(deserializeJson(response,response_)!=DeserializationError::Ok) { state_.store(0); return; }
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
            uint8_t bytes[2048]; const size_t length=std::min(uint32_t(sizeof(bytes)),size_-offset);
            if(file_.read(bytes,length)!=int(length)) out["error"]="read_failed";
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
    state_.store(0);
  }
  if(WiFi.status()!=WL_CONNECTED || uint32_t(millis()-lastRequest_)<interval_) return;
  if(request_.isEmpty()) {
    StaticJsonDocument<256> out; out["device_id"]=config_.deviceId; out["enabled"]=true; serializeJson(out,request_);
  }
  lastRequest_=millis(); state_.store(1); xTaskNotifyGive(task_);
}
#else
void RemoteLogs::begin(const AppConfig::UploadConfig &) {}
void RemoteLogs::loop(bool) {}
#endif
