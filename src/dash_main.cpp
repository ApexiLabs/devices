#if defined(MDA_WAVESHARE_DASH)

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLE2902.h>
#include <TFT_eSPI.h>
#include <WebServer.h>
#include <WiFi.h>

#include "DashLinkProtocol.h"
#include "AppConfig.h"
#include "DashWifiPolicy.h"
#include "DashWebUi.h"
#include "DashTelemetry.h"
#include "DashDisplayLayout.h"
#include "DashLcdBitmap.h"
#include "DashDiagnostics.h"
#include "SystemEvents.h"

namespace {

constexpr char kAccessPointSsid[] = "APEXI-DASH";
constexpr char kAccessPointPassword[] = "apexi-dash";

TFT_eSPI display;
TFT_eSprite frame(&display);
bool lcdBuffered = false;
String lastFrameKey;
uint32_t lcdFrameCount = 0;
String lcdBootId;
uint32_t lastLcdSnapshotMs = 0;
bool lcdSnapshotServed = false;
WebServer webServer(80);
BLEAdvertising *advertising = nullptr;

volatile bool bleConnected = false;
volatile bool loggerReady = false;
volatile bool restartAdvertising = false;
bool renderedLoggerReady = false;
bool renderedBleConnected = false;
bool wifiConnected = false;
String stationIp;
uint32_t lastWifiAttemptMs = 0;
bool otaReady = false;
constexpr bool otaEnabled = AppConfig::kOta.password[0] != '\0';
Preferences preferences;
struct DisplaySettings {
  uint32_t magic = 0x44534831;
  uint32_t refreshMs = 1000;
  char slots[2][16]{}; // Empty = automatic, '-' = hidden, otherwise stable sensor ID.
} settings;
bool storageReady = false;
String csrfToken;
uint32_t lastRenderMs = 0;
portMUX_TYPE telemetryMux = portMUX_INITIALIZER_UNLOCKED;
DashTelemetry::Model telemetry;
struct ReceiveStats {
  uint32_t writes=0,accepted=0,rejected=0,connections=0,disconnects=0;
  uint32_t samples[8]{},lastSampleMs[8]{},maxGapMs[8]{};
} receiveStats;
DashDiagnostics::Log diagnosticLog;
SystemEvents::Queue<64> systemEvents;
uint32_t eventBoot=0,eventSeq=0;
portMUX_TYPE eventMux=portMUX_INITIALIZER_UNLOCKED;
void dashEvent(const char *code,uint32_t value=0,uint8_t severity=1) {
  SystemEvents::Event e{};
  e.boot=eventBoot; e.ms=millis(); e.value=value; e.severity=severity;
  strlcpy(e.code,code,sizeof(e.code));
  portENTER_CRITICAL(&eventMux); e.seq=++eventSeq; systemEvents.push(e); portEXIT_CRITICAL(&eventMux);
}
class EventCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) override {
    const auto raw=c->getValue(); if(raw.length()!=8) return;
    uint32_t ack[2]; memcpy(ack,raw.c_str(),8);
    portENTER_CRITICAL(&eventMux);
    const auto *e=systemEvents.front(); if(e && e->boot==ack[0] && e->seq==ack[1]) systemEvents.pop();
    portEXIT_CRITICAL(&eventMux);
  }
} eventCallbacks;
BLECharacteristic *eventCharacteristic=nullptr;
void serviceSystemEvents() {
  if(!bleConnected || !loggerReady || !eventCharacteristic) return;
  static uint32_t last=0; static uint8_t part=0;
  static uint32_t previousSeq=0;
  if(uint32_t(millis()-last)<(part?40U:1000U)) return;
  last=millis();
  SystemEvents::Event event{}; bool available;
  portENTER_CRITICAL(&eventMux); available=systemEvents.front()!=nullptr; if(available) event=*systemEvents.front(); portEXIT_CRITICAL(&eventMux);
  if(!available) return;
  if(previousSeq!=event.seq) { part=0; previousSeq=event.seq; }
  auto packet=SystemEvents::frame(event,part);
  eventCharacteristic->setValue(packet.data(),packet.size()); eventCharacteristic->notify();
  part=(part+1)%4;
}
uint8_t lastSensorStates[8]={255,255,255,255,255,255,255,255};
uint32_t maxLoopGapMs=0,maxWebMs=0,lastLoopMs=0;
uint32_t renderClockRaces=0;
DashTelemetry::Model snapshotTelemetry();

