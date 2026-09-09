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
node "$ROOT_DIR/tests/dash_ui_tests.cjs"

/usr/bin/c++ -std=c++17 -Wall -Wextra -Werror -I"$ROOT_DIR/include" \
  "$ROOT_DIR/tests/dash_telemetry_tests.cpp" -o "$BUILD_DIR/dash_telemetry_tests"
"$BUILD_DIR/dash_telemetry_tests"
