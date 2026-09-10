# Logger authorization: APE-81 and APE-78

This is a development implementation, not production-qualified. On 2026-09-09 the matching app revision `5333a88` was deployed to dev and the TinyC6 received application-only OTA. Start/poll, restoration of the pending challenge across OTA, owner approval, credential acknowledgement and persistence across reboot were verified. The server confirmed accepted snapshots and independent InfluxDB readback of replayed historical samples. Full backlog drain, credential rotation and revoked-token recovery still require physical acceptance tests.

## Identity

ESP32 derives a stable public `esp32-<12 lowercase hex digits>` identity from its eFuse MAC. A MAC is not an authenticator. The device separately generates and persists a UUID installation ID and a 32-byte random installation secret. The app issues scoped bearer credentials only after explicit owner approval. ESP32 builds no longer consume the compiled `APEXI_APP_DEVICE_TOKEN`; ESP8266 retains its legacy flow.

The app returns the canonical telemetry device ID. Existing `mda-logger` ownership/history is preserved only through explicit owner-selected migration. The firmware does not rewrite queued telemetry identities. The active credential is bound to server host and port and is not forwarded after a server change.

## Connect and re-authorize

Authenticated Logger Settings provides **Connect / re-authorize device**. The local POST requires a per-boot CSRF token; status responses show only public state/user code and never a bearer, installation secret or polling proof. The network worker uses HTTPS with normal certificate validation, Cloudflare Access headers, no redirects, and bounded responses. It does not block sensor sampling while waiting for authorization requests.

The agreed server API is `/api/v1/logger-authorization/start`, `/poll`, and `/ack`:

1. Start supplies hardware ID, installation ID/secret and device name. The server returns a ten-minute user code plus a separate secret device proof.
2. The logger persists the challenge before polling at least five seconds apart. Only the user code appears in Settings.
3. The signed-in owner previews and approves the device in the app. A copied MAC alone cannot claim or recover a logger.
4. The issued candidate is saved separately from the old credential. The device acknowledges only after successful persistent storage.
5. After acknowledgement, the logger atomically promotes the candidate and restarts so upload and log-transfer clients load the same credential. Queued telemetry remains intact.

Invalid credentials do not automatically create new authorization attempts. Recovery requires a local Settings action and fresh owner approval. Cloudflare credentials and local web/OTA authentication remain separate from app authorization.

Authorization, telemetry upload and remote-log transfer share a single TLS-session budget to avoid ESP32-C6 allocation failures alongside BLE. Remote-log polling is not started without an issued credential. Authorization permits a 15-second certificate handshake on its background worker and reports numeric transport/TLS errors without exposing request proofs or secrets.

Failed remote-log requests back off for 60 seconds; the dev authorization release does not itself deploy remote-file-transfer endpoints. Status acknowledgement parsing selects only `status` and boolean `accepted`, so a larger desired-configuration response cannot invalidate a legitimate acknowledgement.

## Storage and power loss

One NVS value in `logger_authz` contains identity, origin-bound active credential, pending proofs, candidate and rotation state. Failed writes prevent acknowledgement or promotion. Reboot restores the pending state. Development-board NVS is not automatically encrypted; encrypted NVS/flash and secure boot remain separate production commissioning requirements.

Server ACK receipts remain replayable for 24 hours. If an ambiguous ACK later returns 410, the logger checks the saved candidate with `/auth/me`, without cookies or redirects. Promotion requires authenticated boolean true, bearer authentication, the exact issued logger subject, and `logger:ingest` scope. Anonymous/wrong-identity results cannot promote it. Network failures retain the candidate for retry. An expired unacknowledged candidate never silently replaces a working old credential.

## Rotation

The APE-78 protocol is reused: `desired_config.credential_rotation` supplies `type`, `version`, `nonce`, `token` and `overlap_expires_at`; status sends `management.credential_rotation_ack` with version, nonce and state `applied`. The firmware persists the candidate, acknowledges using the old bearer, verifies with the candidate on a later status request, then persists promotion and restarts. Replays must match the same version/nonce/token.

Security-only rotation envelopes can be processed while remote sensor management is disabled. The actual management flag remains false and no remote settings are applied. Automatic age-based scheduling is owned by the server and remains an explicit server-side release gate, not a second renewal protocol.

After a non-retryable rotation rejection, the logger checks the old bearer with one status request without a rotation acknowledgement. Only an explicitly accepted response and a successful persistent write discard the pending rotation. Failed verification or storage retains both credentials for recovery; an old-bearer response never promotes the candidate.

## Verification and release gates

Host tests cover durable writes/failures, candidate separation, record restoration, expiry, lost ACK, exact-identity verification, origin binding and rotation boundaries. TinyC6 builds cover the actual worker/NVS integration. Remaining gates: matching server/browser tests, owner-approved legacy migration, revoked-token recovery, automatic renewal with management off, physical power cuts, replay drain, and SD/BLE responsiveness during sustained transport. No successful end-to-end authorization is claimed yet.