void observeDiagnostics(){
  const auto model=snapshotTelemetry();
  const uint32_t now=millis();
  const bool connected=bleConnected && loggerReady;
  for(unsigned i=0;i<8;++i){
    const auto &s=model.sensors[i];
    const auto state=DashDiagnostics::state(s,connected,now);
    if(state!=lastSensorStates[i] && (i<model.count || lastSensorStates[i]!=255)){
      diagnosticLog.add(now,i,state,s.received?uint32_t(now-s.receivedMs):0);
      dashEvent(DashDiagnostics::name(state),i,state==5?1:2);
      Serial0.printf("DASH_DIAG ms=%lu sensor=%u state=%s ageMs=%lu\n",(unsigned long)now,i,DashDiagnostics::name(state),(unsigned long)(s.received?uint32_t(now-s.receivedMs):0));
      lastSensorStates[i]=state;
    }
  }
}

bool validSlot(const char *id) {
  if (strnlen(id, 16) == 16) return false;
  for (const char *p = id; *p; ++p)
    if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
          (*p >= '0' && *p <= '9') || *p == '_' || *p == '-')) return false;
  return true;
}
void clearTelemetry() {
  portENTER_CRITICAL(&telemetryMux);
  for(auto &s:telemetry.sensors){s.received=false;s.valid=false;}
  portEXIT_CRITICAL(&telemetryMux);
}
DashTelemetry::Model snapshotTelemetry() {
  portENTER_CRITICAL(&telemetryMux); auto copy = telemetry; portEXIT_CRITICAL(&telemetryMux);
  return copy;
}
const DashTelemetry::Sensor *selectedSensor(const DashTelemetry::Model &m, size_t slot) {
  const char *id = settings.slots[slot];
  if (!id[0]) return slot < m.count && m.sensors[slot].metadata == 7 ? &m.sensors[slot] : nullptr;
  for (size_t i=0;i<m.count;++i)
    if (m.sensors[i].metadata == 7 && !strcmp(id,m.sensors[i].id)) return &m.sensors[i];
  return nullptr;
}

class ServerCallbacks : public BLEServerCallbacks {
 public:
  void onConnect(BLEServer *) override {
    dashEvent("logger_connected");
    portENTER_CRITICAL(&telemetryMux); ++receiveStats.connections; portEXIT_CRITICAL(&telemetryMux);
    clearTelemetry();
    bleConnected = true;
    loggerReady = false;
  }

  void onDisconnect(BLEServer *) override {
    dashEvent("logger_disconnected",0,2);
    portENTER_CRITICAL(&telemetryMux); ++receiveStats.disconnects; portEXIT_CRITICAL(&telemetryMux);
    clearTelemetry();
    bleConnected = false;
    loggerReady = false;
    restartAdvertising = true;
  }
};

class HandshakeCallbacks : public BLECharacteristicCallbacks {
 public:
  void onWrite(BLECharacteristic *characteristic) override {
    loggerReady = characteristic->getValue() == DashLinkProtocol::kHandshake;
  }
};

ServerCallbacks serverCallbacks;
HandshakeCallbacks handshakeCallbacks;
class TelemetryCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) override {
    const String value = characteristic->getValue();
    DashTelemetry::Frame f{};
    if(value.length()==f.size())memcpy(f.data(), value.c_str(), f.size());
    const uint32_t now = millis();
    portENTER_CRITICAL(&telemetryMux);
    ++receiveStats.writes;
    if(loggerReady && value.length()==f.size() && telemetry.accept(f,now)){
      ++receiveStats.accepted;
      if(f[1]==DashTelemetry::Sample){
        const unsigned i=f[2];
        if(receiveStats.samples[i])receiveStats.maxGapMs[i]=std::max(receiveStats.maxGapMs[i],uint32_t(now-receiveStats.lastSampleMs[i]));
        ++receiveStats.samples[i];receiveStats.lastSampleMs[i]=now;
      }
    }else ++receiveStats.rejected;
    portEXIT_CRITICAL(&telemetryMux);
  }
} telemetryCallbacks;

void pushFrame() { frame.pushSprite(0,0); ++lcdFrameCount; }

