# Repo Contracts

This repo uses a small set of source-of-truth files for the important facts, and scripts enforce the automated checks around them.

## Source Ownership

| Concern | Owning file(s) | Notes |
| --- | --- | --- |
| System events and remote files | `shared/protocols/SystemEvents.h`, `logger/firmware/src/SystemLog.cpp`, `logger/firmware/src/RemoteLogs.cpp` | Versioned BLE records, bounded SD storage and outbound HTTPS file transfer; see `system-logs.md` |
| PlatformIO environment and board target | [`platformio.ini`](../platformio.ini) | Canonical source for build env name, board ID, and build flags |
| Apexi Logger entry point | [`logger/firmware/src/logger_main.cpp`](../logger/firmware/src/logger_main.cpp) | Sensor acquisition, logging, upstream upload, and Dash Link central |
| Apexi Dash entry point | [`dash/firmware/src/dash_main.cpp`](../dash/firmware/src/dash_main.cpp) | Round LCD, dash web UI, and Dash Link peripheral |
| Dash live LCD preview | [`dash/firmware/include/DashLcdBitmap.h`](../dash/firmware/include/DashLcdBitmap.h), [`dash/firmware/include/DashWebUi.h`](../dash/firmware/include/DashWebUi.h) | Indexed BMP encoding of the actual render buffer; changed-frame polling and stale/pause state |
| Dash gauge colour/alarm policy | [`dash/firmware/include/DashGauge.h`](../dash/firmware/include/DashGauge.h), [`dash/firmware/include/DashGaugeJson.h`](../dash/firmware/include/DashGaugeJson.h), [`dash/firmware/include/DashDisplayLayout.h`](../dash/firmware/include/DashDisplayLayout.h) | Finite ordered colour points, opt-in fresh-only alarm limits, stable sensor IDs/units, bounded JSON replacement and fixed circular layout |
| Dash troubleshooting history | [`dash/firmware/include/DashDiagnostics.h`](../dash/firmware/include/DashDiagnostics.h), [`dash/firmware/src/dash_main.cpp`](../dash/firmware/src/dash_main.cpp) | Bounded RAM history and counters; system events are forwarded to Logger SD, not Dash flash |
| Dash web UI and OTA upload helper | [`dash/firmware/include/DashWebUi.h`](../dash/firmware/include/DashWebUi.h), [`scripts/upload-dash-ota.py`](../scripts/upload-dash-ota.py) | Logger-matched status UI and authenticated application-only OTA upload |
| Dash sensor transport and display configuration | [`shared/protocols/DashTelemetry.h`](../shared/protocols/DashTelemetry.h), [`logger/firmware/src/DashLink.cpp`](../logger/firmware/src/DashLink.cpp), [`dash/firmware/src/dash_main.cpp`](../dash/firmware/src/dash_main.cpp) | Bounded versioned BLE frames, freshness/fault handling, and authenticated NVS-backed display settings |
| Pin map | [`shared/libraries/PinDefinitions.h`](../shared/libraries/PinDefinitions.h) | Canonical source for NodeMCU D-label/GPIO assignments |
| Legacy logger display wiring handoff | [`logger/firmware/include/LoggerDisplayTFTSetup.h`](../logger/firmware/include/LoggerDisplayTFTSetup.h) | Must consume the pin macros from `PinDefinitions.h`; its name intentionally avoids TFT_eSPI's auto-loaded `tft_setup.h` |
| Separate dash BLE protocol and fixed Waveshare LCD wiring | [`shared/protocols/DashLinkProtocol.h`](../shared/protocols/DashLinkProtocol.h), [`dash/firmware/include/WaveshareDashTFTSetup.h`](../dash/firmware/include/WaveshareDashTFTSetup.h) | UUIDs and handshake must stay compatible across both firmware images |
| Feature toggles, RTC selection, sensor config, MQTT/HTTPS upload and OTA config | [`shared/libraries/AppConfig.h`](../shared/libraries/AppConfig.h) | Canonical source for firmware configuration defaults |
| Persistent device settings | [`logger/firmware/include/RuntimeSettings.h`](../logger/firmware/include/RuntimeSettings.h), [`logger/firmware/src/RuntimeSettings.cpp`](../logger/firmware/src/RuntimeSettings.cpp) | Checksummed flash-backed upstream endpoint, upload enable flag, NTP servers, and timezone settings |
| Device authorization | `logger/firmware/include/LoggerAuthorization.h`, `logger/firmware/src/LoggerAuthorization.cpp`, `logger/firmware/include/DeviceAuthorizationPolicy.h` | Owner-approved authorization, origin-bound NVS credentials, staged rotation and background HTTPS; release gates in `device-authorization.md` |
| Shared HTTPS and replay scheduling | `logger/firmware/include/HttpsExchange.h`, `logger/firmware/include/HttpsWorker.h`, `logger/firmware/src/HttpsWorker.cpp`, `logger/firmware/include/HttpsPacing.h`, `logger/firmware/src/LiveUploadAsync.cpp` | Single-owner persistent TLS, immutable bounded requests, main-loop queue ownership and throughput counters; see `https-replay.md` |
| Onboard store-and-forward | [`logger/firmware/include/StoreForwardQueue.h`](../logger/firmware/include/StoreForwardQueue.h), [`logger/firmware/src/StoreForwardQueue.cpp`](../logger/firmware/src/StoreForwardQueue.cpp), [`logger/firmware/partitions/esp32-16mb-store-forward.csv`](../logger/firmware/partitions/esp32-16mb-store-forward.csv) | ESP32 LittleFS queue format, rotation policy, capacity, and flash partition ownership |
| Live transport payload schema version | [`logger/firmware/include/Logic.h`](../logger/firmware/include/Logic.h) | `Logic::kLivePayloadSchemaVersion` is the canonical version emitted in MQTT and HTTPS live/status payloads |
| Wiring, detailed BOM, commissioning guidance, pin table | [`docs/hardware-setup.md`](./hardware-setup.md) | Human-oriented hardware source of truth |
| Production hardware qualification plan and evidence schemas | [`docs/production-hardware-qualification.md`](./production-hardware-qualification.md), [`fixtures/qualification/`](../fixtures/qualification/) | Separates configured assumptions from physical acceptance evidence; blank templates never imply qualification |
| Project overview, build entrypoints, verification entrypoints | [`README.md`](../README.md) | Summary only; should link to owning docs instead of duplicating them |
| Agent workflow expectations | [`AGENTS.md`](../AGENTS.md) | Operational guidance and pointers, not a second source of hardware truth |
| Host-test entrypoint | [`scripts/run-host-tests.sh`](../scripts/run-host-tests.sh) | Canonical fast logic-test command |
| Repo-wide verification wrapper | [`scripts/verify-repo.sh`](../scripts/verify-repo.sh) | Canonical local/CI verification entrypoint |
| CI build/release behavior | [`.github/workflows/`](../.github/workflows/) | Must stay aligned with `platformio.ini` env/artifact paths |
| Production image classification | [`scripts/check_production_security.py`](../scripts/check_production_security.py), [`docs/production-security.md`](production-security.md) | Secure Boot v2, signed binaries, release-mode flash encryption, and rollback must all be present before an ESP32 image is production-eligible |
| Signed OTA and release evidence | [`logger/firmware/include/SignedOta.h`](../logger/firmware/include/SignedOta.h), [`logger/firmware/src/SignedOta.cpp`](../logger/firmware/src/SignedOta.cpp), [`logger/firmware/src/SignedOtaEsp32.cpp`](../logger/firmware/src/SignedOtaEsp32.cpp), [`scripts/verify_signed_release.py`](../scripts/verify_signed_release.py), [`production/`](../production/) | Internal signed inactive-slot writer, deferred boot-health confirmation, bounded rollback, and non-approving reproducibility/public-signature evidence; no authorized delivery ingress yet |
| Immutable identity and owner provisioning | [`logger/firmware/include/DeviceProvisioning.h`](../logger/firmware/include/DeviceProvisioning.h), [`logger/firmware/src/DeviceProvisioning.cpp`](../logger/firmware/src/DeviceProvisioning.cpp), [`logger/firmware/include/ProvisioningPolicy.h`](../logger/firmware/include/ProvisioningPolicy.h), [`logger/firmware/src/ProvisioningPolicy.cpp`](../logger/firmware/src/ProvisioningPolicy.cpp), [`docs/provisioning.md`](provisioning.md) | ESP32 eFuse identity, NVS ownership boundary, USB provisioning/factory-reset policy and operator runbook |
| App bearer rotation | [`logger/firmware/include/AppBearerRotation.h`](../logger/firmware/include/AppBearerRotation.h), [`logger/firmware/src/AppBearerRotation.cpp`](../logger/firmware/src/AppBearerRotation.cpp), [`logger/firmware/src/LiveUpload.cpp`](../logger/firmware/src/LiveUpload.cpp), [`docs/provisioning.md`](provisioning.md) | Durable staged/acknowledged/promoted ESP32 credential state, old-bearer recovery, and secret-free acknowledgement contract |
| Periodic status diagnostics | [`logger/firmware/include/StatusDiagnostics.h`](../logger/firmware/include/StatusDiagnostics.h), [`logger/firmware/src/StatusDiagnostics.cpp`](../logger/firmware/src/StatusDiagnostics.cpp), [`docs/status-diagnostics.md`](status-diagnostics.md) | Optional observed system signals, configuration lifecycle, recovery attempts, and persisted completed-boot counter |

