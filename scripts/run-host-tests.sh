#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
BUILD_DIR="$ROOT_DIR/.build-tests"

mkdir -p "$BUILD_DIR"

/usr/bin/c++ -std=c++17 -Wall -Wextra -Werror \
  -I"$ROOT_DIR/include" \
  "$ROOT_DIR/src/Logic.cpp" \
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
/usr/bin/c++ -std=c++17 -Wall -Wextra -Werror -pthread -I"$ROOT_DIR/include" \
  "$ROOT_DIR/tests/https_exchange_tests.cpp" -o "$BUILD_DIR/https_exchange_tests"
"$BUILD_DIR/https_exchange_tests"
/usr/bin/c++ -std=c++17 -Wall -Wextra -Werror -DESP32 -I"$ROOT_DIR/tests/storage_fakes" -I"$ROOT_DIR/include" \
  "$ROOT_DIR/tests/store_forward_batch_tests.cpp" "$ROOT_DIR/tests/storage_fakes/LittleFS.cpp" "$ROOT_DIR/src/StoreForwardQueue.cpp" -o "$BUILD_DIR/store_forward_batch_tests"
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
  echo "Dash gauge JSON integration tests skipped: build Dash once to install ArduinoJson"
fi
