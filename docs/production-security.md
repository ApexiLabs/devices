# ESP32 production security and recovery

## Release status

The checked-in `logger-esp32` environment is a development build. The
`logger-esp32-production-candidate` environment compiles the firmware-side
`APEXI_PRODUCTION_SECURITY_REQUIRED=1` gate, but the bundled precompiled Arduino
SDK currently has Secure Boot and flash encryption disabled. Neither environment
is a production-approved image. ESP8266/NodeMCU is a compatibility and bench
target only and must not be commissioned as production hardware.

Run the machine-readable SDK audit before describing any ESP32 artifact as
production-eligible:

```sh
python3 scripts/check_production_security.py /path/to/sdkconfig \
  --build-flags '-D APEXI_PRODUCTION_SECURITY_REQUIRED=1' \
  --require-production
```

The command fails unless the runtime gate, Secure Boot v2, signed application
binaries, release-mode flash encryption, encrypted NVS, and bootloader rollback support are all
enabled. It also rejects insecure Secure Boot and UART/JTAG exceptions, including
`CONFIG_SECURE_BOOT_ALLOW_JTAG`, `CONFIG_SECURE_BOOT_ALLOW_ROM_BASIC`,
`CONFIG_SECURE_BOOT_INSECURE`, and the `CONFIG_SECURE_FLASH_UART_BOOTLOADER_ALLOW_*`
plaintext/cache options. Its success is necessary, not sufficient: the release evidence must
also contain per-device eFuse inspection and update/rollback results.

## Manufacturing profile

Production hardware requires an ESP-IDF-based or equivalently controlled build
whose SDK configuration includes:

- `CONFIG_SECURE_BOOT_V2_ENABLED=y`
- `CONFIG_SECURE_BOOT_BUILD_SIGNED_BINARIES=y`
- `CONFIG_SECURE_FLASH_ENC_ENABLED=y`
- `CONFIG_SECURE_FLASH_ENCRYPTION_MODE_RELEASE=y`
- `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`
- `CONFIG_NVS_ENCRYPTION=y`, with an encrypted 4 KiB `nvs_keys` partition
- firmware build flag `APEXI_PRODUCTION_SECURITY_REQUIRED=1`

The secure profile also marks LittleFS encrypted. The installed precompiled SDK header lists encrypted partitions as an `ESP_ERR_INVALID_STATE` mount outcome; compatible encrypted queue operation has not been established. `APEXI_ENCRYPTED_QUEUE_QUALIFIED` defaults to zero and is not enabled in any real build. The runtime network/writer gate and SDK production eligibility remain blocked until an encrypted-filesystem implementation and physical evidence are reviewed. Do not set that flag merely to make an audit pass, and do not remove the partition encryption flag as a workaround. The ordinary development partition table is unchanged.

Keep the Secure Boot signing key in the release signer/HSM; never place it in a
repository, CI log, provisioning bundle, or logger. Generate a unique flash
encryption key for each device. During manufacturing, install the signed
bootloader, partition table and application, enable Secure Boot v2 and
release-mode flash encryption, then disable insecure JTAG/UART download paths as
supported by the selected ESP32 revision. Read back and retain only non-secret
eFuse/security state and artifact digests as evidence. Treat eFuse operations as
irreversible and require a two-person reviewed manufacturing procedure before
burning production boards.

Provision owner credentials only after flash encryption **and NVS encryption** are active and verified. Flash encryption alone does not encrypt ordinary NVS entries. The secure partition template in `production/partitions-esp32-secure.csv` adds an encrypted NVS-key partition by reducing the NVS data partition from 20 KiB to 16 KiB. It is not selected by PlatformIO, must never be installed through application OTA, and requires an explicitly approved factory migration, credential reissue and storage qualification. Existing development layouts remain unchanged. The production runtime network/update gate additionally requires the secure SDK controls and encrypted key partition; two positive fuse flags alone no longer suffice.
The firmware checks secure-boot and flash-encryption state before networking; a
production-candidate image on an unenrolled board reports
`NETWORK_DISABLED=production-security-required`. Generic development-mode flash
encryption is reported but cannot satisfy the runtime gate; the chip must report
`ESP_FLASH_ENC_MODE_RELEASE`. Secure Boot is the mechanism
that rejects unsigned or unapproved firmware at boot. HTTPS certificate
validation and application authentication do not replace it.

