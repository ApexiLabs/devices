#pragma once
#include "SignedOta.h"
#if defined(ESP32)
#include <esp_ota_ops.h>

class SignedOtaEsp32 : public SignedOtaBackend {
 public:
  bool securePosture() const override;
  Boot bootState() const override;
  size_t inactiveCapacity() const override;
  bool beginInactive(size_t bytes) override;
  bool write(const uint8_t *data, size_t bytes) override;
  bool finishAndVerify() override;
  bool selectVerified() override;
  void abort() override;
  bool confirmHealthy() override;
  bool rollbackPossible() const override;
  bool rollback() override;
 private:
  const esp_partition_t *inactive() const;
  const esp_partition_t *target_ = nullptr;
  esp_ota_handle_t handle_ = 0;
  bool open_ = false;
  bool verified_ = false;
};
#endif
