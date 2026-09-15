#pragma once
#include <nvs.h>

namespace OwnerReset {

// Keep this namespace separate from every credential-clear callback. Absence
// is permitted only when NVS explicitly returns NOT_FOUND, never on I/O/type
// errors. Coordinator performs the readback after each durable mark operation.
class NvsStore {
 public:
  bool getPending(bool &pending) {
    nvs_handle_t handle;
    const esp_err_t opened = nvs_open("owner_reset", NVS_READONLY, &handle);
    if (opened == ESP_ERR_NVS_NOT_FOUND) { pending = false; return true; }
    if (opened != ESP_OK) return false;
    uint8_t value = 255;
    const esp_err_t read = nvs_get_u8(handle, "pending", &value);
    nvs_close(handle);
    if (read == ESP_ERR_NVS_NOT_FOUND) { pending = false; return true; }
    if (read != ESP_OK || value > 1) return false;
    pending = value == 1;
    return true;
  }

  bool markPending(bool pending) {
    nvs_handle_t handle;
    if (nvs_open("owner_reset", NVS_READWRITE, &handle) != ESP_OK) return false;
    const esp_err_t written = nvs_set_u8(handle, "pending", pending ? 1 : 0);
    const bool committed = written == ESP_OK && nvs_commit(handle) == ESP_OK;
    nvs_close(handle);
    return committed;
  }
};

}  // namespace OwnerReset
