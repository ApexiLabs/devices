#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)

fail() {
  echo "repo-contracts: $1" >&2
  exit 1
}

PLATFORMIO_ENV=$(awk -F'=' '
  $1 ~ /^default_envs[[:space:]]*$/ {
    gsub(/[[:space:]]/, "", $2)
    print $2
    exit
  }' "$ROOT_DIR/platformio.ini")

[ -n "$PLATFORMIO_ENV" ] || fail "could not determine default_envs from platformio.ini"

for workflow in "$ROOT_DIR/.github/workflows/build-firmware.yml" "$ROOT_DIR/.github/workflows/release.yml"; do
  grep -q ".pio/build/$PLATFORMIO_ENV/" "$workflow" || fail "$(basename "$workflow") does not use .pio/build/$PLATFORMIO_ENV/"
done

grep -q "firmware-$PLATFORMIO_ENV" "$ROOT_DIR/.github/workflows/build-firmware.yml" || fail "build-firmware.yml artifact name does not include $PLATFORMIO_ENV"

for macro in MDA_PIN_SPI_MISO MDA_PIN_SPI_MOSI MDA_PIN_SPI_SCLK PIN_TFT_CS PIN_TFT_DC PIN_TFT_RST PIN_STATUS_LED; do
  grep -q "#define $macro" "$ROOT_DIR/include/PinDefinitions.h" || fail "missing $macro in include/PinDefinitions.h"
done

LOGGER_TFT_SETUP="$ROOT_DIR/include/LoggerDisplayTFTSetup.h"
grep -q "#define TFT_MISO MDA_PIN_SPI_MISO" "$LOGGER_TFT_SETUP" || fail "LoggerDisplayTFTSetup.h is not wired to MDA_PIN_SPI_MISO"
grep -q "#define TFT_MOSI MDA_PIN_SPI_MOSI" "$LOGGER_TFT_SETUP" || fail "LoggerDisplayTFTSetup.h is not wired to MDA_PIN_SPI_MOSI"
grep -q "#define TFT_SCLK MDA_PIN_SPI_SCLK" "$LOGGER_TFT_SETUP" || fail "LoggerDisplayTFTSetup.h is not wired to MDA_PIN_SPI_SCLK"
grep -q "#define TFT_CS   PIN_TFT_CS" "$LOGGER_TFT_SETUP" || fail "LoggerDisplayTFTSetup.h is not wired to PIN_TFT_CS"
grep -q "#define TFT_DC   PIN_TFT_DC" "$LOGGER_TFT_SETUP" || fail "LoggerDisplayTFTSetup.h is not wired to PIN_TFT_DC"
grep -q "#define TFT_RST  PIN_TFT_RST" "$LOGGER_TFT_SETUP" || fail "LoggerDisplayTFTSetup.h is not wired to PIN_TFT_RST"

grep -q "docs/hardware-setup.md" "$ROOT_DIR/README.md" || fail "README.md does not point to docs/hardware-setup.md"
grep -q "docs/repo-contracts.md" "$ROOT_DIR/AGENTS.md" || fail "AGENTS.md does not point to docs/repo-contracts.md"

git -C "$ROOT_DIR" check-ignore --quiet include/AppSecrets.h || \
  fail "include/AppSecrets.h must remain ignored"
grep -q "APEXI_OTA_PASSWORD" "$ROOT_DIR/include/AppSecrets.example.h" || \
  fail "include/AppSecrets.example.h must document APEXI_OTA_PASSWORD"
grep -q "upload_protocol = espota" "$ROOT_DIR/platformio.ini" || \
  fail "platformio.ini must provide the OTA upload environment"
if git -C "$ROOT_DIR" ls-files --error-unmatch -- include/AppSecrets.h >/dev/null 2>&1; then
  fail "include/AppSecrets.h contains local credentials and must not be tracked"
fi

echo "repo-contracts: ok"