String fitCaption(String text,int width) {
  if(frame.textWidth(text,2)<=width)return text;
  while(text.length() && frame.textWidth(text+"...",2)>width)text.remove(text.length()-1);
  return text+"...";
}

void renderStatus() {
  const bool ready=loggerReady, link=bleConnected;
  const uint32_t now=millis();
  lastRenderMs=now; renderedLoggerReady=ready; renderedBleConnected=link;
  if (!lcdBuffered) return;
  const auto readings=snapshotTelemetry();
  String labels[2], values[2], details[2];
  uint16_t colours[2];
  uint16_t accents[2];
  unsigned count=0;
  String key=String(ready)+":"+String(link);
  if (ready || readings.count) {
    for (size_t slot=0;slot<2;++slot) {
      if (!strcmp(settings.slots[slot],"-")) continue;
      const auto *s=selectedSensor(readings,slot);
      if(s && s->received && uint32_t(now-s->receivedMs)>0x80000000U)++renderClockRaces;
      const bool fresh=s && s->fresh(link && ready,now);
      const bool valid=fresh && s->valid && s->fault==SensorFault::None;
      labels[count]=s ? s->name : "Waiting for sensor";
      values[count]=valid ? String(s->value,1) : s && s->hasLastGood ? String(s->lastGoodValue,1) : String("--");
      details[count]=valid ? String(s->units) : !s ? "No data" : !link || !ready ? "Disconnected" : !fresh ? "Stale" : "Sensor fault";
      colours[count]=valid && !s->warning ? TFT_WHITE : TFT_ORANGE;
      accents[count]=slot==0 ? 0x05FF : 0xFEA0; // Cyan / warm gold: slot identity, not a scale.
      key += "|"+labels[count]+"|"+values[count]+"|"+details[count]+"|"+String(colours[count])+"|"+String(accents[count]);
      ++count;
    }
  }
  // Only visible changes require an LCD transfer; timestamps and polling do not.
  if (key==lastFrameKey) return;
  lastFrameKey=key;
  frame.fillSprite(TFT_BLACK);
  frame.setTextDatum(MC_DATUM); frame.setTextSize(1);
  if (count) {
    // Reference direction: black instrument face, perimeter accents, large digits.
    // Arcs are fixed identity marks; no invented sensor min/max or progress fill.
    frame.drawCircle(120,120,115,0x2104);
    for (unsigned i=0;i<count;++i) {
      const auto layout=DashDisplayLayout::row(count,i);
      frame.drawSmoothArc(120,120,110,106,count==1?45:i==0?105:285,
                          count==1?315:i==0?255:359,accents[i],TFT_BLACK,true);
      if(count==2 && i==1)frame.drawSmoothArc(120,120,110,106,0,75,accents[i],TFT_BLACK,true);
      frame.setTextSize(1); frame.setTextColor(accents[i],TFT_BLACK);
      frame.drawString(fitCaption(labels[i],DashDisplayLayout::labelWidth),120,layout.labelY,2);
      frame.setTextColor(colours[i],TFT_BLACK);
      frame.setTextSize(layout.scale);
      int font=6; // 48-pixel digits; up to 96 pixels in single-reading mode.
      const int maxWidth=DashDisplayLayout::valueWidth(count);
      if (frame.textWidth(values[i],font)>maxWidth) frame.setTextSize(1);
      if (frame.textWidth(values[i],font)>maxWidth) font=4;
      if (frame.textWidth(values[i],font)>maxWidth) { values[i]="Out of range"; font=2; }
      frame.drawString(values[i],120,layout.valueY,font);
      frame.setTextSize(1); frame.setTextColor(colours[i]==TFT_WHITE?TFT_LIGHTGREY:TFT_ORANGE,TFT_BLACK);
      frame.drawString(fitCaption(details[i],DashDisplayLayout::detailWidth),120,layout.detailY,2);
    }
  } else {
    frame.setTextColor(TFT_WHITE,TFT_BLACK);
    frame.drawString(ready ? "NO READINGS" : link ? "CONNECTING" : "WAITING",120,108,4);
    frame.setTextColor(TFT_LIGHTGREY,TFT_BLACK);
    frame.drawString(ready ? "Choose a sensor in web UI" : "Waiting for Logger",120,145,2);
  }
  pushFrame();
}

