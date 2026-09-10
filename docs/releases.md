# Firmware releases and rollback

Firmware releases are immutable builds of an existing `vMAJOR.MINOR.PATCH` tag. Release Please creates the version commit, tag, changelog and GitHub Release from protected `main`; the tag-triggered `release-firmware` workflow builds and attaches the board-specific artifacts.

The current workflow publishes development artifacts, not production-approved firmware. SHA-256 proves download integrity but is not a boot signature. ESP8266 is not a production target, and the current precompiled ESP32 Arduino SDK does not enable Secure Boot or flash encryption. See [ESP32 production security and recovery](production-security.md). A future production workflow must compile `APEXI_PRODUCTION_SECURITY_REQUIRED=1`, pass `scripts/check_production_security.py --require-production`, sign the bootloader/application outside the repository, and attach the non-secret security classification to release evidence.

## Promotion

APE-82 now provides `scripts/verify_signed_release.py` for read-only candidate evidence: two unsigned build outputs must match, and externally signed bootloader/application images must verify against the public RSA-3072 key and secure partition profile. The verifier does not build, sign, publish, establish provenance or approve production hardware. Its output always remains candidate-only. External signing intentionally disables SDK build-time signing; follow the generated-SDK audit and artifact verification procedure in [production security](production-security.md), not a build-time-signing flag alone. Secure SDK migration, encrypted LittleFS compatibility, hash-locked toolchains, authorized delivery and physical boot/rollback evidence remain gates. Current release publication and Release Please settings are unchanged.

1. Merge conventional commits to `main` only after required verification passes.
2. Review the Release Please PR. Confirm its version and changelog, then merge it through normal protection.
3. Confirm the tag points at that merge commit and the `release-firmware` workflow succeeds.
4. Download `SHA256SUMS` and `build-metadata.json` from the GitHub Release. Run `sha256sum --check SHA256SUMS` in the download directory.
5. Confirm the metadata tag and commit match the GitHub Release, then select the artifact whose environment, flash size, and partition layout match the physical logger.
6. Install on a non-race logger first and run the commissioning checks in [Hardware Setup](hardware-setup.md).

The release job validates the tag/checkout identity, verifies every prepared checksum, refuses to overwrite an existing asset, downloads the exact published assets, verifies their checksums again, and compares them byte-for-byte with the build outputs. A failure therefore remains visible and cannot claim success after a partial or mismatched publication.

## Manual or emergency publication

Use the `release-firmware` workflow's **Run workflow** action only for an existing SemVer tag. Enter the complete tag such as `v0.2.1`; the job checks out and validates that tag rather than the selected branch. Do not create a release from an untagged branch or upload locally built binaries to an existing release.

If a run fails before assets are uploaded, correct the workflow defect and rerun it for the same tag. If any asset was uploaded, do not replace it: preserve the failed release as incident evidence, create a new patch tag after the correction, and publish new immutable artifacts.

## Rollback rehearsal

Rollback is a forward operational action using the prior known-good release; tags and releases are never moved.

1. Record the current logger version, board/flash layout, and queue diagnostics. Preserve or drain queued telemetry before changing the partition.
2. Download the prior release's `SHA256SUMS`, metadata, and matching firmware artifact. Verify all checksums and confirm its target/layout.
3. For development NodeMCU, upload the matching application binary with the documented serial or OTA procedure. For ESP32 development hardware, use the application binary only when the installed partition layout is already compatible; otherwise use the matching factory image after explicitly accepting that a full flash operation can erase settings and queued data. Production rollback must use a signed image accepted by Secure Boot and the governed security-version policy.
4. Re-run the commissioning checklist, verify the local version and live sequence, and confirm ingestion in the telemetry app.
5. Record the rollback release, device ID, reason, verification result, and whether queue data was preserved.

For a rehearsal, perform these steps on a non-race logger: install the new release, verify it, reinstall the immediately preceding known-good release, verify it, then reinstall the new release. A production release is not considered track-ready until that evidence exists for each supported physical target in use.

## Current development artifacts

Both CI and release publication build `logger-nodemcuv2`, `logger-esp32`, `logger-tinyc6`, and `dash-waveshare-s3-128`. Application artifacts are named `apexi-logger-nodemcuv2-firmware.bin`, `apexi-logger-esp32-firmware.bin`, `apexi-logger-tinyc6-firmware.bin`, and `apexi-dash-waveshare-s3-128-firmware.bin`, with matching ELF files. ESP32 targets also publish `apexi-<target>-factory.bin` using the platform-generated factory image and its target-specific offsets. Metadata records all four targets and their flash/partition layout.

`logger-esp32-production-candidate` separately proves the fail-closed firmware gate compiles; it is not a production approval or a published replacement for a development image. The pioarduino SDK audit reads `framework-arduinoespressif32-libs/esp32/sdkconfig` under the PlatformIO packages directory. CI installs the esptool 5 CLI dependencies explicitly; cryptographic release tests use the pinned `requirements-production-verification.txt` packages in a separate `.verification-venv` to avoid conflicting esptool versions.
