#include "SystemLog.h"
#if !defined(MDA_WAVESHARE_DASH)
SystemLog systemLog;
void SystemLog::begin(Timekeeper &time,const char *identity) {
  time_=&time;
  identity_=identity;
  boot_=micros() ^ uint32_t(ESP.getFreeHeap());
#if defined(ESP32)
  boot_=esp_random();
#endif
  add("boot");
}
void SystemLog::add(const char *code,uint32_t value,uint8_t severity) {
  if(!SystemEvents::safeCode(code)) return;
  SystemEvents::Event e{boot_,++seq_,millis(),value,severity,{}};
  strlcpy(e.code,code,sizeof(e.code));
  if(pending_.push(e)) remember(e,"logger");
}
void SystemLog::remember(const SystemEvents::Event &e,const String &device) {
  StaticJsonDocument<512> doc;
  doc["received_at"]=time_?time_->transportTimestamp(millis()):String("unsynced");
  doc["device"]=device; doc["boot"]=e.boot; doc["seq"]=e.seq;
  doc["logger_id"]=identity_;
  doc["uptime_ms"]=e.ms; doc["severity"]=e.severity==2?"WARN":e.severity==3?"ERROR":"INFO";
  doc["event"]=e.code; doc["value"]=e.value;
  if(e.httpStatus) { doc["http_status"]=e.httpStatus; doc["method"]=e.method; doc["duration_ms"]=e.durationMs; }
  String line; serializeJson(doc,line);
  devices_[pushed_%64]=device; lines_[pushed_++%64]=line;
  recent_[recentCount_++%64]=line;
}
void SystemLog::http(const char *route,uint8_t method,uint16_t status,uint32_t duration) {
  if(!detailedRequests() && lastHttpCode_==route && lastHttpStatus_==status && uint32_t(millis()-lastHttpMs_)<10000) { ++httpRepeated_; return; }
  if(httpRepeated_) { add("http_repeat_summary",httpRepeated_); httpRepeated_=0; }
  lastHttpCode_=route; lastHttpStatus_=status; lastHttpMs_=millis();
  SystemEvents::Event e{}; e.boot=boot_; e.seq=++seq_; e.ms=millis();
  strlcpy(e.code,route,sizeof(e.code)); e.method=method; e.httpStatus=status; e.durationMs=duration; e.severity=status>=400?2:1;
  if(pending_.push(e)) remember(e,"logger");
}
bool SystemLog::acceptDash(const SystemEvents::Event &e,const String &device) {
  if(e.version!=1 || !SystemEvents::safeCode(e.code) || !e.seq) return false;
  if(!ready_) return false;
  if(device!=dashDevice_) {
    // Recover the last durable acknowledgement from log tails after a logger
    // reboot. A lost BLE acknowledgement must not duplicate a flushed record.
    File root=SD.open("/"); File entry=root.openNextFile();
    while(entry) {
      String name=entry.name(); if(name.startsWith("/")) name.remove(0,1);
      if(!entry.isDirectory() && name.startsWith("dash-"+device+"-") && name.endsWith(".log")) {
        char tail[1025]{}; const size_t length=std::min(size_t(1024),size_t(entry.size()));
        entry.seek(entry.size()-length); const int read=entry.read(reinterpret_cast<uint8_t *>(tail),length);
        if(read>0) {
          tail[read]=0; String text(tail);
          const int end=text.lastIndexOf('\n');
          const int start=end>0?text.lastIndexOf('\n',end-1):-1;
          if(end>0) {
            StaticJsonDocument<512> last;
            if(!deserializeJson(last,text.substring(start+1,end)) && last["boot"].as<uint32_t>()==e.boot) {
              dashBoot_=e.boot; dashSeq_=std::max(dashSeq_,last["seq"].as<uint32_t>());
            }
          }
        }
      }
      entry.close(); entry=root.openNextFile();
    }
    dashDevice_=device;
  }
  if(device==dashDevice_ && e.boot==dashBoot_ && e.seq<=dashSeq_) return true;
  if(dashPendingSeq_) return false;
  if(!pending_.push(e)) return false;
  remember(e,"dash-"+device);
  dashDevice_=device; dashPendingBoot_=e.boot; dashPendingSeq_=e.seq;
  return false; // Acknowledge only after SD flush, not merely RAM acceptance.
}
void SystemLog::loop(Timekeeper &time,bool sdReady) {
  ready_=sdReady;
  if(uint32_t(millis()-lastDrain_)<250) return;
  lastDrain_=millis();
  const auto *e=pending_.front();
  if(!e) return;
  const String device=devices_[popped_%64];
  const String &line=lines_[popped_%64];
  // RAM history remains useful when no card is fitted. The fallback is bounded.
  if(!ready_) { error_="microSD unavailable; events buffered in RAM"; return; }
  const unsigned source=device.startsWith("dash-")?1:0;
  const String prefix=device+"-"+time.dateStamp()+"-";
  if(!active_[source].startsWith("/"+prefix)) segment_[source]=0;
  char suffix[16]; snprintf(suffix,sizeof(suffix),"%08lu",(unsigned long)segment_[source]);
  String path="/"+prefix+suffix+".log";
  File file=SD.open(path,"a");
  if(file && file.size()>=256*1024) {
    file.close(); ++segment_[source]; active_[source]=path; return;
  }
  if(!file) { error_="System log open failed"; return; }
  const String output=line+"\n";
  if(file.print(output)!=output.length()) { file.close(); error_="System log write failed"; return; }
  error_="";
  file.flush(); file.close(); const bool rotated=active_[source]!=path; active_[source]=path;
  if(source) { dashBoot_=dashPendingBoot_; dashSeq_=dashPendingSeq_; dashPendingSeq_=0; }
  pending_.pop(); lines_[popped_%64]=""; ++popped_;
  // At most 16 segments per source (~4 MiB). CSV files are never pruned.
  if(!rotated) return;
  File root=SD.open("/"); File entry=root.openNextFile();
  unsigned count=0; String oldest;
  while(entry) {
    String name=entry.name(); if(name.startsWith("/")) name.remove(0,1);
    if(!entry.isDirectory() && name.startsWith(device+"-") && name.endsWith(".log") && SystemEvents::safeName(name.c_str())) {
      ++count; if("/"+name!=path && (oldest.isEmpty() || name<oldest)) oldest=name;
    }
    entry.close(); entry=root.openNextFile();
  }
  if(count>16 && !oldest.isEmpty()) SD.remove("/"+oldest);
}
String SystemLog::recentJson() const {
  DynamicJsonDocument doc(32768); doc["pending"]=pending_.size(); doc["dropped"]=pending_.dropped; doc["sd_ready"]=ready_ && error_.isEmpty();
  doc["error"]=error_;
  doc["detailed_requests"]=detailedRequests();
  auto events=doc["events"].to<JsonArray>();
  const unsigned count=recentCount_<64?recentCount_:64;
  for(unsigned i=0;i<count;++i) {
    StaticJsonDocument<512> e; deserializeJson(e,recent_[(recentCount_-count+i)%64]); events.add(e.as<JsonObject>());
  }
  String out; serializeJson(doc,out); return out;
}
String SystemLog::filesJson() const {
  DynamicJsonDocument doc(24576); auto files=doc.to<JsonArray>();
  if(ready_) {
    File root=SD.open("/"); File entry=root.openNextFile();
    while(entry && files.size()<128) {
      String name=entry.name(); if(name.startsWith("/")) name.remove(0,1);
      if(!entry.isDirectory() && SystemEvents::safeName(name.c_str())) { auto f=files.createNestedObject(); f["name"]=name; f["size"]=entry.size(); }
      entry.close(); entry=root.openNextFile();
    }
  }
  String out; serializeJson(doc,out); return out;
}
File SystemLog::open(const String &name) const {
  if(!ready_ || !SystemEvents::safeName(name.c_str())) return File();
  return SD.open("/"+name,FILE_READ);
}
#endif
