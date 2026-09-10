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

Only strict `status: ok`, boolean `accepted: true`, HTTP 200 removes a queued record. The submitted payload must still match the queue head before pop, so segment rotation during an in-flight request cannot acknowledge a different record. Ambiguous responses retain data for idempotent retry. The existing queue format, checksum and capacity/segment-overflow policy are unchanged. Overflow can still drop records if sustained delivery remains below capture; these drops must remain visible. Filesystem operations still run on the main loop and are not claimed to be hard real-time.

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
