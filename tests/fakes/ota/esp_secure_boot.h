#pragma once
#include "esp_ota_ops.h"
inline bool esp_secure_boot_enabled() { return FakeOta::secure; }
