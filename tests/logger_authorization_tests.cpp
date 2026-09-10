#include "LoggerAuthorization.h"
#include "DeviceAuthorizationPolicy.h"
#include "HttpsSlot.h"
#include <cassert>
#include <iostream>
struct AuthorizationTestAccess {
 static bool resetRecord(const String &saved,String &out){return LoggerAuthorization::ownerResetRecord(saved,out);}
 static void seed(LoggerAuthorization &a){
  a.hardwareId_="esp32-aabbccddeeff";a.installationId_="12345678-1234-4234-8234-123456789012";
  a.installationSecret_=std::string(64,'a');a.origin_=a.storedOrigin_="app.example.test:443";
  a.activeToken_=a.storedToken_=std::string(40,'o');a.deviceId_=a.storedDeviceId_="mda-logger";
  a.config_.appDeviceToken=a.activeToken_.c_str();a.credentialVersion_=1;a.ready_=true;
 }
 static void response(LoggerAuthorization &a,int status,const char *body,bool start=false,bool ack=false){a.accept(status,body,start,ack);}
 static bool pendingAck(LoggerAuthorization &a){return a.ackPending_;}
 static bool restore(LoggerAuthorization &a){return a.restoreRecord(AuthTestStore::record);}
 static void differentHost(LoggerAuthorization &a){a.origin_="other.example.test:443";}
 static void verify(LoggerAuthorization &a,const char *body){a.accept(200,body,false,false,true);}
 static bool verifying(LoggerAuthorization &a){return a.verifyPending_;}
};
static JsonDocument &record(){static StaticJsonDocument<4096>d;d.clear();assert(!deserializeJson(d,AuthTestStore::record));return d;}
static const char *challenge=R"({"authorization_id":"12345678-1234-4234-8234-123456789012","device_code":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","user_code":"ABCD-EFGH","expires_in":600,"interval":5})";
static const char *issued=R"({"status":"authorized","token_type":"Bearer","access_token":"nnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnn","device_id":"mda-logger","credential_version":2})";
int main(){
 {HttpsSlot first;assert(first);{HttpsSlot blocked;assert(!blocked);}HttpsSlot stillBlocked;assert(!stillBlocked);}
 {HttpsSlot released;assert(released);}
 using namespace DeviceAuthorizationPolicy;
 assert(hardwareId(0xaabbccddeeffULL)=="esp32-aabbccddeeff");assert(hardwareId(1)=="esp32-000000000001");
 assert(deviceId("mda-logger")&&!deviceId("../bad")&&!deviceId("Upper"));
 assert(!printable("header\r\ninjection",1,100));assert(pollDelay(0)==5000&&pollDelay(100)==60000);
 assert(elapsed(10,UINT32_MAX-10,20));
 LoggerAuthorization a;AuthorizationTestAccess::seed(a);
 AuthorizationTestAccess::response(a,201,challenge,true);
 assert(a.status()=="awaiting_approval");assert(record()["device_code"].is<const char *>());
 a.statusResult(false);assert(a.status()=="awaiting_approval"); //401 cannot cancel owner approval polling
 AuthTestStore::fail=true;AuthorizationTestAccess::response(a,200,issued);
 assert(!AuthorizationTestAccess::pendingAck(a));assert(!a.restartRequired());
 AuthTestStore::fail=false;AuthorizationTestAccess::response(a,200,issued);
 assert(AuthorizationTestAccess::pendingAck(a));assert(record()["access_token"].as<String>()==std::string(40,'o'));
  assert(record()["bootstrap_token"].as<String>()==std::string(40,'n'));
 String resetRecord;
 assert(AuthorizationTestAccess::resetRecord(AuthTestStore::record,resetRecord));
 StaticJsonDocument<1024> resetIdentity;assert(!deserializeJson(resetIdentity,resetRecord));
 assert(resetIdentity.size()==3);
 assert(resetIdentity["installation_id"].as<String>()=="12345678-1234-4234-8234-123456789012");
 assert(!resetIdentity.containsKey("access_token")&&!resetIdentity.containsKey("bootstrap_token")&&!resetIdentity.containsKey("device_code"));
 assert(!AuthorizationTestAccess::resetRecord("broken",resetRecord));
 assert(!AuthorizationTestAccess::resetRecord("{}",resetRecord));
 LoggerAuthorization rebooted;AuthorizationTestAccess::seed(rebooted);assert(AuthorizationTestAccess::restore(rebooted));
 assert(AuthorizationTestAccess::pendingAck(rebooted));assert(String(rebooted.bearer())==std::string(40,'o'));
 assert(String(a.bearer())==std::string(40,'o'));
 AuthTestStore::fail=true;AuthorizationTestAccess::response(a,200,R"({"status":"acknowledged"})",false,true);
 assert(!a.restartRequired()&&AuthorizationTestAccess::pendingAck(a));
 AuthTestStore::fail=false;AuthorizationTestAccess::response(a,200,R"({"status":"acknowledged"})",false,true);
  assert(a.restartRequired());assert(record()["access_token"].as<String>()==std::string(40,'n'));
 LoggerAuthorization approvedReboot;AuthorizationTestAccess::seed(approvedReboot);assert(AuthorizationTestAccess::restore(approvedReboot));
 assert(String(approvedReboot.bearer())==std::string(40,'n'));assert(!AuthorizationTestAccess::pendingAck(approvedReboot));
 LoggerAuthorization moved;AuthorizationTestAccess::seed(moved);AuthorizationTestAccess::differentHost(moved);
 assert(AuthorizationTestAccess::restore(moved));assert(String(moved.bearer()).isEmpty());
 LoggerAuthorization b;AuthorizationTestAccess::seed(b);AuthorizationTestAccess::response(b,201,challenge,true);
 AuthorizationTestAccess::response(b,200,issued);AuthorizationTestAccess::response(b,410,"{}");
  assert(!b.restartRequired()&&!AuthorizationTestAccess::pendingAck(b));assert(record()["access_token"].as<String>()==std::string(40,'o'));
 LoggerAuthorization lostAck;AuthorizationTestAccess::seed(lostAck);AuthorizationTestAccess::response(lostAck,201,challenge,true);
 AuthorizationTestAccess::response(lostAck,200,issued);AuthorizationTestAccess::response(lostAck,410,"{}",false,true);
 assert(AuthorizationTestAccess::verifying(lostAck)&&!lostAck.restartRequired());
 AuthorizationTestAccess::verify(lostAck,R"({"actor":{"authenticated":true,"auth_method":"bearer","subject":"logger:mda-logger","scopes":["logger:ingest"]}})");
 assert(lostAck.restartRequired());
 LoggerAuthorization wrongIdentity;AuthorizationTestAccess::seed(wrongIdentity);AuthorizationTestAccess::response(wrongIdentity,201,challenge,true);
 AuthorizationTestAccess::response(wrongIdentity,200,issued);AuthorizationTestAccess::response(wrongIdentity,410,"{}",false,true);
 AuthorizationTestAccess::verify(wrongIdentity,R"({"actor":{"authenticated":true,"auth_method":"bearer","subject":"logger:other","scopes":["logger:ingest"]}})");
 assert(!wrongIdentity.restartRequired()&&!AuthorizationTestAccess::pendingAck(wrongIdentity));
 LoggerAuthorization c;AuthorizationTestAccess::seed(c);
 StaticJsonDocument<1024> rotation;rotation["type"]="app_bearer";rotation["version"]=3;rotation["nonce"]="nonce-0003";
 rotation["token"]=std::string(40,'r');rotation["overlap_expires_at"]="2026-09-10T00:00:00Z";
 AuthTestStore::fail=true;assert(!c.stageRotation(rotation.as<JsonObjectConst>()));assert(String(c.bearer(true))==std::string(40,'o'));
 AuthTestStore::fail=false;assert(c.stageRotation(rotation.as<JsonObjectConst>()));
 assert(String(c.bearer(true))==std::string(40,'o'));c.statusResult(true);
  assert(String(c.bearer(true))==std::string(40,'r'));assert(String(c.bearer())==std::string(40,'o'));
 LoggerAuthorization rotatedReboot;AuthorizationTestAccess::seed(rotatedReboot);assert(AuthorizationTestAccess::restore(rotatedReboot));
 assert(String(rotatedReboot.bearer(true))==std::string(40,'r'));assert(String(rotatedReboot.bearer())==std::string(40,'o'));
 c.statusResult(false);assert(!c.restartRequired());c.statusResult(true);assert(c.restartRequired());
  assert(record()["access_token"].as<String>()==std::string(40,'r'));
 LoggerAuthorization sparse;AuthorizationTestAccess::seed(sparse);
 StaticJsonDocument<2048> envelope;envelope["schema_version"]=1;envelope["device_id"]="other";envelope["credential_rotation"]=rotation;
 assert(!sparse.stageDesired(envelope.as<JsonObjectConst>()));envelope["device_id"]="mda-logger";
 envelope["schema_version"]="1";assert(!sparse.stageDesired(envelope.as<JsonObjectConst>()));
 envelope["schema_version"]=1;assert(sparse.stageDesired(envelope.as<JsonObjectConst>()));
 assert(!envelope.containsKey("settings")); // Security-only envelope needs no remote configuration.
 sparse.statusResult(true);assert(sparse.hasRotation());sparse.beginRotationFallback();
 assert(sparse.rotationAcknowledgement().isEmpty());assert(String(sparse.bearer(true))==std::string(40,'o'));
 assert(!sparse.finishRotationFallback(false));assert(sparse.hasRotation());
 sparse.beginRotationFallback();AuthTestStore::fail=true;assert(!sparse.finishRotationFallback(true));assert(sparse.hasRotation());
 AuthTestStore::fail=false;sparse.beginRotationFallback();assert(sparse.finishRotationFallback(true));assert(!sparse.hasRotation());
 assert(record()["access_token"].as<String>()==std::string(40,'o'));
 std::cout<<"Logger authorization persistence and rotation tests passed\n";
}
