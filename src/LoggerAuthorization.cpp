#include "LoggerAuthorization.h"
#include "DeviceAuthorizationPolicy.h"
#include "HttpsSlot.h"
#include "HttpsWorker.h"
#if defined(ESP32)
#include "LiveUpload.h"
#endif
#include <ArduinoJson.h>
#include <ctime>
#if defined(ESP32)
#include <Preferences.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_system.h>
#endif

namespace {
#if defined(ESP32)
constexpr const char *kNamespace="logger_authz";
String randomHex(size_t count) {
  String out;out.reserve(count*2);
  for(size_t i=0;i<count;++i){char b[3];snprintf(b,sizeof(b),"%02x",unsigned(esp_random()&255));out+=b;}
  return out;
}
String randomUuid() {
  String s=randomHex(16);s.setCharAt(12,'4');s.setCharAt(16,"89ab"[esp_random()&3]);
  return s.substring(0,8)+"-"+s.substring(8,12)+"-"+s.substring(12,16)+"-"+s.substring(16,20)+"-"+s.substring(20);
}
#endif
}

bool LoggerAuthorization::begin(const AppConfig::UploadConfig &config) {
  config_=config;
  serverHost_=config.mqttHost;cloudflareId_=config.cloudflareAccessClientId;cloudflareSecret_=config.cloudflareAccessClientSecret;
  config_.mqttHost=serverHost_.c_str();config_.cloudflareAccessClientId=cloudflareId_.c_str();config_.cloudflareAccessClientSecret=cloudflareSecret_.c_str();
#if !defined(ESP32)
  status_="unsupported";return false;
#else
  config_.appDeviceToken=""; // Issued app credentials are NVS state, never firmware defaults.
  hardwareId_=DeviceAuthorizationPolicy::hardwareId(ESP.getEfuseMac()).c_str();
  origin_=String(config.mqttHost)+":"+String(config.mqttPort);
  csrf_=randomHex(24);
  Preferences store;
  if(!store.begin(kNamespace,false)){error_="Credential storage unavailable";return false;}
  String saved=store.getString("record","");
  DynamicJsonDocument doc(4096);
  if(!saved.isEmpty() && (deserializeJson(doc,saved) || doc["hardware_id"].as<String>()!=hardwareId_)) {
    store.end();error_="Credential record invalid; preserved for recovery";return false;
  }
  installationId_=doc["installation_id"].as<String>();
  installationSecret_=doc["installation_secret"].as<String>();
  if(saved.isEmpty()) {
    installationId_=randomUuid();installationSecret_=randomHex(32);
    doc["hardware_id"]=hardwareId_;doc["installation_id"]=installationId_;doc["installation_secret"]=installationSecret_;
    saved="";serializeJson(doc,saved);
    if(store.putString("record",saved)!=saved.length()){store.end();error_="Cannot persist device identity";return false;}
  }
  store.end();
  if(!restoreRecord(saved))return false;
  if(!HttpsWorker::shared().begin(LiveUpload::trustRoot())){error_="HTTPS worker unavailable";return false;}
  ready_=true;status_=activeToken_.isEmpty()?"authorization_required":"credential_stored";
  if(!authorizationId_.isEmpty() && !deviceCode_.isEmpty()) {
    const time_t now=time(nullptr);
    expiresMs_=now>1000000000 && expiresEpoch_? (expiresEpoch_>uint32_t(now)?min(uint32_t(600),expiresEpoch_-uint32_t(now))*1000:1):600000;
    startedMs_=millis();status_=ackPending_?"credential_saved":"awaiting_approval";
  }
  return true;
#endif
}

