#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
using esp_ota_handle_t = uint32_t;
enum esp_ota_img_states_t { ESP_OTA_IMG_VALID, ESP_OTA_IMG_PENDING_VERIFY, ESP_OTA_IMG_UNDEFINED };
constexpr int ESP_OK = 0;
constexpr uint32_t ESP_APP_DESC_MAGIC_WORD = 0xABCD5432;
constexpr int ESP_PARTITION_TYPE_APP = 0, ESP_PARTITION_TYPE_DATA = 1;
constexpr int ESP_PARTITION_SUBTYPE_APP_OTA_0 = 16, ESP_PARTITION_SUBTYPE_APP_OTA_1 = 17;
constexpr int ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS = 4, ESP_PARTITION_SUBTYPE_DATA_SPIFFS = 0x82;
struct esp_partition_t { uint32_t address, size; int type, subtype; bool encrypted; };
struct esp_app_desc_t { uint32_t magic_word, secure_version; char project_name[32]; };
namespace FakeOta {
inline esp_partition_t running{0x10000, 0x200000, 0, 16, true};
inline esp_partition_t next{0x210000, 0x200000, 0, 17, true};
inline esp_partition_t keys{0xD000, 4096, 1, 4, true};
inline esp_partition_t queue{0x410000, 0xBF0000, 1, 0x82, true};
inline esp_ota_img_states_t state = ESP_OTA_IMG_VALID;
inline bool secure = true, encrypted = true, signature = true, metadata = true, sameProject = true;
inline uint32_t candidateVersion = 0;
inline int begins = 0, selections = 0, aborts = 0, confirms = 0;
}
inline const esp_partition_t *esp_ota_get_running_partition() { return &FakeOta::running; }
inline const esp_partition_t *esp_ota_get_next_update_partition(const void *) { return &FakeOta::next; }
inline const esp_partition_t *esp_partition_find_first(int, int subtype, const char *) {
  return subtype == 4 ? &FakeOta::keys : &FakeOta::queue;
}
inline int esp_ota_get_state_partition(const esp_partition_t *, esp_ota_img_states_t *state) {
  *state = FakeOta::state; return FakeOta::metadata ? 0 : -1;
}
inline int esp_ota_begin(const esp_partition_t *p, size_t, esp_ota_handle_t *handle) {
  if (p->address == FakeOta::running.address) return -1;
  ++FakeOta::begins; *handle = 1; return 0;
}
inline int esp_ota_write(esp_ota_handle_t, const void *, size_t) { return 0; }
inline int esp_ota_end(esp_ota_handle_t) { return FakeOta::signature ? 0 : -1; }
inline int esp_ota_abort(esp_ota_handle_t) { ++FakeOta::aborts; return 0; }
inline int esp_ota_get_partition_description(const esp_partition_t *p, esp_app_desc_t *desc) {
  desc->magic_word = ESP_APP_DESC_MAGIC_WORD;
  desc->secure_version = p == &FakeOta::next ? FakeOta::candidateVersion : 0;
  std::memset(desc->project_name, 0, 32);
  std::strcpy(desc->project_name, p == &FakeOta::next && !FakeOta::sameProject ? "other" : "logger");
  return 0;
}
inline int esp_ota_set_boot_partition(const esp_partition_t *) { ++FakeOta::selections; return 0; }
inline int esp_ota_mark_app_valid_cancel_rollback() { ++FakeOta::confirms; return 0; }
inline bool esp_ota_check_rollback_is_possible() { return true; }
inline int esp_ota_mark_app_invalid_rollback_and_reboot() { return 0; }