## Commissioning and local exposure

An unprovisioned production-candidate ESP32 has no fallback access point, local web service, upload,
or OTA service. Production-candidate local HTTP settings are disabled even after provisioning;
owner settings enter through the identity-bound USB workflow or the optional,
allow-listed server management path. `/api/live`, diagnostics, and serial status
may expose identity and security posture, but never Wi-Fi, OTA, MQTT, Cloudflare,
app bearer, recovery, flash-encryption, or signing secrets.

The existing USB provisioning payload is plaintext on the operator's local
serial connection. Until a recovery-credential proof protocol is implemented
and physically verified, commission only in a controlled manufacturing station,
block USB/UART access in the enclosure, revoke a bundle if its session may have
been observed, and do not claim the captured-local-traffic acceptance criterion.

## Update, rollback and recovery

Password-based Arduino OTA is disabled by default for ESP32 and is now unconditionally gated off in production-required builds, even if its feature flag is enabled. Neither `ArduinoOTA.begin` nor either `handle` path can run in that posture. Development behavior remains unchanged. The signed writer is not connected to a browser, serial, MQTT or HTTPS upload endpoint: production update delivery remains locked until an authenticated, authorized service is implemented. `SignedOta` is a single-owner internal API, not an authorization boundary.

The ESP-IDF writer checks secure SDK/runtime posture and a confirmed running image before erasing only the next inactive OTA slot. It accepts bounded sequential chunks up to 4096 bytes and requires the exact declared total size. Truncation, overrun, cancellation or write failure aborts the handle without selecting the slot. `esp_ota_end` verifies the image and Secure Boot signature before project-name and equal-security-version checks; only then may `esp_ota_set_boot_partition` select it. It cannot update the bootloader, partition table, active slot or owner credentials. Calling it remains disabled on the current insecure SDK. An approved delivery service must also bind hardware/layout, artifact digest, signer/release authorization, maintenance mode, and update audit identity before invoking this API; streaming/flash latency must not be placed on the sampling path.

Production builds override the Arduino core `verifyRollbackLater()` hook so a pending image is **not** automatically confirmed before setup. Main-loop health confirmation requires valid non-faulted sensor readings, a ready durable queue, and a recent HTTPS status acknowledgement with JSON `accepted: true` and `status: ok` from the certificate-validated authenticated app endpoint. These conditions and security posture must hold for 10 seconds within a 120-second boot window. Socket connection or snapshot success alone is insufficient. Deadline arithmetic handles uptime wrap. On timeout the firmware attempts rollback once if a valid prior image exists. Failed confirmation/rollback or unknown boot metadata reports `recovery-required` and never fabricates a healthy image; reset of an unconfirmed image permits bootloader rollback. `/api/live` exposes the non-secret `ota_boot_health` state. A failure during setup/reset before confirmation is also governed by bootloader rollback.

This slice deliberately rejects automatic anti-rollback SDK mode and permits only equal security versions. It does not advance security-version eFuses, revoke signing keys, or implement an anti-downgrade release-authorization policy. Increasing the security version is a separate reviewed manufacturing/release operation because it can make the previous image permanently unbootable. A first factory image must have a known-good, correctly initialized OTA state established by the approved manufacturing flow; this code does not silently approve unknown factory metadata.

A failed health-confirmation write triggers one immediate rollback attempt when a known-good slot is available, just like a health timeout. If rollback is unavailable or fails, the terminal state is `recovery-required`. No subsequent loop iteration retries confirmation or rollback, and no failure is reported as confirmed.

## Reproducibility and external-signature evidence

The verifier directly pins and checks `esptool==4.11.0` and `cryptography==50.0.1`, and records both versions in evidence. Transitive dependencies and installation hashes are not yet locked; fully hash-locked build/verification environments and trusted provenance remain release gates, not claims made by this checker.

