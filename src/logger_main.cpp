#include <Adafruit_ADS1X15.h>
#include <ArduinoOTA.h>
#include <array>
#include <SPI.h>
#include <time.h>
#include <Wire.h>
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

#include "AppConfig.h"
#include "CsvLogger.h"
#include "Dashboard.h"
#include "DashLink.h"
#include "BatteryMonitor.h"
#include "LiveUpload.h"
#include "Logic.h"
#include "RuntimeSettings.h"
#include "SensorChannel.h"
#include "Timekeeper.h"
#include "WebUi.h"
#include "SystemLog.h"
#include "RemoteLogs.h"

namespace {

Adafruit_ADS1115 ads;
SPIClass &spiBus = SPI;
std::array<SensorChannel, AppConfig::kSensorCount> sensorChannels = [] {
  std::array<SensorChannel, AppConfig::kSensorCount> channels{};
  for (size_t index = 0; index < AppConfig::kSensorCount; ++index) {
    channels[index].configure(AppConfig::kSensorConfigs[index]);
  }
  return channels;
}();
Timekeeper timekeeper;
CsvLogger csvLogger;
Dashboard dashboard;
DashLink dashLink;
BatteryMonitor batteryMonitor;
WebUi webUi;
LiveUpload liveUpload;
LoggerAuthorization loggerAuthorization;
RuntimeSettings runtimeSettings;
RemoteLogs remoteLogs;

bool adcReady = false;
bool rtcReady = false;
bool rtcNetworkSynced = false;
bool wifiReady = false;
bool otaReady = false;
bool networkTimeConfigured = false;
String rtcLastSync;

uint32_t lastSampleMs = 0;
uint32_t lastDisplayMs = 0;
uint32_t lastLogMs = 0;
uint32_t lastUploadPublishMs = 0;
uint32_t lastSerialMs = 0;

bool buttonLastLevel = HIGH;
uint32_t buttonPressedAtMs = 0;
bool longPressHandled = false;

constexpr uint32_t kValidNetworkEpoch = 1704067200UL;  // 2024-01-01 UTC
constexpr uint32_t kNtpSyncTimeoutMs = 10000;
constexpr uint32_t kRtcSyncRetryIntervalMs = 60000;
constexpr uint32_t kRtcResyncIntervalMs = 60UL * 60UL * 1000UL;
uint32_t lastRtcSyncAttemptMs = 0;
uint32_t lastRtcSuccessfulSyncMs = 0;

uint64_t extendedUptimeMs() {
  static uint32_t previousMs = 0;
  static uint64_t wrapBaseMs = 0;
  const uint32_t currentMs = millis();
  if (currentMs < previousMs) {
    wrapBaseMs += (1ULL << 32);
  }
  previousMs = currentMs;
  return wrapBaseMs + currentMs;
}

float readVoltage(const uint8_t channel) {
  const int16_t raw = ads.readADC_SingleEnded(channel);
  return ads.computeVolts(raw);
}

void clearFaults() {
  for (SensorChannel &sensor : sensorChannels) {
    sensor.clearLatchedFault();
  }
}

void handleButton() {
  const bool level = digitalRead(AppConfig::kPins.buttonPin);
  const uint32_t nowMs = millis();

  if (buttonLastLevel == HIGH && level == LOW) {
    buttonPressedAtMs = nowMs;
    longPressHandled = false;
  } else if (buttonLastLevel == LOW && level == LOW) {
    if (!longPressHandled && (nowMs - buttonPressedAtMs) >= 1200) {
      clearFaults();
      longPressHandled = true;
    }
  } else if (buttonLastLevel == LOW && level == HIGH) {
    if (!longPressHandled && (nowMs - buttonPressedAtMs) >= 30) {
      dashboard.nextScreen();
    }
  }

  buttonLastLevel = level;
}

AppState buildState() {
  AppState state{};
  state.system.batterySupported = batteryMonitor.supported();
  state.system.batteryValid = batteryMonitor.valid();
  state.system.batteryVoltage = batteryMonitor.voltage();
  state.system.batteryPercent = batteryMonitor.percent();
  state.system.externalPower = batteryMonitor.externalPower();
  state.system.batteryTrend = batteryMonitor.trend();
  state.system.batteryState = batteryMonitor.state();
  for (size_t index = 0; index < sensorChannels.size(); ++index) {
    state.sensors[index] = sensorChannels[index].snapshot();
  }
  const uint64_t uptimeMs = extendedUptimeMs();
  state.uptimeMs = static_cast<uint32_t>(uptimeMs);
  state.uptime = String(Logic::formatUptime(uptimeMs).c_str());
  state.timestamp = timekeeper.logTimestamp(state.uptimeMs);
  state.transportTimestamp = timekeeper.transportTimestamp(state.uptimeMs);
  state.system.adcReady = adcReady;
  state.system.displayEnabled = AppConfig::kFeatures.displayEnabled;
  state.system.rtcEnabled = AppConfig::kFeatures.rtcEnabled;
  state.system.rtcReady = rtcReady;
  state.system.rtcSynced = rtcNetworkSynced;
  state.system.rtcError = timekeeper.lastError();
  state.system.rtcLastSync = rtcLastSync;
  state.system.timeZone = runtimeSettings.timeZoneLabel();
  state.system.sdEnabled = AppConfig::kFeatures.sdLoggingEnabled;
  state.system.sdReady = csvLogger.isReady() && csvLogger.lastError().isEmpty();
  state.system.wifiReady = wifiReady;
  state.system.uploadEnabled = liveUpload.isEnabled();
  state.system.uploadConnected = liveUpload.isConnected();
  state.system.dashEnabled = dashLink.isEnabled();
  state.system.dashConnected = dashLink.isConnected();
  state.system.dashStatus = dashLink.status();
  state.system.otaEnabled = AppConfig::kFeatures.otaUpdatesEnabled;
  state.system.otaReady = otaReady;
  state.system.wifiMode = webUi.modeString();
  state.system.ipAddress = webUi.ipAddress();
  state.system.currentLogFile = csvLogger.currentFileName();
  state.system.lastLogError = csvLogger.lastError();
  state.system.uploadProtocol = liveUpload.protocolName();
  state.system.uploadServer = liveUpload.serverName();
  state.system.uploadSessionId = liveUpload.sessionId();
  state.system.lastUploadError = liveUpload.lastError();
  state.system.lastUploadSequence = liveUpload.lastSequence();
  state.system.lastUploadHttpStatus=liveUpload.lastHttpStatus();
  state.system.uploadPerformance=liveUpload.performance();
  const auto uploadEvidence=liveUpload.uploadEvidence(millis());
  state.system.uploadEvidenceState=uploadEvidence.state;
  state.system.uploadSuccessAgeMs=uploadEvidence.ageMs;
  state.system.remoteManagementEnabled = runtimeSettings.remoteManagementEnabled();
  state.system.appliedConfigVersion = runtimeSettings.appliedConfigVersion();
  state.system.remoteManagementStatus = liveUpload.managementStatus();
  state.system.remoteManagementError = liveUpload.managementError();
  state.system.storeForwardEnabled = liveUpload.storeForwardEnabled();
  state.system.storeForwardReady = liveUpload.storeForwardReady();
  state.system.storeForwardPendingRecords = liveUpload.storeForwardPendingRecords();
  state.system.storeForwardPendingBytes = liveUpload.storeForwardPendingBytes();
  state.system.storeForwardCapacityBytes = liveUpload.storeForwardCapacityBytes();
  state.system.storeForwardDroppedRecords = liveUpload.storeForwardDroppedRecords();
  state.system.storeForwardError = liveUpload.storeForwardError();
  return state;
}

void sampleSensors() {
  for (size_t index = 0; index < sensorChannels.size(); ++index) {
    const AppConfig::SensorConfig &config = AppConfig::kSensorConfigs[index];
    const float voltage = adcReady ? readVoltage(config.adsChannel) : 0.0f;
    sensorChannels[index].update(voltage, adcReady);
  }
}

void beginOta() {
  if (!AppConfig::kFeatures.otaUpdatesEnabled || !wifiReady ||
      webUi.modeString() != "STA" || strlen(AppConfig::kOta.password) == 0) {
    otaReady = false;
    return;
  }

  ArduinoOTA.setHostname(AppConfig::kOta.hostname);
  ArduinoOTA.setPassword(AppConfig::kOta.password);
  ArduinoOTA.onStart([]() { systemLog.add("ota_started"); Serial.println("OTA update started"); });
  ArduinoOTA.onEnd([]() { systemLog.add("ota_complete"); Serial.println("OTA update complete"); });
  ArduinoOTA.onError([](ota_error_t error) {
    systemLog.add("ota_failed",error,3);
    Serial.print("OTA error=");
    Serial.println(static_cast<unsigned int>(error));
  });
  ArduinoOTA.begin();
  otaReady = true;
}

bool syncRtcFromNetwork(const bool waitForInitialSync) {
  if (!AppConfig::kFeatures.rtcEnabled || !wifiReady ||
      webUi.modeString() != "STA") {
    return false;
  }

  if (!networkTimeConfigured) {
#if defined(ESP8266)
    configTime(runtimeSettings.timeZoneRule(),
               runtimeSettings.ntpPrimary(),
               runtimeSettings.ntpSecondary());
#else
    configTzTime(runtimeSettings.timeZoneRule(),
                 runtimeSettings.ntpPrimary(),
                 runtimeSettings.ntpSecondary());
#endif
    networkTimeConfigured = true;
  }

  const uint32_t startedMs = millis();
  lastRtcSyncAttemptMs = startedMs;
  time_t now = time(nullptr);
  while (waitForInitialSync &&
         now < static_cast<time_t>(kValidNetworkEpoch) &&
         (millis() - startedMs) < kNtpSyncTimeoutMs) {
    ArduinoOTA.handle();
    webUi.handleClient();
    delay(100);
    now = time(nullptr);
  }

  if (now >= static_cast<time_t>(kValidNetworkEpoch)) {
    rtcReady = timekeeper.setFromUnixTime(static_cast<uint32_t>(now));
    if (rtcReady) {
      rtcNetworkSynced = true;
      lastRtcSuccessfulSyncMs = millis();
      rtcLastSync = timekeeper.logTimestamp(lastRtcSuccessfulSyncMs) + " " +
                    runtimeSettings.timeZoneLabel();
      return true;
    }
  }
  return false;
}

void maintainRtcSync(const uint32_t nowMs) {
  if (!AppConfig::kFeatures.rtcEnabled || !wifiReady ||
      webUi.modeString() != "STA") {
    return;
  }
  const uint32_t previousMs =
      rtcNetworkSynced ? lastRtcSuccessfulSyncMs : lastRtcSyncAttemptMs;
  const uint32_t intervalMs =
      rtcNetworkSynced ? kRtcResyncIntervalMs : kRtcSyncRetryIntervalMs;
  if (Logic::intervalElapsed(nowMs, previousMs, intervalMs)) {
    syncRtcFromNetwork(false);
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  const uint32_t serialWaitStartMs = millis();
  while (!Serial && (millis() - serialWaitStartMs) < 5000) {
    delay(10);
  }
  Serial.println("Apexi Logger boot");
  Serial.flush();

  // A steady light confirms that the MCU has reached firmware setup.
  pinMode(AppConfig::kPins.statusLed, OUTPUT);
#if defined(ESP8266)
  digitalWrite(AppConfig::kPins.statusLed, LOW);
#else
  digitalWrite(AppConfig::kPins.statusLed, HIGH);
#endif

  pinMode(AppConfig::kPins.buttonPin, INPUT_PULLUP);
  if (AppConfig::kFeatures.displayEnabled) {
    pinMode(AppConfig::kPins.tftCs, OUTPUT);
    digitalWrite(AppConfig::kPins.tftCs, HIGH);
  }
  if (AppConfig::kFeatures.sdLoggingEnabled) {
    pinMode(AppConfig::kPins.sdCs, OUTPUT);
    digitalWrite(AppConfig::kPins.sdCs, HIGH);
  }

  if (!AppConfig::kFeatures.rtcEnabled) {
    timekeeper.disable();
    rtcReady = false;
  }
  if (!AppConfig::kFeatures.sdLoggingEnabled) {
    csvLogger.disable();
  }

  runtimeSettings.begin(AppConfig::kLiveUpload, AppConfig::kFeatures.liveUploadEnabled);
  wifiReady = webUi.begin(AppConfig::kWifi, csvLogger, runtimeSettings);
  loggerAuthorization.begin(runtimeSettings.uploadConfig());
  webUi.setAuthorization(loggerAuthorization);
  liveUpload.setAuthorization(loggerAuthorization);
  liveUpload.begin(loggerAuthorization.uploadConfig(),
                   runtimeSettings.liveUploadEnabled(),
                   runtimeSettings.remoteManagementEnabled(),
                   runtimeSettings.appliedConfigVersion());
  liveUpload.setReportedConfig(runtimeSettings.liveUploadEnabled(),
                               runtimeSettings.ntpPrimary(),
                               runtimeSettings.ntpSecondary(),
                               runtimeSettings.timeZoneRule(),
                               runtimeSettings.timeZoneLabel());
  Serial.print("storeForwardReady=");
  Serial.print(liveUpload.storeForwardReady() ? "1" : "0");
  Serial.print(" pending=");
  Serial.print(liveUpload.storeForwardPendingRecords());
  Serial.print(" capacityBytes=");
  Serial.println(liveUpload.storeForwardCapacityBytes());
  webUi.setManagementPairingCode(liveUpload.pairingCode(),
                                 liveUpload.pairingCodeExpiresInSeconds());
  Serial.print("wifiReady=");
  Serial.println(wifiReady ? "1" : "0");
  Serial.print("wifiMode=");
  Serial.println(webUi.modeString());
  Serial.print("ip=");
  Serial.println(webUi.ipAddress());
  beginOta();
  Serial.print("otaReady=");
  Serial.println(otaReady ? "1" : "0");

  Wire.begin(AppConfig::kPins.i2cSda, AppConfig::kPins.i2cScl);
  spiBus.begin();

  dashboard.begin();
  dashLink.begin(AppConfig::kDashLink);

  adcReady = ads.begin(AppConfig::kAds1115Address, &Wire);
  if (adcReady) {
    ads.setGain(GAIN_ONE);
    ads.setDataRate(RATE_ADS1115_860SPS);
  }

  if (AppConfig::kFeatures.rtcEnabled) {
    rtcReady = timekeeper.begin(Wire, AppConfig::kRtc);
    syncRtcFromNetwork(true);
  }

  if (AppConfig::kFeatures.sdLoggingEnabled) {
    csvLogger.begin(AppConfig::kPins.sdCs, spiBus);
  }

  Serial.print("adcReady=");
  Serial.println(adcReady ? "1" : "0");

  sampleSensors();
  batteryMonitor.begin();
  systemLog.begin(timekeeper,loggerAuthorization.uploadConfig().deviceId);
  remoteLogs.begin(loggerAuthorization.uploadConfig());
  const AppState initialState = buildState();
  dashboard.render(initialState);
  webUi.publishState(initialState);
}

void loop() {
  loggerAuthorization.loop();
  if(loggerAuthorization.restartRequired()) {
    static uint32_t authorizedMs=millis();
    if(uint32_t(millis()-authorizedMs)>1500)ESP.restart();
  }
  remoteLogs.loop(runtimeSettings.remoteManagementEnabled() && runtimeSettings.liveUploadEnabled());
  systemLog.loop(timekeeper,csvLogger.isReady());
  static uint32_t lastHealthMs=0;
  static uint8_t health=255;
  if(uint32_t(millis()-lastHealthMs)>=1000) {
    lastHealthMs=millis();
    const uint8_t next=(WiFi.status()==WL_CONNECTED?1:0) | (dashLink.isConnected()?2:0) |
        (liveUpload.isConnected()?4:0) | (adcReady?8:0) | (rtcReady?16:0) |
        (csvLogger.isReady() && csvLogger.lastError().isEmpty()?32:0);
    const char *up[]={"wifi_connected","dash_connected","upstream_connected","adc_ready","rtc_ready","sd_ready"};
    const char *down[]={"wifi_disconnected","dash_disconnected","upstream_disconnected","adc_fault","rtc_fault","sd_fault"};
    for(unsigned bit=0;bit<6;++bit) if(health==255 || ((next^health)&(1<<bit)))
      systemLog.add(next&(1<<bit)?up[bit]:down[bit],0,next&(1<<bit)?1:2);
    health=next;
    static int previousHttpStatus=0;
    const int httpStatus=liveUpload.lastHttpStatus();
    if(httpStatus!=previousHttpStatus) {
      systemLog.add("upstream_http_status",uint32_t(httpStatus),httpStatus<0 || httpStatus>=400?2:1);
      previousHttpStatus=httpStatus;
    }
    static bool previousTimeSync=false;
    if(previousTimeSync!=rtcNetworkSynced) {
      systemLog.add(rtcNetworkSynced?"time_synced":"time_sync_lost",0,rtcNetworkSynced?1:2);
      previousTimeSync=rtcNetworkSynced;
    }
    static std::array<SensorFault,AppConfig::kSensorCount> faults{};
    for(size_t i=0;i<sensorChannels.size();++i) {
      const auto fault=sensorChannels[i].snapshot().activeFault;
      if(faults[i]!=fault) { systemLog.add("sensor_state",(uint32_t(i)<<16)|uint32_t(fault),fault==SensorFault::None?1:2); faults[i]=fault; }
    }
  }
  batteryMonitor.loop(millis());
  dashLink.loop(millis());
  handleButton();
  webUi.handleClient();
  liveUpload.loop();
  webUi.setManagementPairingCode(liveUpload.pairingCode(),
                                 liveUpload.pairingCodeExpiresInSeconds());
  RemoteConfig remoteConfig{};
  if (liveUpload.consumeRemoteConfig(remoteConfig)) {
    if (runtimeSettings.applyRemoteConfig(remoteConfig)) {
      liveUpload.setReportedConfig(runtimeSettings.liveUploadEnabled(),
                                   runtimeSettings.ntpPrimary(),
                                   runtimeSettings.ntpSecondary(),
                                   runtimeSettings.timeZoneRule(),
                                   runtimeSettings.timeZoneLabel());
      liveUpload.acknowledgeRemoteConfig(remoteConfig.version);
      Serial.print("remoteConfigApplied=");
      Serial.println(remoteConfig.version);
      Serial.flush();
      delay(250);
      ESP.restart();
    }
  }
  if (otaReady) {
    ArduinoOTA.handle();
  }

  const uint32_t nowMs = millis();
  maintainRtcSync(nowMs);

  if ((nowMs - lastSampleMs) >= AppConfig::kTiming.sampleIntervalMs) {
    sampleSensors();
    lastSampleMs = nowMs;
  }

  std::array<SensorSnapshot, AppConfig::kSensorCount> dashSamples{};
  for (size_t i = 0; i < sensorChannels.size(); ++i) dashSamples[i] = sensorChannels[i].snapshot();
  dashLink.publish(dashSamples, nowMs);
  const uint32_t uploadStatusMs=millis();
  dashLink.publishUploadStatus(liveUpload.uploadEvidence(uploadStatusMs),uploadStatusMs);

  if ((nowMs - lastLogMs) >= AppConfig::kTiming.loggingIntervalMs) {
    AppState state = buildState();
    if (AppConfig::kFeatures.sdLoggingEnabled && csvLogger.isReady()) {
      if (csvLogger.logRow(timekeeper, state.uptimeMs, state.sensors)) {
        csvLogger.flushIfNeeded(state.uptimeMs);
      }
    }
    state.system.sdReady = csvLogger.isReady() && csvLogger.lastError().isEmpty();
    state.system.currentLogFile = csvLogger.currentFileName();
    state.system.lastLogError = csvLogger.lastError();
    webUi.publishState(state);
    lastLogMs = nowMs;
  }

  if ((nowMs - lastDisplayMs) >= AppConfig::kTiming.displayIntervalMs) {
    const AppState state = buildState();
    dashboard.render(state);
    webUi.publishState(state);
    lastDisplayMs = nowMs;
  }

  if ((nowMs - lastUploadPublishMs) >= AppConfig::kLiveUpload.publishIntervalMs) {
    const AppState state = buildState();
    liveUpload.publishIfDue(state);
    webUi.publishState(buildState());
    lastUploadPublishMs = nowMs;
  }

  if ((nowMs - lastSerialMs) >= 1000) {
    const AppState state = buildState();
    Serial.print("IP_ADDRESS=");
    Serial.print(state.system.ipAddress);
    Serial.print(" MODE=");
    Serial.print(state.system.wifiMode);
    Serial.print(" adc=");
    Serial.print(state.system.adcReady ? 1 : 0);
    Serial.print(" wifi=");
    Serial.print(state.system.wifiReady ? 1 : 0);
    Serial.print(" ip=");
    Serial.print(state.system.ipAddress);
    Serial.print(" dash=");
    Serial.print(dashLink.status());
    for (const SensorSnapshot &sensor : state.sensors) {
      Serial.print(" | ");
      Serial.print(sensor.id);
      Serial.print(" V=");
      Serial.print(sensor.rawVoltage, 3);
      Serial.print(" mA=");
      Serial.print(sensor.loopCurrentmA, 2);
      Serial.print(" val=");
      Serial.print(sensor.filteredValue, 2);
      Serial.print(" fault=");
      Serial.print(sensorFaultToString(sensor.activeFault));
    }
    Serial.println();
    Serial.flush();
    lastSerialMs = nowMs;
  }
}
