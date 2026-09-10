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
grep -q "dist/SHA256SUMS" "$ROOT_DIR/.github/workflows/release.yml" || \
  fail "release.yml must publish release checksums"
grep -q "dist/build-metadata.json" "$ROOT_DIR/.github/workflows/release.yml" || \
  fail "release.yml must publish firmware build metadata"
grep -q "dist/esp32-security-classification.json" "$ROOT_DIR/.github/workflows/release.yml" || \
  fail "release.yml must publish the ESP32 security classification"
grep -q "esp32-16mb-store-forward.csv" "$ROOT_DIR/.github/workflows/release.yml" || \
  fail "release metadata must identify the ESP32 partition layout"
grep -q "overwrite_files: false" "$ROOT_DIR/.github/workflows/release.yml" || \
  fail "release workflow must not overwrite immutable firmware assets"
grep -q "Verify exact published assets" "$ROOT_DIR/.github/workflows/release.yml" || \
  fail "release workflow must verify the exact published firmware assets"
grep -q "docs/releases.md" "$ROOT_DIR/README.md" || \
  fail "README.md must link the firmware release and rollback runbook"

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

if grep -Eq 'LittleFS\.begin\([[:space:]]*true' "$ROOT_DIR/src/StoreForwardQueue.cpp"; then
  fail "store-and-forward must not format LittleFS automatically on mount failure"
fi
grep -q 'LittleFS.begin(false' "$ROOT_DIR/src/StoreForwardQueue.cpp" || \
  fail "store-and-forward must mount LittleFS without automatic formatting"

git -C "$ROOT_DIR" check-ignore --quiet include/AppSecrets.h || \
  fail "include/AppSecrets.h must remain ignored"
grep -q "APEXI_OTA_PASSWORD" "$ROOT_DIR/include/AppSecrets.example.h" || \
  fail "include/AppSecrets.example.h must document APEXI_OTA_PASSWORD"
grep -q "upload_protocol = espota" "$ROOT_DIR/platformio.ini" || \
  fail "platformio.ini must provide the OTA upload environment"
grep -A8 -q 'APEXI_PRODUCTION_SECURITY_REQUIRED=1' "$ROOT_DIR/platformio.ini" || \
  fail "production candidate must compile the fail-closed runtime security gate"
grep -q 'check_production_security.py' "$ROOT_DIR/docs/production-security.md" || \
  fail "production security runbook must name the machine-checkable audit"
if git -C "$ROOT_DIR" ls-files --error-unmatch -- include/AppSecrets.h >/dev/null 2>&1; then
  fail "include/AppSecrets.h contains local credentials and must not be tracked"
fi


for target in logger-nodemcuv2 logger-esp32 logger-tinyc6 dash-waveshare-s3-128; do
  grep -q "^\\[env:$target\\]" "$ROOT_DIR/platformio.ini" || fail "missing target $target"
  for workflow in "$ROOT_DIR/.github/workflows/build-firmware.yml" "$ROOT_DIR/.github/workflows/release.yml"; do
    grep -q -- "-e $target" "$workflow" || fail "$(basename "$workflow") does not build $target"
    grep -q ".pio/build/$target/firmware.bin" "$workflow" || fail "$(basename "$workflow") does not package $target"
  done
done
for workflow in "$ROOT_DIR/.github/workflows/build-firmware.yml" "$ROOT_DIR/.github/workflows/release.yml"; do
  grep -q -- "-e logger-esp32-production-candidate" "$workflow" || fail "missing production candidate build"
  grep -q "test_signed_release_crypto.py" "$workflow" || fail "missing real signature verification tests"
  grep -q "rich-click<2" "$workflow" || fail "missing esptool 5 CLI dependency"
done

echo "repo-contracts: ok"