bool LoggerAuthorization::restoreRecord(const String &saved) {
  DynamicJsonDocument doc(4096);
  config_.appDeviceToken="";
  activeToken_="";deviceId_="";candidate_="";rotationPhase_=0;
  if(deserializeJson(doc,saved) || doc["hardware_id"].as<String>()!=hardwareId_) {
    error_="Credential record identity mismatch";return false;
  }
  installationId_=doc["installation_id"].as<String>();
  installationSecret_=doc["installation_secret"].as<String>();
  if(!DeviceAuthorizationPolicy::uuid(installationId_.c_str()) || !DeviceAuthorizationPolicy::hex(installationSecret_.c_str(),64)) {error_="Invalid installation identity";return false;}
  storedOrigin_=doc["origin"].as<String>();
  storedToken_=doc["access_token"].as<String>();storedDeviceId_=doc["device_id"].as<String>();credentialVersion_=doc["credential_version"]|0U;
  if(storedOrigin_==origin_) {
    activeToken_=doc["access_token"].as<String>();deviceId_=doc["device_id"].as<String>();
    if(!activeToken_.isEmpty() && (!DeviceAuthorizationPolicy::printable(activeToken_.c_str(),32,511) ||
       !DeviceAuthorizationPolicy::deviceId(deviceId_.c_str()))) {error_="Invalid stored credential";return false;}
    storedToken_=activeToken_;storedDeviceId_=deviceId_;credentialVersion_=doc["credential_version"]|0U;
    candidate_=doc["candidate"].as<String>();rotationNonce_=doc["rotation_nonce"].as<String>();rotationExpiry_=doc["rotation_expiry"].as<String>();
    rotationVersion_=doc["rotation_version"]|0U;rotationPhase_=doc["rotation_phase"]|0;
    appliedRotationVersion_=doc["applied_rotation_version"]|0U;appliedRotationNonce_=doc["applied_rotation_nonce"].as<String>();
    if(rotationPhase_>2 || (rotationPhase_ && (!DeviceAuthorizationPolicy::printable(candidate_.c_str(),32,511)||
       !DeviceAuthorizationPolicy::printable(rotationNonce_.c_str(),8,96)||rotationVersion_==0))) {
      error_="Invalid rotation record; preserved";return false;
    }
  }
  if(doc["pending_origin"].as<String>()==origin_) {
    authorizationId_=doc["authorization_id"].as<String>();deviceCode_=doc["device_code"].as<String>();
    userCode_=doc["user_code"].as<String>();ackPending_=doc["ack_pending"]|false;expiresEpoch_=doc["expires_epoch"]|0U;
    bootstrapToken_=doc["bootstrap_token"].as<String>();bootstrapDeviceId_=doc["bootstrap_device_id"].as<String>();bootstrapVersion_=doc["bootstrap_version"]|0U;
    if(ackPending_ && (!DeviceAuthorizationPolicy::printable(bootstrapToken_.c_str(),32,511)||
       !DeviceAuthorizationPolicy::deviceId(bootstrapDeviceId_.c_str())||authorizationId_.length()!=36||
       !DeviceAuthorizationPolicy::printable(deviceCode_.c_str(),32,256))) {error_="Invalid pending credential; preserved";return false;}
  }
  // Once a stored credential exists it is never replaced by a compiled fallback,
  // including when the configured server changes.
  if(doc.containsKey("access_token")) {
    config_.appDeviceToken=activeToken_.c_str();
    if(!deviceId_.isEmpty())config_.deviceId=deviceId_.c_str();
  }
  return true;
}

bool LoggerAuthorization::requestAuthorization() {
  if(!ready_||busy_||restart_){error_="Authorization is busy or unavailable";return false;}
  if(config_.protocol!=AppConfig::UploadConfig::Protocol::Https){error_="Authorization requires HTTPS";return false;}
  if(ackPending_ || (status_=="awaiting_approval" && expiresIn()>0))return true;
  authorizationId_="";deviceCode_="";userCode_="";error_="";
  status_="requesting_code";
#if defined(ESP32)
  startPending_=true;
  if(send(true))startPending_=false;
  return true;
#else
  return send(true);
#endif
}

uint32_t LoggerAuthorization::expiresIn() const {
  if(!expiresMs_)return 0;
  const uint32_t age=millis()-startedMs_;return age>=expiresMs_?0:(expiresMs_-age+999)/1000;
}

bool LoggerAuthorization::send(bool start,bool ack,bool verify) {
#if !defined(ESP32)
  (void)start;(void)ack;(void)verify;return false;
#else
  if(WiFi.status()!=WL_CONNECTED){error_="Wi-Fi disconnected";return false;}
  DynamicJsonDocument doc(1536);
  if(start) {
    doc["hardware_id"]=hardwareId_;doc["installation_id"]=installationId_;
    doc["device_name"]="Apexi Logger";
  } else {doc["authorization_id"]=authorizationId_;doc["device_code"]=deviceCode_;}
  doc["installation_secret"]=installationSecret_;
  String body;serializeJson(doc,body);
  const String url="https://"+serverHost_+":"+String(config_.mqttPort)+(verify?String("/auth/me"):String("/api/v1/logger-authorization/")+(start?"start":ack?"ack":"poll"));
  if(!HttpsWorker::shared().submit(HttpsWorker::Owner::Authorization,url.c_str(),body.c_str(),verify?bootstrapToken_.c_str():"",
      cloudflareId_.c_str(),cloudflareSecret_.c_str(),verify)){error_="HTTPS worker busy";return false;}
  requestStart_=start;requestAck_=ack;requestVerify_=verify;
  busy_=true;lastPollMs_=millis();return true;
#endif
}

