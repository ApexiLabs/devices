#pragma once
#include <array>
#include <stdint.h>

// Optional v1 BLE status: snapshots only, never Wi-Fi/heartbeat/queue progress.
namespace DashUploadStatus {
constexpr char kUuid[]="8f771004-6d7a-4f48-9f8a-67a8c14b6c01";
constexpr uint32_t kPublishMs=1000,kStaleMs=5000,kNever=UINT32_MAX;
enum class State:uint8_t {Unknown,Disabled,Failed,ServerAccepted,TransportSent};
enum class Transport:uint8_t {Unknown,Https,Mqtt};
struct Status {State state=State::Unknown;Transport transport=Transport::Unknown;uint32_t successAgeMs=kNever,expectedIntervalMs=1000;};
using Frame=std::array<uint8_t,20>;
inline Frame encode(uint8_t state,uint8_t transport,uint32_t ageMs,uint32_t intervalMs=1000){
  Frame f{};f[0]=1;f[1]=state;f[2]=transport;
  for(unsigned i=0;i<4;++i){f[4+i]=(ageMs>>(8*i))&255;f[8+i]=(intervalMs>>(8*i))&255;}return f;
}
inline Frame encode(const Status &s){return encode(uint8_t(s.state),uint8_t(s.transport),s.successAgeMs,s.expectedIntervalMs);}
inline bool decode(const Frame &f,Status &out){
  if(f[0]!=1||f[1]>4||f[2]>2||f[3])return false;
  for(unsigned i=12;i<20;++i)if(f[i])return false;
  Status s;s.state=State(f[1]);s.transport=Transport(f[2]);s.successAgeMs=0;s.expectedIntervalMs=0;
  for(unsigned i=0;i<4;++i){s.successAgeMs|=uint32_t(f[4+i])<<(8*i);s.expectedIntervalMs|=uint32_t(f[8+i])<<(8*i);}
  if(!s.expectedIntervalMs)return false;
  if((s.state==State::ServerAccepted && s.transport!=Transport::Https)||
     (s.state==State::TransportSent && s.transport!=Transport::Mqtt))return false;
  if((s.state==State::ServerAccepted||s.state==State::TransportSent)&&s.successAgeMs==kNever)return false;
  out=s;return true;
}
inline uint32_t window(uint32_t interval){const uint64_t v=uint64_t(interval)*3;return v<5000?5000:v>=kNever?kNever-1:uint32_t(v);}
enum class View:uint8_t {Unknown,Disabled,Failed,Accepted,Unconfirmed,Stale};
struct Model {
  Status status;uint32_t receivedMs=0;bool received=false;
  bool accept(const Frame &f,uint32_t now){Status next;if(!decode(f,next))return false;status=next;receivedMs=now;received=true;return true;}
  uint32_t age(uint32_t now)const{if(!received||status.successAgeMs==kNever)return kNever;const uint64_t v=uint64_t(status.successAgeMs)+uint32_t(now-receivedMs);return v>=kNever?kNever-1:uint32_t(v);}
  View view(bool connected,uint32_t now)const{
    if(!connected||!received)return View::Unknown;
    if(uint32_t(now-receivedMs)>=kStaleMs)return View::Stale;
    if(status.state==State::Disabled)return View::Disabled;
    if(status.state==State::Failed)return View::Failed;
    if(status.state==State::Unknown)return View::Unknown;
    if(age(now)>=window(status.expectedIntervalMs))return View::Stale;
    return status.state==State::ServerAccepted?View::Accepted:View::Unconfirmed;
  }
};
inline const char *name(View v){switch(v){case View::Disabled:return "disabled";case View::Failed:return "failed";case View::Accepted:return "accepted";case View::Unconfirmed:return "unconfirmed";case View::Stale:return "stale";default:return "unknown";}}
inline uint16_t colour(View v){return v==View::Accepted?0x07e0:v==View::Failed?0xf800:v==View::Unconfirmed?0xfd20:0x8410;}
}
