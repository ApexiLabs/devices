#pragma once
#include <array>
#include <stdint.h>
#include <stdio.h>
namespace DashDiagnostics {
struct Event { uint32_t ms=0, ageMs=0; uint8_t sensor=255, state=0; };
struct Log {
  std::array<Event,64> events{};
  uint32_t total=0;
  void add(uint32_t ms,uint8_t sensor,uint8_t state,uint32_t age=0){
    events[total%events.size()]={ms,age,sensor,state}; ++total;
  }
  uint32_t size() const{return total<events.size()?total:events.size();}
  Event at(uint32_t i) const{return events[(total-size()+i)%events.size()];}
};
// 0 no sample, 1 incomplete metadata, 2 disconnected, 3 stale, 4 fault, 5 live.
template<class Sensor> uint8_t state(const Sensor &s,bool connected,uint32_t now){
  if(!connected)return 2;
  if(!s.received)return 0;
  if(s.metadata!=7)return 1;
  if(!s.fresh(true,now))return 3;
  return s.valid && static_cast<uint8_t>(s.fault)==0?5:4;
}
inline const char *name(uint8_t state){
  constexpr const char *names[]={"no_sample","metadata_incomplete","disconnected","stale","sensor_fault","live"};
  return state<6?names[state]:"link";
}
// One physical line per event; only fixed names and numeric fields enter output.
inline void formatEvent(char *out,size_t capacity,const Event &e){
  snprintf(out,capacity,"[%lu.%03lu] %s apexi-dash sensor_state sensor=%u state=%s sample_age_ms=%lu\n",
    static_cast<unsigned long>(e.ms/1000),static_cast<unsigned long>(e.ms%1000),
    e.state==5?"INFO":"WARN",e.sensor,name(e.state),static_cast<unsigned long>(e.ageMs));
}
}
