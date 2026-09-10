#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
class String {
 public:
  String()=default;String(const char *s):s_(s?s:""){}String(const std::string &s):s_(s){}
  bool isEmpty()const{return s_.empty();}size_t length()const{return s_.size();}const char *c_str()const{return s_.c_str();}
  void reserve(size_t n){s_.reserve(n);}bool concat(const char *s,size_t n){s_.append(s,n);return true;}
  bool concat(const char *s){s_+=s;return true;}bool concat(char c){s_+=c;return true;}
  String &operator+=(const char *s){s_+=s;return *this;}String &operator+=(char c){s_+=c;return *this;}
  bool operator==(const String &s)const{return s_==s.s_;}bool operator!=(const String &s)const{return !(*this==s);}
 private:std::string s_;
};
template<class T>T min(T a,T b){return a<b?a:b;}
inline uint32_t authTestMillis=1000;
inline uint32_t millis(){return authTestMillis;}
