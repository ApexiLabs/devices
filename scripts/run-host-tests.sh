#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
BUILD_DIR="$ROOT_DIR/.build-tests"

mkdir -p "$BUILD_DIR"

/usr/bin/c++ -std=c++17 -Wall -Wextra -Werror -I"$ROOT_DIR/include" \
  "$ROOT_DIR/tests/owner_reset_tests.cpp" -o "$BUILD_DIR/owner_reset_tests"
/usr/bin/c++ -std=c++17 -Wall -Wextra -Werror -I"$ROOT_DIR/tests/owner_reset_fakes" -I"$ROOT_DIR/include" \
  "$ROOT_DIR/tests/owner_reset_nvs_tests.cpp" -o "$BUILD_DIR/owner_reset_nvs_tests"
"$BUILD_DIR/owner_reset_nvs_tests"
"$BUILD_DIR/owner_reset_tests"

/usr/bin/c++ -std=c++17 -Wall -Wextra -Werror -I"$ROOT_DIR/include" \
  "$ROOT_DIR/src/SignedOta.cpp" "$ROOT_DIR/tests/signed_ota_tests.cpp" \
  -o "$BUILD_DIR/signed_ota_tests"
"$BUILD_DIR/signed_ota_tests"

# Exercise the secure branch against fake SDK APIs, never real eFuses/flash.
/usr/bin/c++ -std=c++17 -Wall -Wextra -Werror -DESP32 \
  -DAPEXI_PRODUCTION_SECURITY_REQUIRED=1 -DAPEXI_ENCRYPTED_QUEUE_QUALIFIED=1 \
  -DCONFIG_SECURE_BOOT_V2_ENABLED=1 -DCONFIG_SECURE_BOOT=1 \
  -DCONFIG_SECURE_FLASH_ENC_ENABLED=1 -DCONFIG_SECURE_FLASH_ENCRYPTION_MODE_RELEASE=1 \
  -DCONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=1 -DCONFIG_NVS_ENCRYPTION=1 \
  -I"$ROOT_DIR/tests/fakes/ota" -I"$ROOT_DIR/include" \
  "$ROOT_DIR/src/SignedOtaEsp32.cpp" "$ROOT_DIR/tests/signed_ota_esp32_tests.cpp" \
  -o "$BUILD_DIR/signed_ota_esp32_tests"
"$BUILD_DIR/signed_ota_esp32_tests"

/usr/bin/c++ -std=c++17 -Wall -Wextra -Werror -pthread \
  -I"$ROOT_DIR/include" "$ROOT_DIR/tests/https_exchange_tests.cpp" \
  -o "$BUILD_DIR/https_exchange_tests"
"$BUILD_DIR/https_exchange_tests"

/usr/bin/c++ -std=c++17 -Wall -Wextra -Werror \
  -I"$ROOT_DIR/include" \
  "$ROOT_DIR/src/Logic.cpp" \
  "$ROOT_DIR/src/ProvisioningPolicy.cpp" \
  "$ROOT_DIR/tests/logic_tests.cpp" \
  -o "$BUILD_DIR/logic_tests"

"$BUILD_DIR/logic_tests"

/usr/bin/c++ -std=c++17 -Wall -Wextra -Werror -I"$ROOT_DIR/include" \
  "$ROOT_DIR/tests/tinyc6_pins_tests.cpp" -o "$BUILD_DIR/tinyc6_pins_tests"
"$BUILD_DIR/tinyc6_pins_tests"

/usr/bin/c++ -std=c++17 -Wall -Wextra -Werror -I"$ROOT_DIR/include" \
  "$ROOT_DIR/tests/battery_tests.cpp" -o "$BUILD_DIR/battery_tests"
"$BUILD_DIR/battery_tests"

/usr/bin/c++ -std=c++17 -Wall -Wextra -Werror -I"$ROOT_DIR/include" \
  "$ROOT_DIR/tests/system_events_tests.cpp" -o "$BUILD_DIR/system_events_tests"
"$BUILD_DIR/system_events_tests"

node "$ROOT_DIR/tests/logger_diagnostics_tests.cjs"
node "$ROOT_DIR/tests/logger_branding_tests.cjs"
node "$ROOT_DIR/tests/logger_authorization_ui_tests.cjs"
/usr/bin/c++ -std=c++17 -Wall -Wextra -Werror -I"$ROOT_DIR/include" \
  "$ROOT_DIR/tests/upload_evidence_tests.cpp" -o "$BUILD_DIR/upload_evidence_tests"
"$BUILD_DIR/upload_evidence_tests"
/usr/bin/c++ -std=c++17 -Wall -Wextra -Werror -DESP32 -I"$ROOT_DIR/tests/storage_fakes" -I"$ROOT_DIR/tests/fakes" -I"$ROOT_DIR/include" \
  "$ROOT_DIR/tests/store_forward_batch_tests.cpp" "$ROOT_DIR/tests/storage_fakes/LittleFS.cpp" "$ROOT_DIR/src/StoreForwardQueue.cpp" "$ROOT_DIR/src/StatusDiagnostics.cpp" -o "$BUILD_DIR/store_forward_batch_tests"
"$BUILD_DIR/store_forward_batch_tests"
node "$ROOT_DIR/tests/dash_ui_tests.cjs"

/usr/bin/c++ -std=c++17 -Wall -Wextra -Werror -I"$ROOT_DIR/include" \
  "$ROOT_DIR/tests/dash_telemetry_tests.cpp" -o "$BUILD_DIR/dash_telemetry_tests"