bool LoggerAuthorization::saveRecord(const String &token,const String &deviceId,uint32_t version) {
#if !defined(ESP32) && !defined(APEXI_AUTH_HOST_TEST)
  (void)token;(void)deviceId;(void)version;return false;
#else
  DynamicJsonDocument doc(4096);
  doc["hardware_id"]=hardwareId_;doc["installation_id"]=installationId_;doc["installation_secret"]=installationSecret_;
  doc["origin"]=storedOrigin_;doc["access_token"]=token;doc["device_id"]=deviceId;doc["credential_version"]=version;
  if(token.isEmpty()){doc.remove("access_token");doc.remove("device_id");}
  doc["authorization_id"]=authorizationId_;doc["device_code"]=deviceCode_;doc["user_code"]=userCode_;
  doc["ack_pending"]=ackPending_;doc["expires_epoch"]=expiresEpoch_;
  doc["pending_origin"]=origin_;doc["bootstrap_token"]=bootstrapToken_;doc["bootstrap_device_id"]=bootstrapDeviceId_;doc["bootstrap_version"]=bootstrapVersion_;
  doc["candidate"]=candidate_;doc["rotation_nonce"]=rotationNonce_;doc["rotation_expiry"]=rotationExpiry_;
  doc["rotation_version"]=rotationVersion_;doc["rotation_phase"]=rotationPhase_;
  doc["applied_rotation_version"]=appliedRotationVersion_;doc["applied_rotation_nonce"]=appliedRotationNonce_;
  String value;serializeJson(doc,value);
  if(doc.overflowed())return false;
#if defined(APEXI_AUTH_HOST_TEST)
  if(AuthTestStore::fail)return false;
  AuthTestStore::record=value;return true;
#else
  Preferences store;if(!store.begin(kNamespace,false))return false;
  // One NVS value replaces the complete record atomically; no half token/ID pair.
  const bool ok=store.putString("record",value)==value.length() && store.getString("record","")==value;
  store.end();return ok;
#endif
#endif
}

