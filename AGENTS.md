# Agent Notes

- Product layout and APE-90 provenance: [`docs/repository-layout.md`](docs/repository-layout.md). Run build and verification commands from this repository root.
- Keep shared BLE constants, frames and system event records in `shared/protocols/`; never duplicate them per product.
- Hardware revisions in `<product>/hardware/revision.json` are independent of firmware versions. Do not infer PCB revisions from firmware releases.

- After significant firmware changes, run the available host-side verification in `./scripts/run-host-tests.sh`. If PlatformIO is available, also run `pio run` so the embedded build stays healthy.
- This repo uses a project-local PlatformIO setup. For reliable local firmware builds, prefer `PLATFORMIO_CORE_DIR=$PWD/.platformio PLATFORMIO_SETTING_ENABLE_TELEMETRY=no ./.venv/bin/pio run` over a global `pio` invocation.
- See [`docs/repo-contracts.md`](docs/repo-contracts.md) for source ownership and the repo verification contract.
- After feature, configuration, hardware, or behavior changes, update `README.md` and `docs/hardware-setup.md` so the documentation stays aligned with the current firmware and wiring expectations.
- Keep the sensor model extensible. Sensor definitions live in `shared/libraries/AppConfig.h` via `kSensorConfigs`; when adding channels, prefer extending that configuration rather than reintroducing hardcoded per-sensor paths in the firmware, web UI, CSV logging, or dashboard code.
- Keep hardware-sensitive defaults aligned with the docs. If pin mappings, display controller assumptions, ADC/shunt behavior, Wi-Fi behavior, or BOM guidance change, update the relevant config headers and documentation together.
- Add or update automated tests for new logic and behavior changes so coverage keeps pace with the firmware. Hardware-independent logic should stay covered by the host test suite when practical.
- Keep GitHub release automation aligned with repo changes. If versioning, release tagging, firmware artifacts, or published release behavior changes, update the workflows under `.github/workflows/` and document the release process in `README.md`.
- Keep local-only tool state out of git. `.venv/`, `.platformio/`, `.snyk-cache/`, and `.snyk-home/` are intentionally ignored and should stay uncommitted.
- Use Conventional Commits for commit messages: `feat:`, `fix:`, `docs:`, `test:`, `refactor:`, `chore:`, `perf:`, `build:`, and `ci:`. Use `!` or `BREAKING CHANGE:` for breaking changes so `release-please` can classify them correctly.
- Use SemVer tags and GitHub Release names in the form `vX.Y.Z`.
