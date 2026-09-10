#pragma once
#include <ArduinoJson.h>
#include <cstring>
namespace UploadEvidence {
inline bool acceptedResponse(int status,const char *body) {
  if(status!=200) return false;
  StaticJsonDocument<512> doc;
  StaticJsonDocument<96> filter;
  filter["accepted"]=true;filter["status"]=true;
  if(deserializeJson(doc,body,DeserializationOption::Filter(filter)) || !doc.is<JsonObject>()) return false;
  return doc["accepted"].is<bool>() && doc["accepted"].as<bool>() &&
      doc["status"].is<const char *>() && std::strcmp(doc["status"].as<const char *>(),"ok")==0;
}
}
