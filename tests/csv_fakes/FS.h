#pragma once
#include "Arduino.h"
#include <memory>
#include <algorithm>
#include <limits>
inline size_t nextPrintLimit=std::numeric_limits<size_t>::max();
inline unsigned csvFlushes=0;
class File {
 public:
  File()=default;
  explicit File(std::shared_ptr<std::string> bytes):bytes_(bytes){}
  explicit operator bool()const{return bool(bytes_);}
  size_t print(const String &value){
    if(!bytes_)return 0;
    const size_t n=std::min(value.length(),nextPrintLimit);
    nextPrintLimit=std::numeric_limits<size_t>::max();
    bytes_->append(value.c_str(),n);return n;
  }
  size_t println(const String &value){return print(value+"\r\n");}
  size_t size()const{return bytes_?bytes_->size():0;}
  void flush(){if(bytes_)++csvFlushes;}
  void close(){bytes_.reset();}
  File openNextFile(){return File();}
  bool isDirectory()const{return false;}
  const char *name()const{return "";}
 private:std::shared_ptr<std::string> bytes_;
};
constexpr int FILE_READ=0;
