#pragma once
#include <Arduino.h>
#if defined(APEXI_AUTH_HOST_TEST)
#include "AuthTestConfig.h"
#else
#include "AppConfig.h"
#endif
#include <ArduinoJson.h>
#if defined(ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#endif

// Owner-approved authorization; public hardware ID is never an authenticator.
// Network work runs on a dedicated worker, never in the sensor loop.
class LoggerAuthorization {
 public:
  bool begin(const AppConfig::UploadConfig &config);
  bool requestAuthorization();
  bool factoryReset();
  void loop();
  const AppConfig::UploadConfig &uploadConfig() const {return config_;}
  const String &hardwareId() const {return hardwareId_;}
  const String &status() const {return status_;}
  const String &error() const {return error_;}
  const String &userCode() const {return userCode_;}
  const String &csrfToken() const {return csrf_;}
  uint32_t expiresIn() const;
  bool restartRequired() const {return restart_;}
  bool supported() const {return status_!="unsupported";}
  const char *bearer(bool statusRequest=false) const;
  void statusResult(bool accepted);
  bool stageRotation(JsonObjectConst rotation);
  bool stageDesired(JsonObjectConst envelope);
  String rotationAcknowledgement() const;
  bool hasRotation() const {return rotationPhase_!=0;}
  void beginRotationFallback(){fallback_=true;}
  bool finishRotationFallback(bool oldAccepted);
 private:
  friend struct AuthorizationTestAccess;
  bool restoreRecord(const String &record);
  static bool ownerResetRecord(const String &saved, String &record);
  bool saveRecord(const String &token,const String &deviceId,uint32_t version);
  bool send(bool start,bool ack=false,bool verify=false);
  void accept(int httpStatus,const String &body,bool start,bool ack,bool verify=false);
  AppConfig::UploadConfig config_{};
  String serverHost_,cloudflareId_,cloudflareSecret_;
  String hardwareId_,installationId_,installationSecret_,origin_,activeToken_,deviceId_;
  String authorizationId_,deviceCode_,userCode_,csrf_;
  String storedToken_,storedDeviceId_;
  String storedOrigin_,bootstrapToken_,bootstrapDeviceId_;
  uint32_t bootstrapVersion_=0;
  uint32_t credentialVersion_=0,expiresEpoch_=0;
  bool ackPending_=false;
  bool verifyPending_=false;
  String candidate_,rotationNonce_,rotationExpiry_,appliedRotationNonce_;
  uint32_t rotationVersion_=0,appliedRotationVersion_=0;
  uint8_t rotationPhase_=0;
  bool fallback_=false;
  String status_="unavailable",error_;
  uint32_t startedMs_=0,expiresMs_=0,lastPollMs_=0,pollMs_=5000;
  bool busy_=false,restart_=false,ready_=false;
#if defined(ESP32)
  bool requestStart_=false,requestAck_=false,requestVerify_=false,startPending_=false;
#endif
};