String statusJson() {
  String json = "{\"bleConnected\":";
  json += bleConnected ? "true" : "false";
  json += ",\"loggerReady\":";
  json += loggerReady ? "true" : "false";
  json += ",\"device\":\"";
  json += DashLinkProtocol::kDashDeviceName;
  json += "\",\"ip\":\"";
  json += wifiConnected ? stationIp : WiFi.softAPIP().toString();
  json += "\",\"wifiConnected\":";
  json += wifiConnected ? "true" : "false";
  json += ",\"stationIp\":\"";
  json += stationIp;
  json += "\",\"apIp\":\"";
  json += WiFi.softAPIP().toString();
  json += "\",\"otaEnabled\":";
  json += otaEnabled ? "true" : "false";
  json += ",\"otaReady\":";
  json += otaReady ? "true" : "false";
  json += ",\"build\":\"" __DATE__ " " __TIME__ "\",\"uptimeSeconds\":";
  json += millis() / 1000;
  json += ",\"refreshMs\":" + String(settings.refreshMs);
  json += ",\"lcdBuffered\":"; json += lcdBuffered ? "true" : "false";
  json += ",\"lcdFrameCount\":" + String(lcdFrameCount);
  json += ",\"lcdBootId\":\"" + lcdBootId + "\"";
  json += ",\"lcdValuesShown\":" + String((strcmp(settings.slots[0],"-")!=0)+(strcmp(settings.slots[1],"-")!=0));
  json += ",\"telemetryIntervalMs\":" + String(DashTelemetry::kPublishMs);
  json += ",\"settingsWritable\":";
  json += storageReady && otaEnabled ? "true" : "false";
  json += ",\"slots\":[\"" + String(settings.slots[0]) + "\",\"" + settings.slots[1] + "\"],\"sensors\":";
  StaticJsonDocument<4096> sensorDoc;
  auto array = sensorDoc.to<JsonArray>();
  const auto readings = snapshotTelemetry();
  for (size_t i=0;i<readings.count;++i) {
    const auto &s=readings.sensors[i]; if (s.metadata != 7) continue;
    auto obj=array.createNestedObject();
    obj["id"]=s.id; obj["name"]=s.name; obj["units"]=s.units;
    const bool fresh = s.fresh(bleConnected && loggerReady,millis());
    obj["fresh"]=fresh; obj["valid"]=fresh && s.valid && s.fault == SensorFault::None;
    obj["warning"]=s.warning; obj["fault"]=sensorFaultToString(s.fault);
    obj["displayState"]=!bleConnected || !loggerReady ? "Disconnected" : !fresh ? "Stale" : !s.valid || s.fault!=SensorFault::None ? "Sensor fault" : "Live";
    if(s.hasLastGood){obj["lastGoodValue"]=s.lastGoodValue;obj["lastGoodAgeMs"]=uint32_t(millis()-s.lastGoodMs);}
    else {obj["lastGoodValue"]=nullptr;obj["lastGoodAgeMs"]=nullptr;}
    if (fresh && s.valid && s.fault == SensorFault::None) obj["value"]=s.value;
    else obj["value"] = nullptr;
  }
  serializeJson(array,json);
  json += "}";
  return json;
}

void beginWifi() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.setHostname("apexi-dash");
  WiFi.softAP(kAccessPointSsid, kAccessPointPassword);
  WiFi.setAutoReconnect(true);
  if (AppConfig::kWifi.stationSsid[0] != '\0') {
    WiFi.begin(AppConfig::kWifi.stationSsid, AppConfig::kWifi.stationPassword);
    lastWifiAttemptMs = millis();
    Serial0.println("DASH_WIFI=connecting");
  } else {
    Serial0.println("DASH_WIFI=unconfigured; recovery AP only");
  }
}

void serviceWifi() {
  const bool connected = WiFi.status() == WL_CONNECTED;
  const String ip = connected ? WiFi.localIP().toString() : String();
  if (connected != wifiConnected || ip != stationIp) {
    dashEvent(connected?"wifi_connected":"wifi_disconnected",0,connected?1:2);
    wifiConnected = connected;
    stationIp = ip;
    Serial0.println(connected ? "DASH_WIFI=connected" : "DASH_WIFI=disconnected");
    if (connected) {
      Serial0.print("DASH_STATION_WEB=http://");
      Serial0.println(stationIp);
    }
    renderStatus();
  }
  const uint32_t nowMs = millis();
  if (DashWifiPolicy::shouldRetry(AppConfig::kWifi.stationSsid[0] != '\0',
                                  connected, nowMs, lastWifiAttemptMs)) {
    lastWifiAttemptMs = nowMs;
    Serial0.print("DASH_WIFI=retry; status=");
    Serial0.println(static_cast<int>(WiFi.status()));
    WiFi.disconnect();
    WiFi.begin(AppConfig::kWifi.stationSsid, AppConfig::kWifi.stationPassword);
  }
}

