#pragma once
#include <array>
#include <algorithm>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Fixed-sized, versioned records. Callers supply event codes, never raw requests
// or server response text (which can contain credentials).
namespace SystemEvents {
constexpr const char *kUuid = "8f771003-6d7a-4f48-9f8a-67a8c14b6c01";
struct Event {
  uint32_t boot=0, seq=0, ms=0, value=0;
  uint8_t severity=1;
  char code[32]{};
  uint32_t durationMs=0;
  uint16_t httpStatus=0;
  uint8_t method=0, version=1;
};
static_assert(sizeof(Event)==60,"BLE event v1 layout changed");
// Four <=20-byte notifications work at the default BLE MTU. Missing or
// reordered parts are discarded; the sender retries the unacknowledged event.
using Frame=std::array<uint8_t,20>;
inline void put32(uint8_t *p,uint32_t value) { for(unsigned i=0;i<4;++i) p[i]=uint8_t(value>>(i*8)); }
inline uint32_t get32(const uint8_t *p) { uint32_t value=0; for(unsigned i=0;i<4;++i) value|=uint32_t(p[i])<<(i*8); return value; }
inline Frame frame(const Event &event,uint8_t part) {
  Frame out{}; out[0]=0xe1; out[1]=part; out[2]=uint8_t(event.seq);
  if(part<4) {
    std::array<uint8_t,60> bytes{};
    bytes[0]=event.version; bytes[1]=event.severity; bytes[2]=event.method;
    put32(bytes.data()+4,event.boot); put32(bytes.data()+8,event.seq); put32(bytes.data()+12,event.ms);
    put32(bytes.data()+16,event.value); put32(bytes.data()+20,event.durationMs);
    bytes[24]=uint8_t(event.httpStatus); bytes[25]=uint8_t(event.httpStatus>>8);
    memcpy(bytes.data()+26,event.code,32);
    const size_t offset=part*17;
    memcpy(out.data()+3,bytes.data()+offset,std::min(size_t(17),bytes.size()-offset));
  }
  return out;
}
struct Receiver {
  std::array<uint8_t,sizeof(Event)> bytes{};
  uint8_t next=0,sequence=0;
  bool accept(const uint8_t *data,size_t size,Event &event) {
    if(size!=20 || data[0]!=0xe1 || data[1]>3) { next=0; return false; }
    if(data[1]==0) { next=0; sequence=data[2]; }
    if(data[1]!=next || data[2]!=sequence) { next=0; return false; }
    const size_t offset=next*17;
    memcpy(bytes.data()+offset,data+3,std::min(size_t(17),sizeof(Event)-offset));
    if(++next!=4) return false;
    next=0; event={}; event.version=bytes[0]; event.severity=bytes[1]; event.method=bytes[2];
    event.boot=get32(bytes.data()+4); event.seq=get32(bytes.data()+8); event.ms=get32(bytes.data()+12);
    event.value=get32(bytes.data()+16); event.durationMs=get32(bytes.data()+20);
    event.httpStatus=uint16_t(bytes[24])|(uint16_t(bytes[25])<<8); memcpy(event.code,bytes.data()+26,32);
    return event.version==1 && uint8_t(event.seq)==sequence;
  }
};
template<size_t N> class Queue {
 public:
  bool push(const Event &e) {
    if (count_==N) { ++dropped; return false; }
    entries_[(head_+count_)%N]=e; ++count_; return true;
  }
  const Event *front() const { return count_ ? &entries_[head_] : nullptr; }
  void pop() { if(count_) { head_=(head_+1)%N; --count_; } }
  size_t size() const { return count_; }
  uint32_t dropped=0;
 private:
  std::array<Event,N> entries_{};
  size_t head_=0,count_=0;
};
inline bool safeName(const char *s) {
  const size_t n=strlen(s);
  if(n<5 || n>95 || strstr(s,"..")) return false;
  for(size_t i=0;i<n;++i) if(!((s[i]>='a'&&s[i]<='z') || (s[i]>='0'&&s[i]<='9') || s[i]=='-' || s[i]=='.')) return false;
  return (strncmp(s,"logs-",5)==0 && strcmp(s+n-4,".csv")==0) ||
      ((strncmp(s,"logger-",7)==0 || strncmp(s,"dash-",5)==0) && strcmp(s+n-4,".log")==0);
}
inline bool safeCode(const char *s) {
  if(!*s || strlen(s)>31) return false;
  for(;*s;++s) if(!((*s>='a'&&*s<='z') || (*s>='0'&&*s<='9') || *s=='_')) return false;
  return true;
}
}