void LoggerAuthorization::accept(int code,const String &body,bool start,bool ack,bool verify) {
  DynamicJsonDocument doc(4096);
  if(code==429){pollMs_=min(uint32_t(60000),pollMs_+5000);error_="Authorization rate limited; retrying";return;}
  if(code<=0 || code>=500){error_="Authorization server unavailable; retrying";if(start)status_="request_failed";return;}
  if(code==410 && ack && ackPending_) {
    verifyPending_=true;status_="verifying_saved_credential";error_="Acknowledgement expired; checking saved credential";return;
  }
  if(verify && code==200) {
    if(deserializeJson(doc,body)){error_="Invalid credential verification response";return;}
    JsonObjectConst actor=doc["actor"].as<JsonObjectConst>();bool scope=false;
    for(JsonVariantConst item:actor["scopes"].as<JsonArrayConst>())if(item=="logger:ingest")scope=true;
    if(actor["authenticated"].is<bool>() && actor["authenticated"].as<bool>() && actor["auth_method"]=="bearer" &&
       actor["subject"].as<String>()==String((std::string("logger:")+bootstrapDeviceId_.c_str()).c_str()) && scope) {
      verifyPending_=false;accept(200,"{\"status\":\"acknowledged\"}",false,true);return;
    }
    verifyPending_=false;accept(410,"{}",false,false);return;
  }
  if(code==410||code==403||code==401) {
    status_=code==410?"expired":code==403?"denied":"authorization_required";
    error_="Authorization expired, denied or proof rejected";deviceCode_="";userCode_="";authorizationId_="";ackPending_=false;
    bootstrapToken_="";bootstrapDeviceId_="";bootstrapVersion_=0;
    verifyPending_=false;
    saveRecord(storedToken_,storedDeviceId_,credentialVersion_);return;
  }
  if(deserializeJson(doc,body)||!doc.is<JsonObject>()){error_="Invalid authorization response";return;}
  if(ack && code==200 && doc["status"]=="acknowledged") {
    const String oldId=authorizationId_,oldProof=deviceCode_;
    const String oldOrigin=storedOrigin_;storedOrigin_=origin_;
    const auto oldPhase=rotationPhase_;const String oldCandidate=candidate_,oldAppliedNonce=appliedRotationNonce_;
    const auto oldAppliedVersion=appliedRotationVersion_;
    rotationPhase_=0;candidate_="";appliedRotationVersion_=0;appliedRotationNonce_="";
    authorizationId_="";deviceCode_="";userCode_="";ackPending_=false;
    if(!saveRecord(bootstrapToken_,bootstrapDeviceId_,bootstrapVersion_)) {
      storedOrigin_=oldOrigin;
      rotationPhase_=oldPhase;candidate_=oldCandidate;appliedRotationVersion_=oldAppliedVersion;appliedRotationNonce_=oldAppliedNonce;
      authorizationId_=oldId;deviceCode_=oldProof;ackPending_=true;error_="Cannot save acknowledgement; retrying";return;
    }
    status_="authorized";error_="";restart_=true;return;
  }
  if(start && code==201) {
    const String id=doc["authorization_id"].as<String>(),proof=doc["device_code"].as<String>(),user=doc["user_code"].as<String>();
    if(!doc["authorization_id"].is<const char *>()||!doc["device_code"].is<const char *>()||!doc["user_code"].is<const char *>()||
       !DeviceAuthorizationPolicy::uuid(id.c_str()) || !DeviceAuthorizationPolicy::printable(proof.c_str(),32,256) ||
       !DeviceAuthorizationPolicy::printable(user.c_str(),8,16) || !doc["expires_in"].is<uint32_t>() ||
       doc["expires_in"].as<uint32_t>()==0 || doc["expires_in"].as<uint32_t>()>600) {error_="Invalid authorization challenge";return;}
    authorizationId_=id;deviceCode_=proof;userCode_=user;
    expiresMs_=doc["expires_in"].as<uint32_t>()*1000;startedMs_=millis();
    expiresEpoch_=time(nullptr)>1000000000?uint32_t(time(nullptr))+expiresMs_/1000:0;
    pollMs_=DeviceAuthorizationPolicy::pollDelay(doc["interval"]|5U);
    if(!saveRecord(storedToken_,storedDeviceId_,credentialVersion_)) {
      authorizationId_="";deviceCode_="";userCode_="";status_="storage_fault";error_="Cannot save authorization challenge";return;
    }
    status_="awaiting_approval";error_="";return;
  }
  if(!start && code==202 && doc["status"]=="authorization_pending") {error_="";return;}
  if(!start && !ack && code==200 && doc["status"]=="authorized" && doc["token_type"]=="Bearer") {
    const String token=doc["access_token"].as<String>(),id=doc["device_id"].as<String>();
    if(!doc["access_token"].is<const char *>()||!doc["device_id"].is<const char *>()||
       !DeviceAuthorizationPolicy::printable(token.c_str(),32,511)||!DeviceAuthorizationPolicy::deviceId(id.c_str())||
       !doc["credential_version"].is<uint32_t>()){error_="Invalid issued credential";return;}
    ackPending_=true;
    bootstrapToken_=token;bootstrapDeviceId_=id;bootstrapVersion_=doc["credential_version"].as<uint32_t>();
    if(!saveRecord(storedToken_,storedDeviceId_,credentialVersion_)){ackPending_=false;error_="Cannot save credential; retrying delivery";return;}
    status_="credential_saved";error_="";return;
  }
  error_="Unexpected authorization response";
}

void LoggerAuthorization::loop() {
#if defined(ESP32)
  if(!ready_)return;
  if(startPending_ && !busy_ && send(true))startPending_=false;
  if(const auto *result=HttpsWorker::shared().result(HttpsWorker::Owner::Authorization)){
    busy_=false;accept(result->status,result->body,requestStart_,requestAck_,requestVerify_);
    if(result->status<=0)error_="Authorization transport error "+String(result->status)+"; TLS "+String(result->tlsError);
    HttpsWorker::shared().release(HttpsWorker::Owner::Authorization);
  }
  if(ackPending_ && !busy_ && DeviceAuthorizationPolicy::elapsed(millis(),lastPollMs_,pollMs_))send(false,!verifyPending_,verifyPending_);
  if(status_=="awaiting_approval" && !busy_) {
    if(!expiresIn()){HttpsWorker::shared().cancelPending(HttpsWorker::Owner::Authorization);status_="expired";deviceCode_="";userCode_="";authorizationId_="";saveRecord(storedToken_,storedDeviceId_,credentialVersion_);return;}
    if(DeviceAuthorizationPolicy::elapsed(millis(),lastPollMs_,pollMs_))send(false);
  }
#endif
}

const char *LoggerAuthorization::bearer(bool statusRequest) const {
  return statusRequest && rotationPhase_==2 && !fallback_?candidate_.c_str():config_.appDeviceToken;
}

