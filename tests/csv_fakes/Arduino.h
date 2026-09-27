#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <sstream>
#include <iomanip>
#include <type_traits>
class String {
 public:
  String()=default;
  String(const char *s):text_(s?s:""){}
  String(std::string s):text_(std::move(s)){}
  template<class T, std::enable_if_t<std::is_integral_v<T>,int> = 0>
  String(T value):text_(std::to_string(value)){}
  String(float value,int decimals){std::ostringstream out;out<<std::fixed<<std::setprecision(decimals)<<value;text_=out.str();}
  bool isEmpty()const{return text_.empty();}
  size_t length()const{return text_.size();}
  const char *c_str()const{return text_.c_str();}
  bool endsWith(const char *suffix)const{const std::string s(suffix);return text_.size()>=s.size()&&text_.compare(text_.size()-s.size(),s.size(),s)==0;}
  bool operator==(const String &s)const{return text_==s.text_;}
  bool operator!=(const String &s)const{return !(*this==s);}
  String &operator+=(const String &s){text_+=s.text_;return *this;}
  friend String operator+(const String &a,const String &b){return a.text_+b.text_;}
 private:std::string text_;
};
