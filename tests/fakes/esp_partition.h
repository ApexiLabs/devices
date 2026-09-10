#pragma once
#include <cstddef>
#include <cstring>
constexpr int ESP_PARTITION_TYPE_DATA=1,ESP_PARTITION_SUBTYPE_DATA_SPIFFS=2,ESP_OK=0;
struct esp_partition_t {size_t size=1024*1024;const char *label="littlefs";};
inline const esp_partition_t *esp_partition_find_first(int,int,const char *label){
  static esp_partition_t partition;
  return std::strcmp(label,"littlefs")==0?&partition:nullptr;
}
// Existing recovery fixtures model a populated partition, never autoformattable.
inline int esp_partition_read(const esp_partition_t *,size_t,void *target,size_t size){
  std::memset(target,0,size);return ESP_OK;
}
