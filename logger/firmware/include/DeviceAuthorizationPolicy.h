#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <cstdio>
namespace DeviceAuthorizationPolicy {
inline bool printable(std::string_view s,size_t minimum,size_t maximum) {
  if(s.size()<minimum || s.size()>maximum) return false;
  for(unsigned char c:s) if(c<0x21 || c>0x7e) return false;
  return true;
}
inline bool deviceId(std::string_view s) {
  if(s.empty() || s.size()>63) return false;
  for(char c:s) if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_')) return false;
  return true;
}
inline bool hex(std::string_view s,size_t length) {
  if(s.size()!=length)return false;
  for(char c:s)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
  return true;
}
inline bool uuid(std::string_view s) {
  if(s.size()!=36)return false;
  for(size_t i=0;i<s.size();++i) {
    if(i==8||i==13||i==18||i==23){if(s[i]!='-')return false;}
    else if(!hex(s.substr(i,1),1))return false;
  }
  return true;
}
inline std::string hardwareId(uint64_t mac) {
  char out[19];std::snprintf(out,sizeof(out),"esp32-%012llx",static_cast<unsigned long long>(mac&0xffffffffffffULL));return out;
}
inline bool elapsed(uint32_t now,uint32_t start,uint32_t duration) {return uint32_t(now-start)>=duration;}
inline uint32_t pollDelay(uint32_t seconds) {return seconds<5?5000:seconds>60?60000:seconds*1000;}
}
