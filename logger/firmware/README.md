# Logger firmware

Build from the repository root with `pio run -e <environment>`.

Supported environments: `logger-nodemcuv2, logger-nodemcuv2-ota, logger-esp32, logger-tinyc6, logger-esp32-production-candidate`. The root `platformio.ini` owns target configuration; shared headers live under `shared/`. Firmware releases retain the existing version train; hardware revisions live separately in `../hardware/revision.json`. See [release procedures](../../docs/releases.md).
