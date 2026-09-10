#pragma once
#include "esp_ota_ops.h"
constexpr int ESP_FLASH_ENC_MODE_RELEASE = 2;
inline int esp_get_flash_encryption_mode() { return FakeOta::encrypted ? 2 : 0; }