String LoggerAuthorization::rotationAcknowledgement() const {
  if(fallback_)return "";
  const uint32_t version=rotationPhase_?rotationVersion_:appliedRotationVersion_;
  const String &nonce=rotationPhase_?rotationNonce_:appliedRotationNonce_;
  if(!version||nonce.isEmpty())return "";
  StaticJsonDocument<256> doc;doc["version"]=version;doc["nonce"]=nonce;doc["state"]="applied";
  String json;serializeJson(doc,json);return json;
}

bool LoggerAuthorization::stageRotation(JsonObjectConst rotation) {
  if(restart_)return true;
  if(rotation.isNull())return true;
  if(rotation["type"]!="app_bearer"||!rotation["version"].is<uint32_t>()||!rotation["token"].is<const char *>()||
     !rotation["nonce"].is<const char *>()||!rotation["overlap_expires_at"].is<const char *>())return false;
  const uint32_t version=rotation["version"].as<uint32_t>();
  const String token=rotation["token"].as<String>(),nonce=rotation["nonce"].as<String>(),expiry=rotation["overlap_expires_at"].as<String>();
  if(!version||!DeviceAuthorizationPolicy::printable(token.c_str(),32,511)||
     !DeviceAuthorizationPolicy::printable(nonce.c_str(),8,96)||!DeviceAuthorizationPolicy::printable(expiry.c_str(),20,40))return false;
  if(version==appliedRotationVersion_)return token==storedToken_&&nonce==appliedRotationNonce_;
  if(rotationPhase_)return version==rotationVersion_&&token==candidate_&&nonce==rotationNonce_;
  if(version<appliedRotationVersion_||token==storedToken_||storedToken_.isEmpty()||ackPending_)return false;
  candidate_=token;rotationNonce_=nonce;rotationExpiry_=expiry;rotationVersion_=version;rotationPhase_=1;
  if(!saveRecord(storedToken_,storedDeviceId_,credentialVersion_)) {
    candidate_="";rotationNonce_="";rotationExpiry_="";rotationVersion_=0;rotationPhase_=0;error_="Cannot stage rotated credential";return false;
  }
  status_="rotation_staged";return true;
}

bool LoggerAuthorization::stageDesired(JsonObjectConst envelope) {
  if(!envelope.containsKey("credential_rotation"))return true;
  if(!envelope["schema_version"].is<uint32_t>()||envelope["schema_version"].as<uint32_t>()!=1||
     !envelope["device_id"].is<const char *>()||envelope["device_id"].as<String>()!=String(config_.deviceId)||
     !envelope["credential_rotation"].is<JsonObjectConst>())return false;
  return stageRotation(envelope["credential_rotation"].as<JsonObjectConst>());
}

void LoggerAuthorization::statusResult(bool accepted) {
  if(restart_)return;
  if(!accepted){if(!rotationPhase_ && authorizationId_.isEmpty() && !busy_)status_=activeToken_.isEmpty()?"authorization_required":"connection_failed";return;}
  if(rotationPhase_==1) {
    rotationPhase_=2;
    if(!saveRecord(storedToken_,storedDeviceId_,credentialVersion_)){rotationPhase_=1;error_="Cannot persist rotation acknowledgement";}
    else status_="rotation_verifying";
  } else if(rotationPhase_==2) {
    const uint32_t oldVersion=appliedRotationVersion_;const String oldNonce=appliedRotationNonce_;
    appliedRotationVersion_=rotationVersion_;appliedRotationNonce_=rotationNonce_;rotationPhase_=0;
    if(!saveRecord(candidate_,storedDeviceId_,rotationVersion_)) {
      rotationPhase_=2;appliedRotationVersion_=oldVersion;appliedRotationNonce_=oldNonce;error_="Cannot commit rotated credential";return;
    }
    // Restart reloads every transport client from the same durable credential.
    status_="rotation_complete";restart_=true;
  } else if(!busy_ && status_!="awaiting_approval" && !ackPending_)status_="connected";
}

bool LoggerAuthorization::finishRotationFallback(bool oldAccepted) {
  fallback_=false;
  if(!oldAccepted)return false;
  const auto phase=rotationPhase_;const String token=candidate_,nonce=rotationNonce_,expiry=rotationExpiry_;
  const uint32_t version=rotationVersion_;
  rotationPhase_=0;candidate_="";rotationNonce_="";rotationExpiry_="";rotationVersion_=0;
  if(!saveRecord(storedToken_,storedDeviceId_,credentialVersion_)) {
    rotationPhase_=phase;candidate_=token;rotationNonce_=nonce;rotationExpiry_=expiry;rotationVersion_=version;
    error_="Cannot clear rejected rotation; old credential retained";return false;
  }
  status_="connected";error_="Rejected rotation cleared; old credential retained";return true;
}
