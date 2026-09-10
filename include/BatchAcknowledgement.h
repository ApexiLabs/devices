#pragma once
#include <ArduinoJson.h>
#include <cstring>

namespace BatchAcknowledgement {
inline bool accepted(int status,const char *body,const char *batchId,size_t count) {
  if(status!=200 || !count || count>8 || !batchId || std::strlen(batchId)!=32)return false;
  StaticJsonDocument<192> filter;filter["status"]=true;filter["accepted"]=true;filter["batch_id"]=true;filter["accepted_count"]=true;
  StaticJsonDocument<384> doc;
  if(deserializeJson(doc,body,DeserializationOption::Filter(filter)))return false;
  return doc["status"]=="ok" && doc["accepted"].is<bool>() && doc["accepted"].as<bool>() &&
    doc["batch_id"].is<const char *>() && std::strcmp(doc["batch_id"].as<const char *>(),batchId)==0 &&
    doc["accepted_count"].is<unsigned>() && doc["accepted_count"].as<unsigned>()==count;
}
}
