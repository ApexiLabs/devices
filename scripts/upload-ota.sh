#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
SECRETS_FILE="$ROOT_DIR/include/AppSecrets.h"
OTA_TARGET=${1:-mda-logger.local}

if [ ! -f "$SECRETS_FILE" ]; then
  echo "OTA upload requires $SECRETS_FILE" >&2
  exit 1
fi

OTA_PASSWORD=$(sed -n 's/^[[:space:]]*#define[[:space:]][[:space:]]*APEXI_OTA_PASSWORD[[:space:]][[:space:]]*"\([^"]*\)".*/\1/p' "$SECRETS_FILE")
if [ -z "$OTA_PASSWORD" ]; then
  echo "APEXI_OTA_PASSWORD is missing or empty in $SECRETS_FILE" >&2
  exit 1
fi

export PLATFORMIO_CORE_DIR="$ROOT_DIR/.platformio"
export PLATFORMIO_SETTING_ENABLE_TELEMETRY=no

"$ROOT_DIR/.venv/bin/python" -m platformio run \
  --project-dir "$ROOT_DIR" \
  --environment logger-nodemcuv2

exec "$ROOT_DIR/.venv/bin/python" \
  "$ROOT_DIR/.platformio/packages/framework-arduinoespressif8266/tools/espota.py" \
  --ip "$OTA_TARGET" \
  --auth "$OTA_PASSWORD" \
  --file "$ROOT_DIR/.pio/build/logger-nodemcuv2/firmware.bin"
