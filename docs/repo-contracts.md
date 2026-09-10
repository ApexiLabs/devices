# Repo Contracts

This repo uses a small set of source-of-truth files for the important facts, and scripts enforce the automated checks around them.

## Source Ownership

| Concern | Owning file(s) | Notes |
| --- | --- | --- |
| System events and remote files | `include/SystemEvents.h`, `src/SystemLog.cpp`, `src/RemoteLogs.cpp` | Versioned BLE records, bounded SD storage and outbound HTTPS file transfer; see `system-logs.md` |
| PlatformIO environment and board target | [`platformio.ini`](../platformio.ini) | Canonical source for build env name, board ID, and build flags |
| Apexi Logger entry point | [`src/logger_main.cpp`](../src/logger_main.cpp) | Sensor acquisition, logging, upstream upload, and Dash Link central |
| Apexi Dash entry point | [`src/dash_main.cpp`](../src/dash_main.cpp) | Round LCD, dash web UI, and Dash Link peripheral |
| Dash live LCD preview | [`include/DashLcdBitmap.h`](../include/DashLcdBitmap.h), [`include/DashWebUi.h`](../include/DashWebUi.h) | Indexed BMP encoding of the actual render buffer; changed-frame polling and stale/pause state |
| Dash gauge colour/alarm policy | [`include/DashGauge.h`](../include/DashGauge.h), [`include/DashGaugeJson.h`](../include/DashGaugeJson.h), [`include/DashDisplayLayout.h`](../include/DashDisplayLayout.h) | Finite ordered colour points, opt-in fresh-only alarm limits, stable sensor IDs/units, bounded JSON replacement and fixed circular layout |
| Dash troubleshooting history | [`include/DashDiagnostics.h`](../include/DashDiagnostics.h), [`src/dash_main.cpp`](../src/dash_main.cpp) | Bounded RAM history and counters; system events are forwarded to Logger SD, not Dash flash |
| Dash web UI and OTA upload helper | [`include/DashWebUi.h`](../include/DashWebUi.h), [`scripts/upload-dash-ota.py`](../scripts/upload-dash-ota.py) | Logger-matched status UI and authenticated application-only OTA upload |
| Dash sensor transport and display configuration | [`include/DashTelemetry.h`](../include/DashTelemetry.h), [`src/DashLink.cpp`](../src/DashLink.cpp), [`src/dash_main.cpp`](../src/dash_main.cpp) | Bounded versioned BLE frames, freshness/fault handling, and authenticated NVS-backed display settings |
| Pin map | [`include/PinDefinitions.h`](../include/PinDefinitions.h) | Canonical source for NodeMCU D-label/GPIO assignments |
| Legacy logger display wiring handoff | [`include/LoggerDisplayTFTSetup.h`](../include/LoggerDisplayTFTSetup.h) | Must consume the pin macros from `PinDefinitions.h`; its name intentionally avoids TFT_eSPI's auto-loaded `tft_setup.h` |
| Separate dash BLE protocol and fixed Waveshare LCD wiring | [`include/DashLinkProtocol.h`](../include/DashLinkProtocol.h), [`include/WaveshareDashTFTSetup.h`](../include/WaveshareDashTFTSetup.h) | UUIDs and handshake must stay compatible across both firmware images |
| Feature toggles, RTC selection, sensor config, MQTT/HTTPS upload and OTA config | [`include/AppConfig.h`](../include/AppConfig.h) | Canonical source for firmware configuration defaults |
| Persistent device settings | [`include/RuntimeSettings.h`](../include/RuntimeSettings.h), [`src/RuntimeSettings.cpp`](../src/RuntimeSettings.cpp) | Checksummed flash-backed upstream endpoint, upload enable flag, NTP servers, and timezone settings |
| Device authorization | `include/LoggerAuthorization.h`, `src/LoggerAuthorization.cpp`, `include/DeviceAuthorizationPolicy.h` | Owner-approved authorization, origin-bound NVS credentials, staged rotation and background HTTPS; release gates in `device-authorization.md` |
| Shared HTTPS and replay scheduling | `include/HttpsExchange.h`, `include/HttpsWorker.h`, `src/HttpsWorker.cpp`, `include/HttpsPacing.h`, `src/LiveUploadAsync.cpp` | Single-owner persistent TLS, immutable bounded requests, main-loop queue ownership and throughput counters; see `https-replay.md` |
| Onboard store-and-forward | [`include/StoreForwardQueue.h`](../include/StoreForwardQueue.h), [`src/StoreForwardQueue.cpp`](../src/StoreForwardQueue.cpp), [`partitions/esp32-16mb-store-forward.csv`](../partitions/esp32-16mb-store-forward.csv) | ESP32 LittleFS queue format, rotation policy, capacity, and flash partition ownership |
| Live transport payload schema version | [`include/Logic.h`](../include/Logic.h) | `Logic::kLivePayloadSchemaVersion` is the canonical version emitted in MQTT and HTTPS live/status payloads |
| Wiring, detailed BOM, commissioning guidance, pin table | [`docs/hardware-setup.md`](./hardware-setup.md) | Human-oriented hardware source of truth |
| Project overview, build entrypoints, verification entrypoints | [`README.md`](../README.md) | Summary only; should link to owning docs instead of duplicating them |
| Agent workflow expectations | [`AGENTS.md`](../AGENTS.md) | Operational guidance and pointers, not a second source of hardware truth |
| Host-test entrypoint | [`scripts/run-host-tests.sh`](../scripts/run-host-tests.sh) | Canonical fast logic-test command |
| Repo-wide verification wrapper | [`scripts/verify-repo.sh`](../scripts/verify-repo.sh) | Canonical local/CI verification entrypoint |
| CI build/release behavior | [`.github/workflows/`](../.github/workflows/) | Must stay aligned with `platformio.ini` env/artifact paths |

## Verification Contract

### Fast path

Use for quick local checks and CI contract validation:

```sh
./scripts/verify-repo.sh --fast
```

This must run:
- host logic tests
- repo contract checks

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
- If the pin map changes, update [`include/PinDefinitions.h`](../include/PinDefinitions.h) first, then align [`include/LoggerDisplayTFTSetup.h`](../include/LoggerDisplayTFTSetup.h) and [`docs/hardware-setup.md`](./hardware-setup.md).
- If RTC, display, dash-link, SD, live-upload, or OTA defaults change, update [`include/AppConfig.h`](../include/AppConfig.h) first, then align docs.
- If the onboard queue capacity or record format changes, align `StoreForwardQueue`, the ESP32 partition table, local status fields, and recovery documentation together.
- If live/status MQTT payload shape changes incompatibly, bump `Logic::kLivePayloadSchemaVersion`, keep version `1` compatibility documented, and align bridge/app tests before rollout.
- Keep README concise. Detailed hardware descriptions belong in [`docs/hardware-setup.md`](./hardware-setup.md).
- Add new durable docs only when they reduce ambiguity that cannot be enforced another way.
