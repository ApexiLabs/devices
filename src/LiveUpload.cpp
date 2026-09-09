#include "LiveUpload.h"

#include <ArduinoJson.h>

#if defined(ESP8266)
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#else
#include <HTTPClient.h>
#include <WiFi.h>
#include <esp_system.h>
#endif

#include "Logic.h"

namespace {

constexpr uint16_t kMqttBufferSize = 2048;
constexpr uint16_t kMqttSocketTimeoutSeconds = 1;
constexpr uint32_t kNetworkClientTimeoutMs = 500;
constexpr uint32_t kStatusHeartbeatMs = 30000;
constexpr uint32_t kPairingCodeRefreshMs = 10UL * 60UL * 1000UL;
constexpr size_t kRemoteConfigJsonCapacity = 1024;
constexpr size_t kHttpsResponseJsonCapacity = 1536;
constexpr uint32_t kHttpsTimeoutMs = 4000;
constexpr uint8_t kReplayRecordsPerCycle = 2;

const char kIsrgRootX1[] PROGMEM = R"CERT(-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----)CERT";

bool validRemoteText(const char *value, const size_t maximumLength) {
  if (value == nullptr) {
    return true;
  }
  const size_t length = strlen(value);
  if (length == 0 || length > maximumLength) {
    return false;
  }
  for (size_t index = 0; index < length; ++index) {
    if (static_cast<uint8_t>(value[index]) < 0x20) {
      return false;
    }
  }
  return true;
}

}  // namespace

LiveUpload::LiveUpload() : mqttClient_(networkClient_) {}

bool LiveUpload::begin(const AppConfig::UploadConfig &config,
                       const bool enabled,
                       const bool remoteManagementEnabled,
                       const uint32_t appliedConfigVersion) {
  config_ = config;
  enabled_ = enabled;
  remoteManagementEnabled_ = remoteManagementEnabled;
  appliedConfigVersion_ = appliedConfigVersion;
  managementStatus_ = appliedConfigVersion_ > 0 ? "applied" : "ready";
  managementError_ = "";

  const std::string normalizedDevice = Logic::normalizeTopicSegment(config.deviceId);
  deviceId_ = normalizedDevice.empty() ? "mda-logger" : String(normalizedDevice.c_str());

#if defined(ESP8266)
  const uint32_t bootCounter = ESP.random();
  const uint32_t chipSuffix = ESP.getChipId() & 0xFFFFUL;
#else
  const uint32_t bootCounter = esp_random();
  const uint32_t chipSuffix = static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFULL);
#endif
  sessionId_ = String(Logic::formatSessionId(deviceId_.c_str(), bootCounter).c_str());
  clientId_ = deviceId_ + "-" + String(chipSuffix, HEX);
  rotatePairingCode(millis());

  if (!enabled_) {
    lastError_ = "Live upload disabled";
    return false;
  }

  if (strlen(config_.mqttHost) == 0) {
    lastError_ = "Upload host not configured";
    return false;
  }

  if (config_.protocol == AppConfig::UploadConfig::Protocol::Https) {
    if (strlen(config_.httpsPath) == 0 || strlen(config_.cloudflareAccessClientId) == 0 ||
        strlen(config_.cloudflareAccessClientSecret) == 0 || strlen(config_.appDeviceToken) == 0) {
      lastError_ = "HTTPS credentials not configured";
      return false;
    }
#if defined(ESP8266)
    httpsTrustAnchor_.reset(new BearSSL::X509List(kIsrgRootX1));
    if (!httpsTrustAnchor_) {
      lastError_ = "HTTPS trust anchor allocation failed";
      return false;
    }
    httpsClient_.setTrustAnchors(httpsTrustAnchor_.get());
#else
    httpsClient_.setCACert(kIsrgRootX1);
#endif
    httpsClient_.setTimeout(kHttpsTimeoutMs);
    storeForwardQueue_.begin(AppConfig::kStoreForward.enabled,
                             AppConfig::kStoreForward.maximumBytes);
    lastError_ = "";
    return true;
  }

  mqttClient_.setServer(config_.mqttHost, config_.mqttPort);
  mqttClient_.setBufferSize(kMqttBufferSize);
  mqttClient_.setSocketTimeout(kMqttSocketTimeoutSeconds);
  mqttClient_.setCallback(
      [this](char *topic, uint8_t *payload, unsigned int length) {
        handleMqttMessage(topic, payload, length);
      });
  networkClient_.setTimeout(kNetworkClientTimeoutMs);

  if (!Logic::mqttIdentityMatches(config_.deviceId, config_.mqttUsername)) {
    lastError_ = "MQTT username must match normalized device ID " + deviceId_;
    return false;
  }

  lastError_ = "";
  return true;
}

