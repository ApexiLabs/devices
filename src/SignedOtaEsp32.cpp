#include "SignedOtaEsp32.h"
#if defined(ESP32)
#ifndef APEXI_PRODUCTION_SECURITY_REQUIRED
#define APEXI_PRODUCTION_SECURITY_REQUIRED 0
#endif
#ifndef APEXI_ENCRYPTED_QUEUE_QUALIFIED
// Current precompiled LittleFS has no qualified encrypted-partition contract.
// Do not enable on a production profile without reviewed implementation/tests.
#define APEXI_ENCRYPTED_QUEUE_QUALIFIED 0
#endif
#include <cstring>
#include <esp_flash_encrypt.h>
#include <esp_secure_boot.h>

// The Arduino core otherwise marks pending OTA images valid before setup().
// Strong C-linkage override of its weak hook; development behavior unchanged.
extern "C" bool verifyRollbackLater() { return APEXI_PRODUCTION_SECURITY_REQUIRED != 0; }

bool SignedOtaEsp32::securePosture() const {
#if APEXI_PRODUCTION_SECURITY_REQUIRED && CONFIG_SECURE_BOOT_V2_ENABLED && \
    CONFIG_SECURE_BOOT && CONFIG_SECURE_FLASH_ENC_ENABLED && \
    CONFIG_SECURE_FLASH_ENCRYPTION_MODE_RELEASE && CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE && \
    CONFIG_NVS_ENCRYPTION && APEXI_ENCRYPTED_QUEUE_QUALIFIED && !CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK
  // No automatic security-version eFuse advancement in this implementation.
  const auto *keys = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS, nullptr);
  const auto *queue = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "littlefs");
  return keys != nullptr && keys->encrypted && keys->size == 4096 &&
         queue != nullptr && queue->encrypted &&
         esp_secure_boot_enabled() && esp_get_flash_encryption_mode() == ESP_FLASH_ENC_MODE_RELEASE;
#else
  return false;
#endif
}

SignedOtaBackend::Boot SignedOtaEsp32::bootState() const {
  const auto *running = esp_ota_get_running_partition();
  esp_ota_img_states_t state;
  if (running == nullptr || esp_ota_get_state_partition(running, &state) != ESP_OK) return Boot::Unknown;
  if (state == ESP_OTA_IMG_VALID) return Boot::Valid;
  if (state == ESP_OTA_IMG_PENDING_VERIFY) return Boot::Pending;
  return Boot::Unknown;
}

const esp_partition_t *SignedOtaEsp32::inactive() const {
  const auto *running = esp_ota_get_running_partition();
  const auto *next = esp_ota_get_next_update_partition(nullptr);
  if (running == nullptr || next == nullptr || running->address == next->address ||
      next->type != ESP_PARTITION_TYPE_APP ||
      (next->subtype != ESP_PARTITION_SUBTYPE_APP_OTA_0 && next->subtype != ESP_PARTITION_SUBTYPE_APP_OTA_1)) return nullptr;
  return next;
}

size_t SignedOtaEsp32::inactiveCapacity() const {
  const auto *next = inactive();
  return next == nullptr ? 0 : next->size;
}

bool SignedOtaEsp32::beginInactive(const size_t bytes) {
  if (open_ || !securePosture() || bootState() != Boot::Valid) return false;
  target_ = inactive();
  verified_ = false;
  if (target_ == nullptr || bytes == 0 || bytes > target_->size) return false;
  open_ = esp_ota_begin(target_, bytes, &handle_) == ESP_OK;
  return open_;
}

bool SignedOtaEsp32::write(const uint8_t *data, const size_t bytes) {
  return open_ && esp_ota_write(handle_, data, bytes) == ESP_OK;
}

bool SignedOtaEsp32::finishAndVerify() {
  if (!open_ || !securePosture()) return false;
  open_ = false;  // esp_ota_end consumes the handle on every outcome.
  if (esp_ota_end(handle_) != ESP_OK) return false;
  esp_app_desc_t candidate{}, running{};
  if (esp_ota_get_partition_description(target_, &candidate) != ESP_OK ||
      esp_ota_get_partition_description(esp_ota_get_running_partition(), &running) != ESP_OK) return false;
  // Signature verification is in esp_ota_end under the required secure SDK.
  // Preserve rollback availability: no security-version change is allowed.
  verified_ = candidate.magic_word == ESP_APP_DESC_MAGIC_WORD &&
              candidate.secure_version == running.secure_version &&
              std::memcmp(candidate.project_name, running.project_name, sizeof(candidate.project_name)) == 0;
  return verified_;
}

bool SignedOtaEsp32::selectVerified() {
  if (!verified_ || !securePosture() || bootState() != Boot::Valid || target_ != inactive()) return false;
  verified_ = false;
  return esp_ota_set_boot_partition(target_) == ESP_OK;
}

void SignedOtaEsp32::abort() {
  if (open_) esp_ota_abort(handle_);
  open_ = false;
  verified_ = false;
}

bool SignedOtaEsp32::confirmHealthy() {
  return securePosture() && bootState() == Boot::Pending && esp_ota_mark_app_valid_cancel_rollback() == ESP_OK;
}
bool SignedOtaEsp32::rollbackPossible() const { return esp_ota_check_rollback_is_possible(); }
bool SignedOtaEsp32::rollback() { return esp_ota_mark_app_invalid_rollback_and_reboot() == ESP_OK; }
#endif
