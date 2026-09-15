#pragma once

#include <Arduino.h>
#if defined(ESP8266)
#include <ESP8266WebServer.h>
using LoggerWebServer = ESP8266WebServer;
#else
#include <WebServer.h>
using LoggerWebServer = WebServer;
#endif
#include "AppConfig.h"
#include "CsvLogger.h"
#include "RuntimeSettings.h"
#include "Types.h"
#include "LoggerAuthorization.h"

class WebUi {
 public:
  bool begin(const AppConfig::WifiConfig &config,
             CsvLogger &logger,
             RuntimeSettings &settings,
             const char *deviceHostname,
             const char *settingsPassword,
             bool localSettingsEnabled);
  void handleClient();
  void publishState(const AppState &state);
  bool isReady() const;
  String modeString() const;
  String ipAddress() const;
  void setManagementPairingCode(const String &pairingCode, uint32_t expiresInSeconds);
  void setAuthorization(LoggerAuthorization &authorization){authorization_=&authorization;}

 private:
  void registerRoutes();
  void sendLogged(int status,const char *type,const String &body);
  uint32_t requestStartedMs_=0;
  void handleIndex();
  void handleDiagnostics();
  void handleLiveJson();
  void handleFilesJson();
  void handleSettings();
  void handleSettingsSave();
  void handleDownload();
  String liveJson() const;
  String indexHtml() const;
  String diagnosticsHtml() const;
  String sensorCardsHtml() const;
  bool settingsAuthorized();
  static String htmlEscape(const String &value);
  static String jsonEscape(const String &value);

  LoggerWebServer server_{80};
  CsvLogger *logger_ = nullptr;
  RuntimeSettings *settings_ = nullptr;
  LoggerAuthorization *authorization_ = nullptr;
  AppState state_{};
  bool ready_ = false;
  String mode_;
  String ipAddress_;
  String managementPairingCode_;
  String deviceHostname_;
  String settingsPassword_;
  bool localSettingsEnabled_ = false;
  uint32_t managementPairingExpiresInSeconds_ = 0;
  bool restartPending_ = false;
  uint32_t restartRequestedMs_ = 0;
};