## Verification Contract

### Fast path

Use for quick local checks and CI contract validation:

```sh
./scripts/verify-repo.sh --fast
```

This must run:
- host logic tests
- repo contract checks
- qualification record schema checks

### Full path

Use when the local toolchain is available or in firmware-oriented CI lanes:

```sh
./scripts/verify-repo.sh --full
```

This must run:
- the fast path
- the repo-local PlatformIO firmware build

## Change Rules

- If the board target changes, update [`platformio.ini`](../platformio.ini) first, then align the workflows.
- If the pin map changes, update [`shared/libraries/PinDefinitions.h`](../shared/libraries/PinDefinitions.h) first, then align [`logger/firmware/include/LoggerDisplayTFTSetup.h`](../logger/firmware/include/LoggerDisplayTFTSetup.h) and [`docs/hardware-setup.md`](./hardware-setup.md).
- If RTC, display, dash-link, SD, live-upload, or OTA defaults change, update [`shared/libraries/AppConfig.h`](../shared/libraries/AppConfig.h) first, then align docs.
- If the onboard queue capacity or record format changes, align `StoreForwardQueue`, the ESP32 partition table, local status fields, and recovery documentation together.
- If live/status MQTT payload shape changes incompatibly, bump `Logic::kLivePayloadSchemaVersion`, keep version `1` compatibility documented, and align bridge/app tests before rollout.
- Keep README concise. Detailed hardware descriptions belong in [`docs/hardware-setup.md`](./hardware-setup.md).
- Add new durable docs only when they reduce ambiguity that cannot be enforced another way.
