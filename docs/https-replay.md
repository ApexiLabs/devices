# HTTPS replay and responsiveness (APE-76)

The TinyC6 bench branch uses the `HttpsExchange`, `HttpsWorker` and `HttpsPacing` foundation from the newer APE-76 main-branch implementation, adapted to the bench device-authorization and remote-log clients. ESP8266 retains the existing synchronous path.

## Efficiency decision

Before the coordinated batch deployment, the app and gateway accepted one snapshot per request. The device made a new TLS connection per snapshot and replayed at most two records per capture cycle on the Arduino loop. A server measurement on 2026-09-09 found approximately 18 accepted snapshots/minute with stored samples about 56 minutes old. The configured capture interval is 250 ms (4 Hz), but blocking transport prevented that cadence being maintained.

The compatible first improvement is persistent HTTPS plus continuous FIFO replay on a worker. Physical verification found 2.9 accepted requests/second and local response times of 44–170 ms, with connection reuse confirmed. That is a major responsiveness improvement but still below 4 Hz. The next step is bounded batching, gated on collision-safe server storage and an advertised capability. A single HTTP 200 or a maximum sequence must not acknowledge missing records.

## Ownership and scheduling

One shared worker owns the TLS connection for telemetry, authorization and remote logs. Immutable bounded request/result buffers transfer ownership through release/acquire atomics. The Arduino loop never waits for the network and remains the sole owner of queue append/pop, device credentials and remote configuration. Other consumers defer while the worker is occupied.

Pending authorization and remote-log requests are explicitly scheduled ahead of the next telemetry request, so a permanent backlog cannot starve credential recovery or management. This arbitration does not interrupt an in-flight request.

The HTTP client consumes each response completely, rejects oversized/partial responses, disables redirects, clears per-request headers and reuses connections only for the same origin. Failed connections are closed and retried without changing record identities. Idle connections close after 15 seconds. TLS verification and Cloudflare Access headers remain enabled.

Snapshots are captured independently at the configured interval. Replay starts the next oldest record as soon as the preceding request is acknowledged; it does not wait for another capture tick. Heartbeats are paced from completed attempts so a failed heartbeat cannot monopolize the connection. Credential rotation may request an earlier status exchange. Unsupported remote-log requests retain their 60-second failure backoff.

Only strict `status: ok`, boolean `accepted: true`, HTTP 200 acknowledges a queued record as uploaded. Explicitly rejected records can instead move to the durable recovery archive described below; they are never counted as accepted. The submitted payload must still match the queue head before pop, so segment rotation during an in-flight request cannot acknowledge a different record. Ambiguous responses retain data for idempotent retry. The existing queue format, checksum and capacity/segment-overflow policy are unchanged. Overflow can still drop records if sustained delivery remains below capture; these drops must remain visible. Filesystem operations still run on the main loop and are not claimed to be hard real-time.

## Measurement

`/api/live` includes `system.upload_performance`, with per-boot `captured`, `accepted`, `capture_rejected`, `requests`, `reused`, `last_request_ms` and nullable `last_sample_epoch`. Reused counts describe an existing socket at request start, not guaranteed successful reuse. Accepted counts are acknowledged snapshot records (up to eight per batch), not a deduplicated server row count; requests also include status exchanges. Compare counter deltas and queue counts over the same interval; capture sequence alone is not an upload counter.

Acceptance requires responsive local requests, uninterrupted sensor/SD/Dash operation, accepted throughput above capture while a backlog exists, stable dropped-record counts, and advancing stored sample timestamps. Fresh HTTP acknowledgement alone does not establish fresh capture. Full drain must be observed separately.

Host tests cover request bounds, immutable ownership, pending-worker sampling deadlines, completed response transfer, retry pacing and timer wrap, credential state transitions, and strict acknowledgement parsing with large configuration bodies. Physical throughput and idle/error recovery remain necessary alongside host tests.

## Batch release gate

The firmware batch implementation was flashed to the TinyC6 bench logger on 2026-09-10 after coordinated dev-server verification. It defaults off and requires an accepted authenticated status response advertising `ingest_capabilities: {snapshot_batch_v1: true, max_snapshots: 8}`. The request route is `/api/v1/device/loggers/ingest/snapshot-batch`; its envelope contains `schema_version: 1`, `device_id`, a 32-lowercase-hex `batch_id`, and up to eight unchanged `snapshots`. Firmware limits the complete request to 6144 bytes; the server limit is 8192 bytes.

Success must be HTTP 200 with `status: ok`, boolean `accepted: true`, the exact `batch_id`, and integer `accepted_count` equal to the number submitted. Partial or ambiguous results never advance the queue. Retries retain the same batch body while running. After a valid complete ACK, matching heads are popped one per main-loop turn. A reboot may replay already accepted records, requiring server idempotency.

If an accepted status response withdraws batch capability, or the batch route returns 404/405, the client drops only the batch envelope and resumes individual requests. All underlying queue records remain available for retry.

Legacy queue timestamps have whole-second precision. Legacy Influx point identity did not distinguish sequence, so successful writes could overwrite same-second records. The coordinated gateway uses a durable identity ledger and isolated `logger_v2` storage series, preserving the original capture timestamp separately. Installed dev-server proof verified eight distinct same-second records, retry deduplication, and app-reader values before capability was enabled. Do not rewrite queued timestamps as a workaround; the capability remains a server-side release gate.

## Physical batch acceptance, 2026-09-10

The bench image is based on `07c0783` plus the existing working-tree changes; binary SHA-256 is `2360c4f0c25145b9fa3447c911c7136f6b288defd3041dfb0ab58a11c1998275`. App-only OTA preserved queue, settings, credentials and SD files. Dev app revision `472de7e` and gateway `cfb38d0` supplied the enabled capability.