"$BUILD_DIR/dash_telemetry_tests"

/usr/bin/c++ -std=c++17 -Wall -Wextra -Werror -I"$ROOT_DIR/include" \
  "$ROOT_DIR/tests/dash_gauge_tests.cpp" -o "$BUILD_DIR/dash_gauge_tests"
"$BUILD_DIR/dash_gauge_tests"
JSON_INCLUDE="$ROOT_DIR/.pio/libdeps/dash-waveshare-s3-128/ArduinoJson/src"
if [ ! -d "$JSON_INCLUDE" ]; then
  JSON_INCLUDE="$ROOT_DIR/.pio/libdeps/logger-nodemcuv2/ArduinoJson/src"
fi
if [ -d "$JSON_INCLUDE" ]; then
  /usr/bin/c++ -std=c++17 -Wall -Wextra -Werror -DESP32 -DARDUINOJSON_ENABLE_ARDUINO_STRING=1 \
    -I"$ROOT_DIR/tests/provisioning_fakes" -I"$ROOT_DIR/include" -I"$JSON_INCLUDE" \
    "$ROOT_DIR/tests/provisioning_transition_tests.cpp" "$ROOT_DIR/src/DeviceProvisioning.cpp" "$ROOT_DIR/src/ProvisioningPolicy.cpp" \
    -o "$BUILD_DIR/provisioning_transition_tests"
  "$BUILD_DIR/provisioning_transition_tests"
  /usr/bin/c++ -std=c++17 -Wall -Wextra -Werror -DAPEXI_AUTH_HOST_TEST -DARDUINOJSON_ENABLE_ARDUINO_STRING=1 \
    -I"$ROOT_DIR/tests/auth_fakes" -I"$ROOT_DIR/include" -I"$JSON_INCLUDE" \
    "$ROOT_DIR/tests/logger_authorization_tests.cpp" "$ROOT_DIR/src/LoggerAuthorization.cpp" -o "$BUILD_DIR/logger_authorization_tests"
  "$BUILD_DIR/logger_authorization_tests"
  /usr/bin/c++ -std=c++17 -Wall -Wextra -Werror -DTEST_UPLOAD_JSON -I"$ROOT_DIR/include" -I"$JSON_INCLUDE" \
    "$ROOT_DIR/tests/upload_evidence_tests.cpp" -o "$BUILD_DIR/upload_evidence_json_tests"
  "$BUILD_DIR/upload_evidence_json_tests"
  /usr/bin/c++ -std=c++17 -Wall -Wextra -Werror -DTEST_GAUGE_JSON -I"$ROOT_DIR/include" -I"$JSON_INCLUDE" \
    "$ROOT_DIR/tests/dash_gauge_tests.cpp" -o "$BUILD_DIR/dash_gauge_tests"
  "$BUILD_DIR/dash_gauge_tests"
else
  echo "JSON-dependent authorization, provisioning, upload and gauge tests skipped: build a firmware target to install ArduinoJson"
fi
/usr/bin/c++ -std=c++17 -Wall -Wextra -Werror -DESP32 \
  -I"$ROOT_DIR/tests/fakes" \
  -I"$ROOT_DIR/include" \
  "$ROOT_DIR/tests/fakes/LittleFS.cpp" \
  "$ROOT_DIR/src/StoreForwardQueue.cpp" \
  "$ROOT_DIR/src/StatusDiagnostics.cpp" \
  "$ROOT_DIR/tests/store_forward_queue_tests.cpp" \
  -o "$BUILD_DIR/store_forward_queue_tests"

"$BUILD_DIR/store_forward_queue_tests"

/usr/bin/c++ -std=c++17 -Wall -Wextra -Werror -DAPEXI_HOST_TEST \
  -I"$ROOT_DIR/tests/fakes" \
  -I"$ROOT_DIR/include" \
  "$ROOT_DIR/src/AppBearerRotation.cpp" \
  "$ROOT_DIR/tests/app_bearer_rotation_tests.cpp" \
  -o "$BUILD_DIR/app_bearer_rotation_tests"

"$BUILD_DIR/app_bearer_rotation_tests"

/usr/bin/c++ -std=c++17 -Wall -Wextra -Werror -DESP32 \
  -I"$ROOT_DIR/tests/fakes" -I"$ROOT_DIR/include" \
  "$ROOT_DIR/src/AppBearerRotation.cpp" \
  "$ROOT_DIR/tests/app_bearer_rotation_tests.cpp" \
  -o "$BUILD_DIR/app_bearer_rotation_nvs_tests"
"$BUILD_DIR/app_bearer_rotation_nvs_tests"

python3 -m unittest "$ROOT_DIR/tests/test_provision_device.py"
python3 -m unittest "$ROOT_DIR/tests/test_production_security.py"
python3 -m unittest "$ROOT_DIR/tests/test_signed_release.py"
python3 -m unittest "$ROOT_DIR/tests/test_security_surface.py"
python3 -m unittest "$ROOT_DIR/tests/test_qualification_records.py"

for target in ESP32 ESP8266; do
  /usr/bin/c++ -std=c++17 -Wall -Wextra -Werror -D"$target" \
    -I"$ROOT_DIR/tests/fakes" -I"$ROOT_DIR/include" \
    "$ROOT_DIR/src/StatusDiagnostics.cpp" "$ROOT_DIR/tests/status_diagnostics_tests.cpp" \
    -o "$BUILD_DIR/status_diagnostics_$target"
  "$BUILD_DIR/status_diagnostics_$target"
done
