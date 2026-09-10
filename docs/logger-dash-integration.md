# Functional Logger and Dash integration

This integration replaces preservation PR #21 and stacked gauge PR #22. It combines their Logger/Dash foundation with the later functional bench changes and reconciles the current main-branch provisioning, queue recovery, status diagnostics, and signed-release safeguards.

## Compatibility decisions

- TinyC6, classic ESP32, legacy NodeMCU, and Waveshare S3 Dash remain separate targets. The production-candidate build checks enforcement code and is not a production-qualified image.
- Development HTTPS authorization uses the hardware-derived public identity, persisted installation identity/secret, and an app-issued canonical recorder ID. New boards do not inherit a shared `mda-logger` recorder identity. Existing legacy history requires explicit owner-approved migration. Unauthenticated HTTPS capture does not enqueue shared-ID telemetry; local SD capture remains independent.
- Existing USB-provisioned devices retain their provisioned settings and AppBearerRotation state. Development app authorization and USB bearer rotation are mutually exclusive. Remote log requests use the effective rotated bearer.
- Queue v2 metadata, v1 migration, quarantine, atomic acknowledgement, and saturating loss counters remain in place. TinyC6 adds the named default partition and initialization only after proving the entire partition is erased. Do not erase or reformat existing queued data during application updates.
- A single HTTPS worker serves telemetry, authorization, and remote logs. Batches require an advertised server capability and an exact full-count acknowledgement. Gateway/app collision-safe storage and reader support must be deployed before enabling that capability; single-snapshot fallback remains available.
- Production networking and Logger BLE share the provisioning/security gate. Development retains its authenticated local settings and recovery access point. Failed owner resets keep networking disabled, using a separate durable marker to resume interrupted cleanup before credentials are used again.
- Existing BLE handshake compatibility remains; newer upload-status evidence is optional. HTTPS green indicates accepted snapshots, which may be replayed data, rather than proof of the newest sample reaching storage.

## Evidence and qualification

Prior physical bench evidence is recorded in [HTTPS replay](https-replay.md), [device authorization](device-authorization.md), and [hardware setup](hardware-setup.md). These dated tests exercised pre-integration binaries; they must not be described as a physical test of the final merged image.

The merged source is checked with the full host/repository suite, all four firmware targets, the production-candidate compile, and pinned public-key release verification tests. Owner-reset fault tests cover partial clears, latch failures, read errors, and simulated restart boundaries. The PR records actual results for its final revision.

Remaining physical qualification includes the configured 4 Hz versus observed approximately 2.6 Hz capture rate, one observed 4970 ms local response during replay, flash endurance/power-loss behaviour, final battery calibration, and production security/update/rollback acceptance. The previously observed 1186-record backlog drained while capture continued without increasing the historical drop baseline; exact old-backlog payload parity was not established. Keep these limitations visible when deciding field or production use.
