#pragma once
#include <ArduinoJson.h>
#include "DashGauge.h"
namespace DashGauge {
inline void toJson(JsonArray array,const Rules &rules){
  for(const auto &r:rules.entries){if(!r.id[0])continue;auto o=array.createNestedObject();
    o["id"]=r.id;o["units"]=r.units;auto p=o.createNestedArray("points");for(auto v:r.points)p.add(v);
    o["low"]=r.low;o["high"]=r.high;o["lowEnabled"]=r.lowEnabled;o["highEnabled"]=r.highEnabled;
  }
}
// Complete replacement, validated before touching persisted or active settings.
inline bool fromJson(JsonVariantConst input,Rules &out){
  if(!input.is<JsonArrayConst>() || input.size()>kMaxRules)return false;
  Rules next;unsigned i=0;
  for(auto o:input.as<JsonArrayConst>()){
    if(!o.is<JsonObjectConst>() || !o["id"].is<const char*>() || !o["units"].is<const char*>() ||
       strlen(o["id"].as<const char*>())>15 || strlen(o["units"].as<const char*>())>15 ||
       !o["points"].is<JsonArrayConst>() || o["points"].size()!=4 ||
       !o["lowEnabled"].is<bool>() || !o["highEnabled"].is<bool>() ||
       !o["low"].is<float>() || !o["high"].is<float>())return false;
    auto &r=next.entries[i++];strcpy(r.id,o["id"]);strcpy(r.units,o["units"]);
    for(unsigned j=0;j<4;++j){if(!o["points"][j].is<float>())return false;r.points[j]=o["points"][j];}
    r.low=o["low"];r.high=o["high"];r.lowEnabled=o["lowEnabled"];r.highEnabled=o["highEnabled"];
    if(!valid(r))return false;
  }
  if(!valid(next))return false;
  out=next;return true;
}
}