`production/sdkconfig.defaults` is a policy input template, not a secure SDK build. Current Arduino precompiled libraries cannot gain these controls from application `-D` flags. A controlled SDK integration, pinned dependencies/toolchain, review of the Arduino NVS initialization/recovery path, and two isolated builds are still required. IDF 4.x requires compile-time/date disabled and hidden source paths; IDF 5+ provides a reproducible-build option. Use a maintained SDK for the eventual production migration rather than treating the current 4.x compatibility template as a version recommendation.

The external signer/HSM receives reproduced **unsigned** images, never source-tree signing keys. With external signing, `CONFIG_SECURE_BOOT_BUILD_SIGNED_BINARIES` is intentionally disabled. The SDK classifier can report `sdk_security_ready`, but cannot report production eligibility on that basis: both actual signatures still need verification. RSA-PSS signatures can differ even for identical inputs, so compare unsigned artifacts before signing rather than requiring signed bytes to reproduce.

For each build directory provide `firmware.bin`, `bootloader.bin`, `partitions.bin`, generated `sdkconfig`, `build-flags.txt`, and a resolved `dependencies.lock`. Obtain the two signed images from the authorized external signer in a third directory. Verify with a public RSA-3072 key using the pinned verifier:

```sh
python -m pip install -r requirements-production-verification.txt
python scripts/verify_signed_release.py \
  --first-build /release/build-one --second-build /release/build-two \
  --signed /release/signed --public-key /release/approved-public.pem \
  --source-commit FULL_40_CHARACTER_COMMIT_SHA \
  --project apexilabs-logger --security-version 0
```

The checker compares both builds byte-for-byte, rejects insecure/contradictory SDK configuration, checks reproducibility controls, the encrypted-NVS/dual-slot partition profile, application chip/project/security version and slot sizes, and ensures signing changed only padding/signature sectors. It verifies private temporary snapshots against the public key to avoid mutable-path races. Private keys are rejected. Tool output is captured and never echoed; evidence contains hashes, versions and fixed classifications, not keys, source paths, bundles or HSM configuration. The public-key PEM SHA-256 is an evidence identifier, **not** the ESP32 eFuse key digest.

Output is always `candidate-evidence-only`, `production_approved: false` and `source_provenance_verified: false`. Identical files do not prove the claimed commit/toolchain produced them, the builds were independent, the signer was authorized, or the board has the matching trusted key. This tool is not a build runner, signer, release publisher or physical qualification. If a secure bootloader exceeds the template 28 KiB bootloader region, it is rejected; do not silently move the partition table or enlarge slots. Resolve the manufacturing layout first.

References: [ESP-IDF OTA/rollback](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32/api-reference/system/ota.html), [NVS encryption](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32/api-reference/storage/nvs_flash.html#nvs-encryption), [external HSM signing and public verification](https://docs.espressif.com/projects/esptool/en/release-v4/esp32/espsecure/index.html).

Owner recovery uses a deliberate five-second physical button hold to wipe owner
state. Re-provisioning or ownership transfer additionally requires a unique
per-device recovery credential and server/admin authorization before new scoped
credentials are issued. There is no fleet-wide recovery secret. The current
firmware implements the physical wipe but not recovery proof or server issuance,
so recovery authorization remains a production blocker.

For every production release, record the immutable device ID, hardware revision,
artifact SHA-256, signing-key identifier (not the key), security-version value,
eFuse security summary, old/new versions, update result, failed-boot rollback
result, recovery authorization result, and credential revocation result. Never
record secret values. Follow [Firmware releases and rollback](releases.md) for
the immutable release evidence, and [ESP32 identity and provisioning](provisioning.md)
for owner-state handling.

## Open production blockers

- migrate from the precompiled Arduino SDK to a reproducible secure ESP-IDF
  build and manufacturing signer;
- integrate an authenticated/authorized streaming delivery service with the signed writer and enforce artifact/hardware/release authorization; implement and rehearse the governed anti-rollback policy;
- qualify the secure partition/NVS migration, initial OTA metadata and bootloader size; establish trusted reproducible-build provenance and signer authorization;
- implement per-device recovery proof plus server/admin credential issuance;
- physically validate nearby-wireless denial, serial capture handling, eFuse
  state, signed/unsigned images, failed boot rollback, and credential redaction.

These blockers keep APE-82 In Progress.