void beginWebUi() {
  webServer.on("/api/diagnostics",HTTP_GET,[](){
    dashEvent("http_get_diagnostics");
    ReceiveStats stats; DashTelemetry::Model model;
    portENTER_CRITICAL(&telemetryMux);stats=receiveStats;model=telemetry;portEXIT_CRITICAL(&telemetryMux);
    const uint32_t now=millis();
    DynamicJsonDocument doc(12288);
    doc["bootId"]=lcdBootId;doc["uptimeMs"]=now;doc["staleThresholdMs"]=DashTelemetry::kStaleMs;
    doc["writes"]=stats.writes;doc["accepted"]=stats.accepted;doc["rejected"]=stats.rejected;
    doc["connections"]=stats.connections;doc["disconnects"]=stats.disconnects;
    doc["maxLoopGapMs"]=maxLoopGapMs;doc["maxWebHandlerMs"]=maxWebMs;
    doc["renderClockRaces"]=renderClockRaces;
    doc["freeHeap"]=ESP.getFreeHeap();doc["wifiRssi"]=WiFi.RSSI();
    doc["eventsTotal"]=diagnosticLog.total;doc["retention"]="Latest 64 state transitions, RAM only; lost on reboot";
    portENTER_CRITICAL(&eventMux); const auto pendingEvents=systemEvents.size(); const auto droppedEvents=systemEvents.dropped; portEXIT_CRITICAL(&eventMux);
    doc["system_events_pending"]=pendingEvents; doc["system_events_dropped"]=droppedEvents;
    auto sensors=doc.createNestedArray("sensors");
    for(unsigned i=0;i<8;++i){if(i>=model.count && !stats.samples[i])continue;
      auto s=sensors.createNestedObject();s["index"]=i;s["id"]=model.sensors[i].id;
      s["sampleCount"]=stats.samples[i];s["maxSampleGapMs"]=stats.maxGapMs[i];
      if(stats.samples[i])s["sampleAgeMs"]=uint32_t(now-stats.lastSampleMs[i]);else s["sampleAgeMs"]=nullptr;
      s["metadataMask"]=model.sensors[i].metadata;s["fault"]=sensorFaultToString(model.sensors[i].fault);
      s["state"]=DashDiagnostics::name(DashDiagnostics::state(model.sensors[i],bleConnected&&loggerReady,now));
    }
    auto events=doc.createNestedArray("events");
    for(unsigned i=0;i<diagnosticLog.size();++i){const auto e=diagnosticLog.at(i);auto o=events.createNestedObject();
      o["uptimeMs"]=e.ms;o["sensorIndex"]=e.sensor;o["state"]=DashDiagnostics::name(e.state);o["sampleAgeMs"]=e.ageMs;
    }
    String body;serializeJson(doc,body);webServer.sendHeader("Cache-Control","no-store");
    webServer.send(200,"application/json",body);
  });
  const char *headers[]={"If-None-Match"};
  webServer.collectHeaders(headers,1);
  webServer.on("/api/lcd.bmp",HTTP_GET,[]() {
    if (!lcdBuffered) { webServer.send(503,"text/plain","LCD buffer unavailable"); return; }
    const String etag=String("\"")+lcdBootId+"-"+String(lcdFrameCount)+"\"";
    webServer.sendHeader("Cache-Control","no-cache");
    webServer.sendHeader("ETag",etag);
    webServer.sendHeader("X-LCD-Frame",String(lcdFrameCount));
    webServer.sendHeader("X-LCD-Boot",lcdBootId);
    if (webServer.header("If-None-Match")==etag) { webServer.send(304); return; }
    const uint32_t now=millis();
    if (lcdSnapshotServed && uint32_t(now-lastLcdSnapshotMs)<1000) {
      webServer.sendHeader("Retry-After","1"); webServer.send(429,"text/plain","Preview limited to one frame per second"); return;
    }
    lastLcdSnapshotMs=now; lcdSnapshotServed=true;
    const auto header=DashLcdBitmap::header();
    webServer.setContentLength(DashLcdBitmap::kFileSize);
    webServer.send(200,"image/bmp","");
    webServer.sendContent(reinterpret_cast<const char *>(header.data()),header.size());
    // HTTP and rendering run on the same loop, so this frame cannot change mid-send.
    // Stream the existing sprite: no second full-screen buffer is allocated.
    webServer.sendContent(static_cast<const char *>(frame.getPointer()),DashLcdBitmap::kPixels);
  });
  webServer.on("/", HTTP_GET, []() {
    dashEvent("http_get_dashboard");
    webServer.sendHeader("Cache-Control", "no-store");
    webServer.send_P(200, "text/html", kDashWebUi);
  });
  webServer.on("/api/status", HTTP_GET, []() {
    static uint32_t count=0,last=0; ++count;
    if(uint32_t(millis()-last)>=60000) { dashEvent("http_status_poll_summary",count); count=0; last=millis(); }
    webServer.sendHeader("Cache-Control", "no-store");
    webServer.send(200, "application/json", statusJson());
  });
  webServer.on("/settings",HTTP_GET,[]() {
    if (!otaEnabled) { webServer.send(403,"text/plain","Configure an OTA password to enable settings"); return; }
    if (!webServer.authenticate("admin",AppConfig::kOta.password)) {
      webServer.requestAuthentication(DIGEST_AUTH,"Apexi Dash"); return;
    }
    webServer.sendHeader("Cache-Control","no-store");
    webServer.send_P(200,"text/html",kDashWebUi);
  });
  webServer.on("/api/settings",HTTP_GET,[]() {
    if (!otaEnabled) { webServer.send(403); return; }
    if (!webServer.authenticate("admin",AppConfig::kOta.password)) {
      webServer.requestAuthentication(DIGEST_AUTH,"Apexi Dash"); return;
    }
    webServer.sendHeader("Cache-Control","no-store");
    webServer.send(200,"application/json",String("{\"csrf\":\"")+csrfToken+"\"}");
  });
  webServer.on("/api/settings",HTTP_POST,[]() {
    dashEvent("http_post_settings");
    if (!otaEnabled) { webServer.send(403); return; }
    if (!webServer.authenticate("admin",AppConfig::kOta.password)) {
      webServer.requestAuthentication(DIGEST_AUTH,"Apexi Dash"); return;
    }
    if (!storageReady) { webServer.send(503,"text/plain","Settings storage unavailable"); return; }
    StaticJsonDocument<512> doc;
    if (webServer.arg("plain").length()>512 || deserializeJson(doc,webServer.arg("plain")) ||
        !doc["csrf"].is<const char *>() || csrfToken != doc["csrf"].as<const char *>()) {
      webServer.send(400,"text/plain","Invalid request or settings token"); return;
    }
    if (!doc["refreshMs"].is<uint32_t>() || !DashTelemetry::validRefresh(doc["refreshMs"]) ||
        !doc["slots"].is<JsonArray>() || doc["slots"].size()!=2) {
      webServer.send(400,"text/plain","Choose two slots and 250–5000 ms in 250 ms steps"); return;
    }
    DisplaySettings next=settings; next.refreshMs=doc["refreshMs"];
    for (size_t i=0;i<2;++i) {
      if (!doc["slots"][i].is<const char *>()) { webServer.send(400); return; }
      const char *id=doc["slots"][i];
      if (!validSlot(id)) { webServer.send(400,"text/plain","Invalid sensor ID"); return; }
      memset(next.slots[i],0,16); strcpy(next.slots[i],id);
    }
    if (memcmp(&settings,&next,sizeof(next)) && preferences.putBytes("display",&next,sizeof(next))!=sizeof(next)) {
      webServer.send(500,"text/plain","Could not save settings"); return;
    }
    settings=next; renderStatus(); webServer.send(200,"application/json","{\"saved\":true}");
  });
  webServer.begin();
}

