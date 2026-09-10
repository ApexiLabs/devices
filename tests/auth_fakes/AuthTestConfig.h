#pragma once
#include <Arduino.h>
namespace AppConfig {
struct UploadConfig {
 enum class Protocol {Https,Mqtt};Protocol protocol=Protocol::Https;
 const char *mqttHost="app.example.test";uint16_t mqttPort=443;
 const char *appDeviceToken="";const char *deviceId="mda-logger";
 const char *cloudflareAccessClientId="";const char *cloudflareAccessClientSecret="";
};
}
namespace AuthTestStore {inline bool fail=false;inline String record;}
