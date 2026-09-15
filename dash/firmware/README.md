# Dash firmware

Build from the repository root with `pio run -e <environment>`.

Supported environments: `dash-waveshare-s3-128`. The root `platformio.ini` owns target configuration; shared headers live under `shared/`. Firmware releases retain the existing version train; hardware revisions live separately in `../hardware/revision.json`. See [release procedures](../../docs/releases.md).
