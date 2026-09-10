# ESP32 store-and-forward recovery

The onboard queue is an outage buffer for the 16 MiB ESP32 layout, not a replacement for removable SD logging. ESP32 HTTPS snapshots are persisted before transmission and use at most 10 MiB of the LittleFS partition. ESP8266 has no onboard replay and retains its synchronous HTTPS implementation. ESP32 layouts without enough LittleFS capacity report the queue as unavailable and use bounded, volatile upload slots instead.

## Nonblocking ESP32 HTTPS execution

The Arduino loop has configured deadlines of 10 ms for sampling, 50 ms for CSV logging, 100 ms for display updates, and 250 ms for HTTPS capture. It never calls blocking HTTPS APIs on ESP32. A FreeRTOS worker on core 0 owns HTTP/TLS work, including DNS, connection, handshake, POST and response reads. The worker has a 12 KiB stack and one fixed request/result exchange (6144-byte payload and response limits, 512-byte URL, 2048-byte bearer, 256-byte Access ID and 512-byte Access secret). An oversized request fails closed; a truncated or oversized response is a retryable transport failure. Credentials are copied for each request, never shared through mutable settings pointers, and cleared from the exchange after completion.

The state machine is `idle -> requested -> complete -> idle`. Main-loop submission and completion polling never wait for the worker. Each loop performs at most one submission or completion, not a replay burst. The main loop alone owns queue files, bearer rotation/NVS, remote configuration, diagnostics and UI state. The worker owns no references to those objects. Sampling can proceed while the network is stalled; this is not a hard-real-time guarantee for LittleFS, I2C, SD, display, OTA, or scheduler latency. Physical deadline/watchdog measurements remain mandatory. MQTT reconnect and ESP8266 HTTPS blocking behavior are unchanged and outside this implementation.

Status has priority at startup and every 30 seconds, measured from **completed attempts**, including failures. Failure backoff starts at request completion. A rejecting status endpoint therefore yields snapshot work between heartbeat attempts instead of starving replay. Explicit configuration/pairing changes request a fresh status. Credential rotation acknowledgement/proof are separate transactions: only completed accepted responses advance durable rotation state; failed candidate proof requests one old-bearer fallback after backoff and retains the candidate for the next periodic heartbeat. A reboot during any request leaves durable snapshots available for at-least-once replay.

Persist-before-send gives new captures their session/sequence identity once and keeps them behind older pending records. A late response only acknowledges the exact submitted head. If capacity rotation removed that head meanwhile, completion cannot pop its replacement. Failed permanent snapshot validation is counted as a queue drop; retryable failures remain durable. With no usable queue, one RAM slot plus one in-flight snapshot bounds memory. Slot overflow is counted; on retryable failure the older snapshot replaces any newer RAM-slot record, also counting that loss. RAM slots do not survive reboot.

### Oldest-record diagnostics

The local Diagnostics page and `/api/live` expose `system.store_forward_oldest` as an allowlisted object containing `session_id`, `sequence`, `timestamp`, and `age_seconds`, or `null` for an empty/unavailable/unreadable record. No sensors, payload body or credentials are exposed by this object. Age uses exact valid UTC calendar timestamps, independent of the configured local timezone; uptime fallback, invalid calendar dates, or a wall clock before the record timestamp yield `null`, not a misleading zero. Age is wall-clock-derived and cannot certify clock accuracy. `system.upload_capture_drops` is a saturating per-boot count of captures not retained and volatile-slot loss, separate from persistent queue capacity/rejection drops. These additions are local diagnostics only; the server's existing seven-field periodic status contract is unchanged.

## Durable format

The queue has two append-only segment files, `/sfq-0.log` and `/sfq-1.log`, plus `/sfq.meta`. Each record contains the `SFQ1` magic, a 32-bit payload length, a 32-bit FNV-1a checksum, and the exact JSON snapshot bytes. Payloads are limited to 4096 bytes. Metadata format version 2 contains the active segment, replay head for both segments, cumulative capacity drops, corruption repairs, quarantined bytes, and its own FNV-1a checksum. Version 1 metadata is migrated in place without rewriting segment records.

At startup, firmware scans every record from the committed head. A complete record with a wrong header, invalid length, truncated payload, or wrong checksum ends the valid prefix. The invalid suffix is copied to `/sfq-0.corrupt` or `/sfq-1.corrupt` before the live segment is repaired. The local diagnostics page and `/api/live` expose `store_forward_corruption_events`, `store_forward_quarantined_bytes`, and `store_forward_error`. Existing quarantine files are never replayed.

## Interruption semantics