void serviceOta() {
  if (!otaEnabled) return;
  if (!wifiConnected) {
    if (otaReady) ArduinoOTA.end();
    otaReady = false;
    return;
  }
  if (!otaReady) {
    ArduinoOTA.setHostname("apexi-dash");
    ArduinoOTA.setPassword(AppConfig::kOta.password);
    ArduinoOTA.onStart([]() {
      dashEvent("ota_started");
      Serial0.println("DASH_OTA=updating");
      if (lcdBuffered) {
        frame.fillSprite(TFT_BLACK); frame.setTextSize(1);
        frame.setTextColor(TFT_WHITE,TFT_BLACK);
        frame.drawString("UPDATING",120,110,4);
        frame.drawString("Keep power connected",120,145,2);
        pushFrame(); lastFrameKey="";
      }
    });
    ArduinoOTA.onEnd([]() { dashEvent("ota_complete"); Serial0.println("DASH_OTA=complete"); });
    ArduinoOTA.onError([](ota_error_t error) {
      dashEvent("ota_failed",error,3);
      Serial0.printf("DASH_OTA=error:%u\n", static_cast<unsigned>(error));
      renderStatus();
    });
    ArduinoOTA.begin();
    otaReady = true;
    Serial0.println("DASH_OTA=ready");
  }
  ArduinoOTA.handle();
}

