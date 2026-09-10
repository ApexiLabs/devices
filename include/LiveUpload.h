#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#if defined(ESP8266)
#include <WiFiClientSecureBearSSL.h>
#include <memory>
#else
#include <WiFiClientSecure.h>
#endif

#include "AppConfig.h"
#include "RemoteConfig.h"
#include "StoreForwardQueue.h"
#include "Types.h"
#include "UploadEvidence.h"
#include "HttpsWorker.h"
#include "HttpsPacing.h"
class LoggerAuthorization;

class LiveUpload {
 public:
  LiveUpload();
  static const char *trustRoot();

  bool begin(const AppConfig::UploadConfig &config,
             bool enabled,
             bool remoteManagementEnabled,
             uint32_t appliedConfigVersion);
  void loop();
  bool publishIfDue(const AppState &state);

  bool isEnabled() const;
  void setAuthorization(LoggerAuthorization &authorization){authorization_=&authorization;}
  UploadEvidence::Status uploadEvidence(uint32_t nowMs);
  bool isConnected();
  String protocolName() const;
  String serverName() const;
  String sessionId() const;
  String lastError() const;
  uint32_t lastSequence() const;
  int lastHttpStatus() const { return lastHttpStatus_; }
  UploadPerformance performance() const {return performance_;}
  bool storeForwardEnabled() const;
  bool storeForwardReady() const;
  uint32_t storeForwardPendingRecords() const;
  size_t storeForwardPendingBytes() const;
  size_t storeForwardCapacityBytes() const;
  uint32_t storeForwardDroppedRecords() const;
  String storeForwardError() const;
  String pairingCode() const;
  uint32_t pairingCodeExpiresInSeconds() const;
  String managementStatus() const;
  String managementError() const;
  void setReportedConfig(bool uploadEnabled,
                         const char *ntpPrimary,
                         const char *ntpSecondary,
                         const char *timeZoneRule,
                         const char *timeZoneLabel);
  bool consumeRemoteConfig(RemoteConfig &config);
  void acknowledgeRemoteConfig(uint32_t version);

 private:
#if defined(ESP32)
  enum class Operation { None, Status, Fallback, Snapshot, Batch };
  void serviceHttps(uint32_t now);
  bool captureHttps(const AppState &state);
  bool submitHttps(Operation operation,const String &payload);
  Operation operation_=Operation::None;
  HttpsPacing pacing_;
  bool workerReady_=false,statusRequested_=true,fallbackRequested_=false,backoff_=false,durableInFlight_=false;
  uint32_t completedMs_=0;
  String inFlightPayload_,volatilePayload_;
  std::vector<String> batchRecords_;
  String batchId_,batchPayload_;
  bool batchEnabled_=false,batchAcknowledged_=false;
  size_t batchAckIndex_=0;
#endif
  bool reconnect(uint32_t nowMs);
  void publishOfflineStatusAndDisconnect();
  bool publishStatus(bool connected);
  bool publishSnapshot(const AppState &state);
  bool queueSnapshot(const AppState &state);
  bool replayQueuedSnapshot();
  bool postHttps(const char *kind, const String &payload, String *responseBody = nullptr);
  void consumeHttpsDesiredConfig(const String &responseBody);
  void handleMqttMessage(char *topic, uint8_t *payload, unsigned int length);
  bool parseRemoteConfig(const uint8_t *payload, unsigned int length, RemoteConfig &config);
  void rotatePairingCode(uint32_t nowMs);
  String liveTopic() const;
  String statusTopic() const;
  String desiredConfigTopic() const;
  String buildStatusJson(bool connected) const;
  String buildSnapshotJson(const AppState &state, uint32_t sequence) const;
  static String jsonEscape(const String &value);

  WiFiClient networkClient_;
  PubSubClient mqttClient_;
#if defined(ESP8266)
  BearSSL::WiFiClientSecure httpsClient_;
  std::unique_ptr<BearSSL::X509List> httpsTrustAnchor_;
#else
  WiFiClientSecure httpsClient_;
#endif
  AppConfig::UploadConfig config_{};
  bool enabled_ = false;
  UploadEvidence::Tracker uploadEvidence_;
  UploadPerformance performance_;
  LoggerAuthorization *authorization_=nullptr;
  bool remoteManagementEnabled_ = false;
  uint32_t lastPublishMs_ = 0;
  uint32_t lastReconnectAttemptMs_ = 0;
  uint32_t lastHttpsAttemptMs_ = 0;
  uint32_t lastSequence_ = 0;
  uint32_t lastStatusPublishMs_ = 0;
  uint32_t appliedConfigVersion_ = 0;
  String deviceId_;
  String sessionId_;
  String clientId_;
  String lastError_;
  String lastHttpError_;
  String pairingCode_;
  uint32_t pairingCodeGeneratedMs_ = 0;
  String managementStatus_ = "ready";
  String managementError_;
  bool reportedUploadEnabled_ = false;
  String reportedNtpPrimary_;
  String reportedNtpSecondary_;
  String reportedTimeZoneRule_;
  String reportedTimeZoneLabel_;
  RemoteConfig pendingRemoteConfig_{};
  bool hasPendingRemoteConfig_ = false;
  bool httpsConnected_ = false;
  int lastHttpStatus_ = 0;
  bool lastPostRetryable_ = false;
  StoreForwardQueue storeForwardQueue_;
};