void LiveUpload::loop() {
  if (!enabled_) {
    return;
  }

  const uint32_t nowMs = millis();
  if (remoteManagementEnabled_ &&
      Logic::intervalElapsed(nowMs, pairingCodeGeneratedMs_, kPairingCodeRefreshMs)) {
    rotatePairingCode(nowMs);
    if (config_.protocol == AppConfig::UploadConfig::Protocol::Https) {
      publishStatus(true);
    } else if (mqttClient_.connected()) {
      publishStatus(true);
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    publishOfflineStatusAndDisconnect();
    if (lastError_.isEmpty()) {
      lastError_ = "Wi-Fi disconnected";
    }
    return;
  }

  if (config_.protocol == AppConfig::UploadConfig::Protocol::Https) {
    if ((nowMs - lastStatusPublishMs_) >= kStatusHeartbeatMs &&
        (httpsConnected_ || (nowMs - lastHttpsAttemptMs_) >= config_.reconnectIntervalMs)) {
      publishStatus(true);
    }
    return;
  }

  mqttClient_.loop();
  if (mqttClient_.connected() && (nowMs - lastStatusPublishMs_) >= kStatusHeartbeatMs) {
    publishStatus(true);
  }
}

bool LiveUpload::publishIfDue(const AppState &state) {
  if (!enabled_) {
    return false;
  }

  const uint32_t nowMs = millis();
  const bool snapshotDue = (nowMs - lastPublishMs_) >= config_.publishIntervalMs;
  if (WiFi.status() != WL_CONNECTED) {
    publishOfflineStatusAndDisconnect();
    lastError_ = "Wi-Fi disconnected";
    if (config_.protocol == AppConfig::UploadConfig::Protocol::Https && snapshotDue) {
      return queueSnapshot(state);
    }
    return false;
  }

  if (config_.protocol == AppConfig::UploadConfig::Protocol::Https) {
    if (!snapshotDue) {
      return false;
    }
    if (lastStatusPublishMs_ == 0) {
      if (lastHttpsAttemptMs_ != 0 &&
          (nowMs - lastHttpsAttemptMs_) < config_.reconnectIntervalMs) {
        return queueSnapshot(state);
      }
      if (!publishStatus(true)) {
        return lastPostRetryable_ ? queueSnapshot(state) : false;
      }
    }
    if (storeForwardQueue_.pendingRecords() > 0) {
      // Append the current sample before replay so sequence order remains
      // monotonic. Replaying two records for every one appended drains the
      // backlog without dropping samples gathered during recovery.
      if (!queueSnapshot(state)) {
        return false;
      }
      for (uint8_t replayed = 0;
           replayed < kReplayRecordsPerCycle && storeForwardQueue_.pendingRecords() > 0;
           ++replayed) {
        if (!replayQueuedSnapshot()) {
          return lastPostRetryable_;
        }
      }
      httpsConnected_ = true;
      lastError_ = storeForwardQueue_.pendingRecords() == 0
                       ? ""
                       : "Replaying onboard queue: " +
                             String(storeForwardQueue_.pendingRecords()) + " pending";
      return true;
    }
    if (!publishSnapshot(state)) {
      return lastPostRetryable_ ? queueSnapshot(state) : false;
    }
    lastPublishMs_ = millis();
    lastError_ = "";
    return true;
  }
  mqttClient_.loop();

  if (!mqttClient_.connected() && !reconnect(nowMs)) {
    return false;
  }

  if ((nowMs - lastPublishMs_) < config_.publishIntervalMs) {
    return false;
  }

  if (!publishSnapshot(state)) {
    return false;
  }

  lastPublishMs_ = nowMs;
  lastError_ = "";
  return true;
}

bool LiveUpload::isEnabled() const { return enabled_; }

bool LiveUpload::isConnected() {
  return config_.protocol == AppConfig::UploadConfig::Protocol::Https ? httpsConnected_
                                                                      : mqttClient_.connected();
}

String LiveUpload::protocolName() const {
  if (!enabled_) return "";
  return config_.protocol == AppConfig::UploadConfig::Protocol::Https ? "https" : "mqtt";
}

String LiveUpload::serverName() const {
  if (strlen(config_.mqttHost) == 0) {
    return "Not configured";
  }
  const String endpoint = String(config_.mqttHost) + ":" + String(config_.mqttPort);
  return config_.protocol == AppConfig::UploadConfig::Protocol::Https
             ? endpoint + String(config_.httpsPath)
             : endpoint;
}

String LiveUpload::sessionId() const { return sessionId_; }

String LiveUpload::lastError() const { return lastError_; }

uint32_t LiveUpload::lastSequence() const { return lastSequence_; }

bool LiveUpload::storeForwardEnabled() const { return storeForwardQueue_.isEnabled(); }
bool LiveUpload::storeForwardReady() const { return storeForwardQueue_.isReady(); }
uint32_t LiveUpload::storeForwardPendingRecords() const {
  return storeForwardQueue_.pendingRecords();
}
size_t LiveUpload::storeForwardPendingBytes() const { return storeForwardQueue_.pendingBytes(); }
size_t LiveUpload::storeForwardCapacityBytes() const { return storeForwardQueue_.capacityBytes(); }
uint32_t LiveUpload::storeForwardDroppedRecords() const {
  return storeForwardQueue_.droppedRecords();
}
String LiveUpload::storeForwardError() const { return storeForwardQueue_.lastError(); }

String LiveUpload::pairingCode() const {
  return remoteManagementEnabled_ ? pairingCode_ : "";
}

uint32_t LiveUpload::pairingCodeExpiresInSeconds() const {
  if (!remoteManagementEnabled_ || pairingCode_.isEmpty()) {
    return 0;
  }
  const uint32_t elapsedMs = static_cast<uint32_t>(millis() - pairingCodeGeneratedMs_);
  if (elapsedMs >= kPairingCodeRefreshMs) {
    return 0;
  }
  return (kPairingCodeRefreshMs - elapsedMs + 999UL) / 1000UL;
}

String LiveUpload::managementStatus() const { return managementStatus_; }

String LiveUpload::managementError() const { return managementError_; }

void LiveUpload::setReportedConfig(const bool uploadEnabled,
                                   const char *ntpPrimary,
                                   const char *ntpSecondary,
                                   const char *timeZoneRule,
                                   const char *timeZoneLabel) {
  reportedUploadEnabled_ = uploadEnabled;
  reportedNtpPrimary_ = ntpPrimary == nullptr ? "" : ntpPrimary;
  reportedNtpSecondary_ = ntpSecondary == nullptr ? "" : ntpSecondary;
  reportedTimeZoneRule_ = timeZoneRule == nullptr ? "" : timeZoneRule;
  reportedTimeZoneLabel_ = timeZoneLabel == nullptr ? "" : timeZoneLabel;
}

void LiveUpload::rotatePairingCode(const uint32_t nowMs) {
#if defined(ESP8266)
  const uint64_t entropy = (static_cast<uint64_t>(ESP.random()) << 32) | ESP.random();
#else
  const uint64_t entropy = (static_cast<uint64_t>(esp_random()) << 32) | esp_random();
#endif
  pairingCode_ = Logic::formatPairingCode(entropy).c_str();
  pairingCodeGeneratedMs_ = nowMs;
}

bool LiveUpload::consumeRemoteConfig(RemoteConfig &config) {
  if (!hasPendingRemoteConfig_) {
    return false;
  }
  config = pendingRemoteConfig_;
  hasPendingRemoteConfig_ = false;
  return true;
}

void LiveUpload::acknowledgeRemoteConfig(const uint32_t version) {
  appliedConfigVersion_ = version;
  managementStatus_ = "applied";
  managementError_ = "";
  if (mqttClient_.connected()) {
    publishStatus(true);
  } else if (config_.protocol == AppConfig::UploadConfig::Protocol::Https &&
             WiFi.status() == WL_CONNECTED) {
    publishStatus(true);
  }
}

bool LiveUpload::reconnect(const uint32_t nowMs) {
  if (strlen(config_.mqttHost) == 0) {
    lastError_ = "MQTT host not configured";
    return false;
  }

  if ((nowMs - lastReconnectAttemptMs_) < config_.reconnectIntervalMs) {
    return false;
  }

  bool connected = false;
  const String willPayload = buildStatusJson(false);
  if (strlen(config_.mqttUsername) > 0) {
    connected = mqttClient_.connect(clientId_.c_str(),
                                    config_.mqttUsername,
                                    config_.mqttPassword,
                                    statusTopic().c_str(),
                                    0,
                                    true,
                                    willPayload.c_str());
  } else {
    connected =
        mqttClient_.connect(clientId_.c_str(), statusTopic().c_str(), 0, true, willPayload.c_str());
  }

  // Start the backoff after the blocking client call completes. Otherwise an
  // attempt lasting longer than the configured interval is retried immediately
  // and starves sensor sampling, OTA, and the web server.
  lastReconnectAttemptMs_ = millis();

  if (!connected) {
    lastError_ = "MQTT connect failed rc=" + String(mqttClient_.state());
    return false;
  }

  if (!publishStatus(true)) {
    return false;
  }
  if (remoteManagementEnabled_ && !mqttClient_.subscribe(desiredConfigTopic().c_str(), 1)) {
    lastError_ = "MQTT desired config subscribe failed";
    mqttClient_.disconnect();
    return false;
  }

  lastError_ = "";
  return true;
}

void LiveUpload::publishOfflineStatusAndDisconnect() {
  if (config_.protocol == AppConfig::UploadConfig::Protocol::Https) {
    httpsConnected_ = false;
    return;
  }
  if (mqttClient_.connected()) {
    if (publishStatus(false)) {
      mqttClient_.disconnect();
    }
  }
}

bool LiveUpload::publishStatus(const bool connected) {
  const String payload = buildStatusJson(connected);
  if (config_.protocol == AppConfig::UploadConfig::Protocol::Https) {
    String response;
    if (!postHttps("status", payload, &response)) {
      return false;
    }
    httpsConnected_ = connected;
    lastStatusPublishMs_ = millis();
    consumeHttpsDesiredConfig(response);
    return true;
  }
  if (!mqttClient_.publish(statusTopic().c_str(), payload.c_str(), true)) {
    lastError_ = "MQTT status publish failed";
    return false;
  }
  lastStatusPublishMs_ = millis();
  return true;
}

void LiveUpload::handleMqttMessage(char *topic, uint8_t *payload, unsigned int length) {
  if (!remoteManagementEnabled_ || String(topic) != desiredConfigTopic()) {
    return;
  }
  RemoteConfig candidate{};
  if (!parseRemoteConfig(payload, length, candidate)) {
    managementStatus_ = "rejected";
    managementError_ = "Invalid desired configuration";
    return;
  }
  if (candidate.version <= appliedConfigVersion_) {
    return;
  }
  pendingRemoteConfig_ = candidate;
  hasPendingRemoteConfig_ = true;
  managementStatus_ = "pending";
  managementError_ = "";
}

bool LiveUpload::parseRemoteConfig(const uint8_t *payload,
                                   const unsigned int length,
                                   RemoteConfig &config) {
  if (length == 0 || length > kRemoteConfigJsonCapacity) {
    return false;
  }
  StaticJsonDocument<kRemoteConfigJsonCapacity> document;
  if (deserializeJson(document, payload, length) != DeserializationError::Ok) {
    return false;
  }
  if (document["schema_version"].as<int>() != 1 ||
      String(document["device_id"].as<const char *>()) != deviceId_) {
    return false;
  }
  const uint32_t version = document["config_version"].as<uint32_t>();
  JsonObject settings = document["settings"].as<JsonObject>();
  if (version == 0 || settings.isNull()) {
    return false;
  }
  for (JsonPair item : settings) {
    const String key = item.key().c_str();
    if (key != "upload_enabled" && key != "ntp_primary" && key != "ntp_secondary" &&
        key != "tz_rule" && key != "tz_label") {
      return false;
    }
  }
  // The app sends a complete desired-settings snapshot. Requiring every field
  // prevents a version-only payload from being acknowledged as applied while
  // silently retaining the logger's previous values.
  if (!settings["upload_enabled"].is<bool>() ||
      !settings["ntp_primary"].is<const char *>() ||
      !settings["ntp_secondary"].is<const char *>() ||
      !settings["tz_rule"].is<const char *>() ||
      !settings["tz_label"].is<const char *>()) {
    return false;
  }
  const char *primary = settings["ntp_primary"].as<const char *>();
  const char *secondary = settings["ntp_secondary"].as<const char *>();
  const char *rule = settings["tz_rule"].as<const char *>();
  const char *label = settings["tz_label"].as<const char *>();
  if (!validRemoteText(primary, 63) || !validRemoteText(secondary, 63) ||
      !validRemoteText(rule, 63) || !validRemoteText(label, 31)) {
    return false;
  }
  config = {};
  config.version = version;
  config.hasUploadEnabled = true;
  config.uploadEnabled = settings["upload_enabled"].as<bool>();
  config.ntpPrimary = primary;
  config.ntpSecondary = secondary;
  config.timeZoneRule = rule;
  config.timeZoneLabel = label;
  return true;
}

bool LiveUpload::publishSnapshot(const AppState &state) {
  const uint32_t sequence = lastSequence_ + 1;
  const String payload = buildSnapshotJson(state, sequence);
  if (config_.protocol == AppConfig::UploadConfig::Protocol::Https) {
    if (!postHttps("snapshot", payload)) {
      return false;
    }
    httpsConnected_ = true;
  } else if (!mqttClient_.publish(liveTopic().c_str(), payload.c_str(), false)) {
    lastError_ = "MQTT live publish failed";
    return false;
  }
  lastSequence_ = sequence;
  return true;
}

bool LiveUpload::queueSnapshot(const AppState &state) {
  const uint32_t sequence = lastSequence_ + 1;
  const String payload = buildSnapshotJson(state, sequence);
  if (!storeForwardQueue_.enqueue(payload)) {
    lastError_ = "HTTPS unavailable and onboard queue failed: " + storeForwardQueue_.lastError();
    return false;
  }
  lastSequence_ = sequence;
  lastPublishMs_ = millis();
  httpsConnected_ = false;
  lastError_ = "HTTPS unavailable; snapshot stored onboard";
  return true;
}

bool LiveUpload::replayQueuedSnapshot() {
  String payload;
  if (!storeForwardQueue_.peek(payload)) {
    lastError_ = "Onboard queue read failed: " + storeForwardQueue_.lastError();
    return false;
  }
  if (postHttps("snapshot", payload)) {
    if (!storeForwardQueue_.pop()) {
      lastError_ = "Onboard queue acknowledge failed: " + storeForwardQueue_.lastError();
      return false;
    }
    httpsConnected_ = true;
    return true;
  }
  if (Logic::shouldDiscardQueuedHttpStatus(lastHttpStatus_)) {
    const String rejectedError = lastError_;
    if (!storeForwardQueue_.pop(true)) {
      lastError_ = "Rejected queue record could not be discarded: " +
                   storeForwardQueue_.lastError();
      return false;
    }
    lastError_ = rejectedError + "; queued record discarded";
    return true;
  }
  return false;
}

const char *LiveUpload::trustRoot() { return kIsrgRootX1; }

bool LiveUpload::postHttps(const char *kind, const String &payload, String *responseBody) {
  lastHttpsAttemptMs_ = millis();
  HTTPClient http;
  http.setTimeout(kHttpsTimeoutMs);
  http.setReuse(true);
  // Cloudflare's browser-integrity checks reject requests without a stable
  // client signature before Access evaluates the service-token headers.
  http.setUserAgent("ApexiLabs-Logger/1.0");
  String url = "https://" + String(config_.mqttHost);
  if (config_.mqttPort != 443) {
    url += ":" + String(config_.mqttPort);
  }
  url += String(config_.httpsPath) + "/" + String(kind);
  if (!http.begin(httpsClient_, url)) {
    lastHttpStatus_ = -1;
    lastPostRetryable_ = true;
    lastError_ = "HTTPS request setup failed";
    httpsConnected_ = false;
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(config_.appDeviceToken));
  http.addHeader("CF-Access-Client-Id", config_.cloudflareAccessClientId);
  http.addHeader("CF-Access-Client-Secret", config_.cloudflareAccessClientSecret);
#if defined(ESP8266)
  const uint32_t heapBeforePost = ESP.getFreeHeap();
  const uint32_t maxBlockBeforePost = ESP.getMaxFreeBlockSize();
  const uint8_t fragmentationBeforePost = ESP.getHeapFragmentation();
#endif
  const int status = http.POST(payload);
  lastHttpStatus_ = status;
  lastPostRetryable_ = Logic::isRetryableHttpStatus(status);
  const String body = status > 0 ? http.getString() : "";
  http.end();
  if (status < 200 || status >= 300) {
    if (status < 0) {
      lastError_ = "HTTPS " + HTTPClient::errorToString(status);
#if defined(ESP8266)
      char tlsError[96] = {};
      const int tlsErrorCode = httpsClient_.getLastSSLError(tlsError, sizeof(tlsError));
      if (tlsErrorCode != 0) {
        lastError_ += " TLS " + String(tlsErrorCode) + " " + String(tlsError);
      }
      lastError_ += " heap=" + String(heapBeforePost) + " max=" + String(maxBlockBeforePost) +
                    " frag=" + String(fragmentationBeforePost) + "%";
#endif
    } else {
      lastError_ = "HTTPS upload rejected " + String(status);
    }
    httpsConnected_ = false;
    return false;
  }
  if (responseBody != nullptr) {
    *responseBody = body;
  }
  lastPostRetryable_ = false;
  return true;
}

void LiveUpload::consumeHttpsDesiredConfig(const String &responseBody) {
  if (!remoteManagementEnabled_ || responseBody.isEmpty()) {
    return;
  }
  StaticJsonDocument<kHttpsResponseJsonCapacity> document;
  if (deserializeJson(document, responseBody) != DeserializationError::Ok ||
      !document["desired_config"].is<JsonObject>()) {
    return;
  }
  String encoded;
  serializeJson(document["desired_config"], encoded);
  RemoteConfig candidate{};
  if (!parseRemoteConfig(reinterpret_cast<const uint8_t *>(encoded.c_str()), encoded.length(), candidate) ||
      candidate.version <= appliedConfigVersion_) {
    return;
  }
  pendingRemoteConfig_ = candidate;
  hasPendingRemoteConfig_ = true;
  managementStatus_ = "pending";
  managementError_ = "";
}

String LiveUpload::liveTopic() const {
  return String(Logic::formatUploadTopic(config_.topicPrefix, deviceId_.c_str(), "live").c_str());
}

String LiveUpload::statusTopic() const {
  return String(Logic::formatUploadTopic(config_.topicPrefix, deviceId_.c_str(), "status").c_str());
}

String LiveUpload::desiredConfigTopic() const {
  String prefix = config_.topicPrefix;
  while (prefix.startsWith("/")) {
    prefix.remove(0, 1);
  }
  while (prefix.endsWith("/")) {
    prefix.remove(prefix.length() - 1);
  }
  return prefix + "/" + deviceId_ + "/config/desired";
}

String LiveUpload::buildStatusJson(const bool connected) const {
  String json = "{";
  json += "\"schema_version\":" + String(Logic::kLivePayloadSchemaVersion) + ",";
  json += "\"device_id\":\"" + jsonEscape(deviceId_) + "\",";
  json += "\"session_id\":\"" + jsonEscape(sessionId_) + "\",";
  json += "\"protocol\":\"" + protocolName() + "\",";
  json += "\"connected\":" + String(connected ? "true" : "false");
  if (remoteManagementEnabled_) {
    json += ",\"management\":{";
    json += "\"enabled\":true,";
    json += "\"pairing_code\":\"" + jsonEscape(pairingCode_) + "\",";
    json += "\"pairing_expires_in_s\":" + String(pairingCodeExpiresInSeconds()) + ",";
    json += "\"config_version\":" + String(appliedConfigVersion_) + ",";
    json += "\"status\":\"" + jsonEscape(managementStatus_) + "\",";
    json += "\"error\":\"" + jsonEscape(managementError_) + "\",";
    json += "\"firmware_version\":\"" + jsonEscape(APEXI_FIRMWARE_VERSION) + "\",";
    json += "\"settings\":{";
    json += "\"upload_enabled\":" + String(reportedUploadEnabled_ ? "true" : "false") + ",";
    json += "\"ntp_primary\":\"" + jsonEscape(reportedNtpPrimary_) + "\",";
    json += "\"ntp_secondary\":\"" + jsonEscape(reportedNtpSecondary_) + "\",";
    json += "\"tz_rule\":\"" + jsonEscape(reportedTimeZoneRule_) + "\",";
    json += "\"tz_label\":\"" + jsonEscape(reportedTimeZoneLabel_) + "\"}";
    json += "}";
  }
  json += "}";
  return json;
}

String LiveUpload::buildSnapshotJson(const AppState &state, const uint32_t sequence) const {
  String json = "{";
  json += "\"schema_version\":" + String(Logic::kLivePayloadSchemaVersion) + ",";
  json += "\"device_id\":\"" + jsonEscape(deviceId_) + "\",";
  json += "\"session_id\":\"" + jsonEscape(sessionId_) + "\",";
  json += "\"sequence\":" + String(sequence) + ",";
  json += "\"timestamp\":\"" + jsonEscape(state.transportTimestamp) + "\",";
  json += "\"uptime_ms\":" + String(state.uptimeMs) + ",";
  json += "\"sensors\":[";
  for (size_t index = 0; index < state.sensors.size(); ++index) {
    if (index > 0) {
      json += ",";
    }
    const SensorSnapshot &sensor = state.sensors[index];
    json += "{";
    json += "\"id\":\"" + jsonEscape(sensor.id) + "\",";
    json += "\"name\":\"" + jsonEscape(sensor.name) + "\",";
    json += "\"value\":" + String(sensor.filteredValue, 3) + ",";
    json += "\"units\":\"" + jsonEscape(sensor.units) + "\",";
    json += "\"loop_mA\":" + String(sensor.loopCurrentmA, 3) + ",";
    json += "\"fault\":\"" + jsonEscape(sensorFaultToString(sensor.activeFault)) + "\"";
    json += "}";
  }
  json += "],";
  json += "\"system\":{";
  json += "\"adc_ready\":" + String(state.system.adcReady ? "true" : "false") + ",";
  json += "\"rtc_ready\":" + String(state.system.rtcReady ? "true" : "false") + ",";
  json += "\"sd_ready\":" + String(state.system.sdReady ? "true" : "false") + ",";
  json += "\"wifi_ready\":" + String(state.system.wifiReady ? "true" : "false");
  json += "}";
  json += "}";
  return json;
}

String LiveUpload::jsonEscape(const String &value) {
  String escaped;
  escaped.reserve(value.length() + 8);
  for (size_t index = 0; index < value.length(); ++index) {
    const char ch = value[index];
    switch (ch) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        escaped += ch;
        break;
    }
  }
  return escaped;
}