void beginBle() {
  eventBoot=esp_random(); dashEvent("boot");
  BLEDevice::init(DashLinkProtocol::kDashDeviceName);
  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(&serverCallbacks);
  BLEService *service = server->createService(DashLinkProtocol::kServiceUuid);
  BLECharacteristic *handshake = service->createCharacteristic(
      DashLinkProtocol::kHandshakeCharacteristicUuid,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  handshake->setValue("waiting");
  handshake->setCallbacks(&handshakeCallbacks);
  auto *telemetryCharacteristic = service->createCharacteristic(DashTelemetry::kUuid,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  telemetryCharacteristic->setCallbacks(&telemetryCallbacks);
  eventCharacteristic=service->createCharacteristic(SystemEvents::kUuid,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR | BLECharacteristic::PROPERTY_NOTIFY);
  eventCharacteristic->addDescriptor(new BLE2902());
  eventCharacteristic->setCallbacks(&eventCallbacks);
  service->start();

  advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(DashLinkProtocol::kServiceUuid);
  advertising->setScanResponse(true);
  advertising->start();
}

}  // namespace

void setup() {
  Serial0.begin(115200);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
  display.begin();
  display.setRotation(0);
  lcdBootId=String(esp_random(),HEX)+String(esp_random(),HEX);
  frame.setColorDepth(8); // 57.6 KB, allocated once before BLE; no per-frame allocations.
  lcdBuffered=frame.createSprite(240,240)!=nullptr;
  if (!lcdBuffered) {
    display.fillScreen(TFT_BLACK); display.setTextDatum(MC_DATUM);
    display.setTextColor(TFT_ORANGE,TFT_BLACK);
    display.drawString("LCD buffer unavailable",120,120,2);
    Serial0.println("DASH_LCD=buffer allocation failed");
  }
  storageReady=preferences.begin("dash-display",false);
  DisplaySettings stored;
  if (storageReady && preferences.getBytes("display",&stored,sizeof(stored))==sizeof(stored) &&
      stored.magic==settings.magic && DashTelemetry::validRefresh(stored.refreshMs) &&
      validSlot(stored.slots[0]) && validSlot(stored.slots[1])) settings=stored;
  csrfToken=String(esp_random(),HEX)+String(esp_random(),HEX)+String(esp_random(),HEX)+String(esp_random(),HEX);

  beginWifi();
  beginWebUi();
  beginBle();
  renderStatus();

  Serial0.println("Apexi Dash ready");
  Serial0.print("DASH_WEB=http://");
  Serial0.println(WiFi.softAPIP());
}

void loop() {
  serviceSystemEvents();
  const uint32_t loopNow=millis();
  if(lastLoopMs)maxLoopGapMs=std::max(maxLoopGapMs,uint32_t(loopNow-lastLoopMs));
  lastLoopMs=loopNow;
  observeDiagnostics();
  serviceWifi();
  serviceOta();
  const uint32_t webStart=millis();webServer.handleClient();
  maxWebMs=std::max(maxWebMs,uint32_t(millis()-webStart));
  if (restartAdvertising) {
    restartAdvertising = false;
    advertising->start();
  }
  if (loggerReady != renderedLoggerReady ||
      bleConnected != renderedBleConnected || uint32_t(millis()-lastRenderMs)>=settings.refreshMs) {
    renderStatus();
  }
  delay(2);
}

#endif