Metadata v2's reserved flag byte now preserves uncertainty about historical drop counts after metadata-loss recovery. A nonzero flag causes periodic status to omit `system.store_forward_dropped_records`; rescanned pending records remain reportable. Existing v1/v2 metadata with a known count remains compatible. Drop increments saturate at the unsigned 32-bit maximum. See the [status diagnostics contract](status-diagnostics.md).

| Interruption | Deterministic result after reboot |
| --- | --- |
| During record append | The last incomplete record is quarantined; the preceding valid records remain ordered and replayable. |
| During metadata replacement | Segment records are rescanned. A replay acknowledgement that was not durably committed may be sent again, giving at-least-once delivery. |
| After acknowledgement commit but before empty-segment reclamation | The committed head prevents replay; startup safely reclaims the empty segment. |
| During segment rotation | The previously committed active segment remains recoverable. Capacity drops are committed with the rotation metadata. |
| Mount failure | Existing data is never formatted. First-use initialization is permitted only after proving the entire selected partition is erased; otherwise report a fault and preserve it. |
| Invalid server response during replay | Retryable failures remain queued. A permanently rejected record is explicitly popped and increments the drop counter. |

The ingest API must therefore remain idempotent for a device/session/sequence identity. At-least-once replay can repeat an acknowledged snapshot after an interrupted metadata commit, but it must not create duplicate final state. The matching gateway must preserve immutable device/session/sequence identity across retries and distinguish records that share a legacy whole-second source timestamp. Capability-gated batching depends on the collision-safe ledger described in the HTTPS replay contract. The internal receiver acknowledges storage to the app; firmware requires the public app's HTTP `200`, `status: ok`, and boolean `accepted: true` before advancing a singleton. For capability-gated batches it also requires the exact batch ID and full accepted count. See [HTTPS replay](https-replay.md) for collision-safe storage requirements. For singleton requests, `408`, `425`, `429`, transport failures, and `5xx` remain queued, while permanent `400`, `404`, `413`, and `422` rejects are dropped and counted.

## Capacity and endurance

Usable record count is approximately `configured queue bytes / (JSON payload bytes + 12)`: classic 16 MB ESP32 defaults to 10 MiB and TinyC6 to 1 MiB. Retention is that count divided by the configured HTTPS publish rate. At the current 250 ms interval, a 512-byte snapshot in the classic 10 MiB queue retains about 83 minutes; the maximum 4096-byte payload retains about 11 minutes. TinyC6 retention is about one tenth of those values. Actual commissioning must measure the emitted payload size for the configured sensor set. A multi-hour outage can exceed the buffer, at which point rotation deliberately removes the oldest whole segment and increments `store_forward_dropped_records`.

Every ESP32 HTTPS snapshot is now written before submission, including during healthy operation. A 512-byte payload at 4 Hz writes about 7.2 MiB per hour (7.5 MB decimal) before filesystem overhead. Acknowledgement metadata, segment reclamation and LittleFS amplification add writes, particularly when the queue drains completely after every request. This deliberately trades connected-operation endurance for durable ordering during asynchronous transmission. It is not yet qualified for continuous production use: measure physical write amplification, erase-cycle lifetime and capture latency before release. LittleFS wear levelling and the flash vendor's erase-cycle rating determine service life. Use SD logging when the required outage retention exceeds the measured buffer or when long-term archives are required.

Replay submits the oldest record, or a bounded oldest-first batch when the server advertises support, whenever the worker is available, independently of capture cadence. A healthy connection only drains backlog if its sustainable completion rate exceeds the capture rate after heartbeat overhead; no minimum drain rate is claimed. The 4-second socket/connect and 15-second TLS handshake limits apply in the worker, not on the sampling path, and are not a single end-to-end transaction deadline (DNS and response phases may add latency). A stalled worker can fill the durable queue, but cannot cause unbounded RAM allocation or an inline HTTP wait.

## Recovery procedure

1. Record the queue counters and error shown on **Diagnostics**, and save `/api/live` if incident evidence is required.
2. Reboot once. Startup repair is non-destructive to the valid prefix and preserves an invalid suffix in the `.corrupt` file.
3. If LittleFS still will not mount, stop. Do not deploy firmware that calls `LittleFS.begin(true, ...)` and do not erase flash until the queued partition has been captured or explicitly declared expendable.
4. After evidence is captured and loss is accepted, perform an intentional full-device erase with `pio run -t erase`, then reinstall the known-good firmware and settings. This destroys settings, queue data, and quarantine evidence.
5. Confirm `store_forward_ready=true`, zero pending records, and an empty queue error before returning the logger to service.

Physical release evidence must include one interrupted append or metadata commit, a reconnect replay, observed queue counters, and confirmation that sensor sampling and the watchdog remain healthy.
