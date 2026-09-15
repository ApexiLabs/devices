#pragma once
#include <cstddef>
#include <cstdint>
namespace BlankPartition {
template<class Read> bool verify(size_t size,Read read) {
  if(!size) return false;
  uint8_t bytes[256];
  for(size_t offset=0;offset<size;) {
    const size_t length=size-offset<sizeof(bytes)?size-offset:sizeof(bytes);
    if(!read(offset,bytes,length)) return false;
    for(size_t i=0;i<length;++i) if(bytes[i]!=0xff) return false;
    offset+=length;
  }
  return true;
}
}
