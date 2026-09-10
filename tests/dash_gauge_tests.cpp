#include "DashGauge.h"
#ifdef TEST_GAUGE_JSON
#include "DashGaugeJson.h"
#endif
#include <cassert>
#include <limits>
#include <iostream>
using namespace DashGauge;
int main(){
  Rule r;strcpy(r.id,"oil_temperature");strcpy(r.units,"C");
  r.points[0]=0;r.points[1]=70;r.points[2]=110;r.points[3]=140;r.low=10;r.high=125;
  assert(valid(r));assert(alarm(&r,150,true)==Alarm::None);
  r.highEnabled=true;r.lowEnabled=true;
  assert(alarm(&r,125,true)==Alarm::High);assert(alarm(&r,10,true)==Alarm::Low);
  assert(alarm(&r,100,true)==Alarm::None);assert(alarm(&r,150,false)==Alarm::None);
  assert(alarm(&r,std::numeric_limits<float>::quiet_NaN(),true)==Alarm::None);
  assert(colour(&r,0,true)==rgb(0,160,255));assert(colour(&r,70,true)==rgb(32,220,96));
  assert(colour(&r,110,true)==rgb(255,215,0));assert(colour(&r,140,true)==rgb(255,32,32));
  assert(colour(&r,-10,true)==colour(&r,0,true));assert(colour(&r,200,true)==colour(&r,140,true));
  assert(colour(&r,90,true)!=colour(&r,70,true)&&colour(&r,90,true)!=colour(&r,110,true));
  assert(colour(&r,150,false)==rgb(72,84,96));
  assert(arcColour(&r,10,true)==rgb(255,0,0));
  assert(arcColour(&r,125,true)==rgb(255,0,0));
  assert(arcColour(&r,100,true)==colour(&r,100,true));
  assert(arcColour(&r,150,false)==colour(&r,150,false));
  assert(arcColour(nullptr,150,true)==colour(nullptr,150,true));
  auto disabled=r;disabled.lowEnabled=false;disabled.highEnabled=false;
  assert(arcColour(&disabled,10,true)==colour(&disabled,10,true));
  assert(arcColour(&disabled,150,true)==colour(&disabled,150,true));
  Rules rules;rules.entries[0]=r;assert(valid(rules));
  assert(find(rules,"oil_temperature","bar")==nullptr);assert(find(rules,"other","C")==nullptr);
#ifdef TEST_GAUGE_JSON
  DynamicJsonDocument doc(8192);toJson(doc.to<JsonArray>(),rules);Rules decoded;
  assert(fromJson(doc.as<JsonVariantConst>(),decoded));assert(decoded.entries[0].high==125);
  auto original=decoded;
  doc[0]["id"]="";assert(!fromJson(doc.as<JsonVariantConst>(),decoded));doc[0]["id"]="oil_temperature";
  doc[0]["points"][1]=0;assert(!fromJson(doc.as<JsonVariantConst>(),decoded));
  assert(decoded.entries[0].points[1]==original.entries[0].points[1]);
  doc.clear();toJson(doc.to<JsonArray>(),rules);doc[0]["lowEnabled"]="true";assert(!fromJson(doc.as<JsonVariantConst>(),decoded));
  doc.clear();toJson(doc.to<JsonArray>(),rules);doc[0]["points"][1]=nullptr;assert(!fromJson(doc.as<JsonVariantConst>(),decoded));
  doc.clear();toJson(doc.to<JsonArray>(),rules);doc[0]["low"]=130;assert(!fromJson(doc.as<JsonVariantConst>(),decoded));
  rules.entries[1]=r;assert(!valid(rules));
  doc.clear();toJson(doc.to<JsonArray>(),rules);assert(!fromJson(doc.as<JsonVariantConst>(),decoded));
  r.points[2]=std::numeric_limits<float>::infinity();assert(!valid(r));
  doc.clear();doc.to<JsonArray>();assert(fromJson(doc.as<JsonVariantConst>(),decoded));assert(!decoded.entries[0].id[0]);
#endif
  std::cout<<"Dash gauge tests passed\n";
}
