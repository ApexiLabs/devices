#pragma once
#include <array>
#include <cmath>
#include <cstring>
#include <stdint.h>

// Gauge policy is independent of rendering, transport and sensor acquisition.
namespace DashGauge {
constexpr unsigned kMaxRules=8;
struct Rule {
  char id[16]{}, units[16]{};
  float points[4]{0,33,66,100}; // blue, green, yellow, red (engineering units)
  float low=0, high=100;
  bool lowEnabled=false, highEnabled=false;
};
struct Rules { std::array<Rule,kMaxRules> entries{}; };
inline bool identifier(const char *id){
  const auto n=strnlen(id,16); if(!n || n==16)return false;
  for(unsigned i=0;i<n;++i)if(!((id[i]>='a'&&id[i]<='z')||(id[i]>='A'&&id[i]<='Z')||(id[i]>='0'&&id[i]<='9')||id[i]=='_'||id[i]=='-'))return false;
  return strcmp(id,"-")!=0;
}
inline bool number(float v){return std::isfinite(v) && v>=-1000000 && v<=1000000;}
inline bool valid(const Rule &r){
  if(!identifier(r.id) || strnlen(r.units,16)==16)return false;
  for(const char *p=r.units;*p;++p)if(*p<32 || *p>126)return false;
  for(unsigned i=0;i<4;++i)if(!number(r.points[i]) || (i && r.points[i]<=r.points[i-1]))return false;
  return number(r.low)&&number(r.high)&&(!(r.lowEnabled&&r.highEnabled)||r.low<r.high);
}
inline bool valid(const Rules &rules){
  for(unsigned i=0;i<kMaxRules;++i){const auto &r=rules.entries[i];if(!r.id[0])continue;
    if(!valid(r))return false;
    for(unsigned j=0;j<i;++j)if(!strcmp(r.id,rules.entries[j].id))return false;
  }return true;
}
inline const Rule *find(const Rules &rules,const char *id,const char *units){
  for(const auto &r:rules.entries)if(r.id[0]&&!strcmp(r.id,id)&&!strcmp(r.units,units))return &r;
  return nullptr;
}
enum class Alarm:uint8_t { None, Low, High };
inline Alarm alarm(const Rule *r,float value,bool freshValid){
  if(!r || !valid(*r) || !freshValid || !std::isfinite(value))return Alarm::None;
  if(r->lowEnabled && value<=r->low)return Alarm::Low;
  if(r->highEnabled && value>=r->high)return Alarm::High;
  return Alarm::None;
}
inline const char *alarmName(Alarm a){return a==Alarm::Low?"LOW":a==Alarm::High?"HIGH":"";}
constexpr uint16_t rgb(unsigned r,unsigned g,unsigned b){return ((r>>3)<<11)|((g>>2)<<5)|(b>>3);}
inline uint16_t highlight(uint16_t c){
  unsigned r=((c>>11)&31)*255/31,g=((c>>5)&63)*255/63,b=(c&31)*255/31;
  return rgb(r+(255-r)/4,g+(255-g)/4,b+(255-b)/4);
}
inline uint16_t colour(const Rule *r,float value,bool freshValid){
  if(!freshValid || !std::isfinite(value))return rgb(72,84,96); // Unknown is not 'cold'.
  if(!r || !valid(*r))return rgb(0,200,255);
  constexpr unsigned colours[4][3]={{0,160,255},{32,220,96},{255,215,0},{255,32,32}};
  unsigned segment=0;while(segment<2 && value>r->points[segment+1])++segment;
  float t=(value-r->points[segment])/(r->points[segment+1]-r->points[segment]);
  t=t<0?0:t>1?1:t;
  unsigned channels[3];for(unsigned c=0;c<3;++c)channels[c]=unsigned(colours[segment][c]+(float(colours[segment+1][c])-colours[segment][c])*t+0.5f);
  return rgb(channels[0],channels[1],channels[2]);
}
// Alarm colour takes precedence over the configured engineering-value gradient.
inline uint16_t arcColour(const Rule *r,float value,bool freshValid){
  return alarm(r,value,freshValid)!=Alarm::None?rgb(255,0,0):colour(r,value,freshValid);
}
}
