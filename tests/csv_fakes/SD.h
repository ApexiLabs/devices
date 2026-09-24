#pragma once
#include "FS.h"
#include "SPI.h"
#include <map>
struct FakeSD {
  bool mounted=true;
  std::map<std::string,std::shared_ptr<std::string>> files;
  bool begin(uint8_t,SPIClass &,int){return mounted;}
  bool exists(const String &path){return files.count(path.c_str());}
  File open(const String &path,const char *) {
    if(!mounted)return File();
    auto &bytes=files[path.c_str()];if(!bytes)bytes=std::make_shared<std::string>();return File(bytes);
  }
  File open(const String &path,int=FILE_READ){return files.count(path.c_str())?File(files.at(path.c_str())):File();}
};
inline FakeSD SD;