Diagnostics now expose `batch_enabled`, completed `batch_requests`, and successful whole-batch `batch_accepted` counters under `system.upload_performance`. At 01:04:14 UTC the logger reported capability enabled and eight accepted batches/64 records. At 01:05:00 UTC it reported 42 accepted batches/336 records, with no batch failures. Initial pending records were 1186; pending records fell to 1013 while new captures continued. Replay was approximately 5.9 records/second versus 2.6 captures/second. This clears the transport bottleneck but does not establish 4 Hz capture timing; main-loop filesystem/sensor cadence still requires separate qualification.

The post-flash dropped-record baseline is 16549, including historical overflow; it must not be described as zero total loss. By 01:09:59 UTC the backlog had drained to one live in-flight record. Through 01:11:15 UTC, the queue stayed at one or two records while capture continued; the dropped count remained 16549, capture rejections remained zero, and SD/Dash remained ready. All 285 completed batch requests were accepted. Sustained backlog replay was about six records/second; the initial 1186-record backlog cleared in approximately six minutes while roughly 2.6 captures/second continued.

Most measured local responses were 46–428 ms, but one response took 4970 ms during drain. This outlier remains an unresolved responsiveness issue; the test does not establish consistently bounded UI latency. Actual capture cadence is still below the configured 4 Hz target.

At 01:11:30 UTC the queue remained at one record, with 289/289 accepted batches and 494 reused connections across 495 completed requests. The server's read-only raw prefix check at 01:11:26 UTC verified exactly 900 rows and 900 unique identities for sequences 1–900 of `mda-logger-boot-3748151581`, with no gaps/duplicates and all original source timestamps present. Sequence 1 was captured at 01:03:59 UTC; sequence 900 at 01:09:45 UTC. Pressure/temperature values were plausible against representative local readings.

No pre-flash FIFO payload inventory was retained, so queue drain plus stable drops does not establish exact old-backlog value parity. The new-boot check proves stored identity/count parity, not an exact comparison of every value against local original payloads. Old finalized Parquet artifacts do not automatically include late replay and are not valid raw-ingest parity evidence.

## Rejected payload recovery and local recording (APE-101)

A September 24 incident repeatedly returned gateway HTTP 422 for the same in-memory batch. A firmware update/reboot cleared the pending batch and restored replay; that alone did not prove that the original validation failure was fixed. The Logger SD file list contains daily CSV archives throughout the outage, including approximately 64 MB on September 24. Queue drop counters refer to the bounded LittleFS upload queue, not deletion from SD. A file listing does not establish complete sample-by-sample parity.

On ESP32 targets, a batch rejected with HTTP 400, 413 or 422 is split into individual requests without acknowledging or removing any member. Up to the original batch count are resolved individually before batching resumes. Transient failures, authentication failures and ambiguous acknowledgements retain records. Existing 404/405 compatibility fallback remains in place.

An individual HTTP 400/413/422 can leave replay only when the authenticated app response explicitly supplies `X-APX-Retryable: false` and its exact payload has first been saved and read back in a separate LittleFS recovery slot. A temporary file is flushed, verified, then renamed before the matching queue head is popped. Repeated recovery after a reset is idempotent. Archive failure or failed queue metadata commit retains the queue record. Rejected records never increment the accepted-upload counter or the queue-overflow drop counter.

There are 16 recovery slots, each limited to a 4096-byte payload, using less than 70 KiB of the existing 512 KiB filesystem reserve. Saved slots, including corrupt ones, are never overwritten automatically. A full/unavailable archive leaves replay pending and reports `upload_rejection_archive_error`; it does not disable SD recording. Normal finite queue capacity can still cause visible overflow if rejection or outage persists. Devices with no usable LittleFS cannot archive a rejected volatile record and retain it for retry.

Use the existing local Settings/Digest credentials to export `GET /api/upload-rejections` (slot manifest), then `GET /api/upload-rejections?slot=0` for each listed slot. Each response contains the original payload as a string and its HTTP status. Corrupt slots remain listed but return an error on export. No deletion or automatic re-import endpoint is provided. After a verified external backup, reclaiming archive slots requires a separately reviewed recovery procedure; never erase the whole filesystem to clear these records. The on-disk files are `/upload-rejected-00.bin` through `/upload-rejected-15.bin`, with a checksummed header and exact original payload bytes.

Sensor sampling and CSV writes execute before network, UI and upload work in the main loop. SD writes do not depend on upload acknowledgement, connectivity, or queue/archive capacity. `sd_rows_written` counts complete row writes accepted by the SD API during this boot; `sd_last_write_age_ms` reports their age, or null before the first successful write. Neither is a per-row power-loss durability guarantee: the existing flush interval still applies. Partial writes are errors and do not advance these counters; subsequent rows start on a new line. An interrupted row may remain in the CSV and must be rejected by a recovery/import validator.

Host regression tests exercise pending HTTP exchanges, permanent rejections, missing permanent-rejection headers, authentication/transient failures, archive commit failure, archive capacity, corrupt evidence, reboot between archive and pop, queue-pop failure, and short SD writes. They run the real CSV writer against a fake SD filesystem. These tests do not prove physical card power-loss behaviour or hard real-time acquisition deadlines; synchronous local filesystem/UI operations can still delay the loop.

Classic ESP32 links with one LTO partition to retain the existing 2 MiB OTA slots. The APE-101 local unprovisioned build produced a 2,080,896-byte application. Check the final `.bin` against both OTA slots for any credential-bearing or signed build; the ELF size estimate alone excludes image overhead. The NodeMCU compiler in this Mac installation is x86_64 and cannot run on the ARM host; Linux CI supplies that target check.
