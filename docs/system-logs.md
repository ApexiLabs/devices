# System logs and remote downloads

Sensor CSV logging is unchanged. System events use newline-delimited JSON in
`.log` files: readable structured records, not a syslog network service.

## Storage and local viewing

The Logger saves `logger-<date>-<segment>.log`; Dash events go to
`dash-<BLE-address>-<date>-<segment>.log`. Segments rotate at approximately
256 KiB, with 16 retained per source (approximately 4 MiB). Only recognised
system-log segments are pruned, never sensor CSVs. Logs share the existing
microSD mount; no automatic formatting or internal-flash fallback is introduced.

Open **Diagnostics → System logs** (`/logs`) with the settings credential.
The combined recent view filters by source and severity, exposes pending/dropped
counts, and offers `.log` and sensor `.csv` downloads. New log endpoints require
the existing settings authentication.

Records include device, boot ID, sequence, source uptime, logger receipt time,
severity, fixed event code and numeric value. Receipt time uses UTC network time
when available, otherwise an explicit uptime fallback. Delayed Dash delivery is
not presented as the event's occurrence time: boot/sequence and source uptime
remain authoritative. No credentials, pairing codes, bodies, headers or arbitrary
request paths/query strings are recorded.

Logger events include boot, sensor fault/recovery, ADC/RTC/SD readiness,
Wi-Fi/Dash/upstream transitions, OTA and HTTP activity. Logger HTTP responses
include status, method enum and handling duration. Successful API polling is
summarised once per minute. The local viewer can enable detailed HTTP logging
for ten minutes; repeated requests are otherwise coalesced. Dash events include boot, sensor freshness/fault
transitions, BLE/Wi-Fi changes, OTA and web activity. Existing serial output is
separate from these logs.

## Dash forwarding

Optional BLE characteristic `8f771003-6d7a-4f48-9f8a-67a8c14b6c01` serves
version-1 fixed 60-byte little-endian event records in four 20-byte notifications,
compatible with the default BLE MTU. The Logger consumes at most one per second
and writes a non-blocking 8-byte boot/sequence acknowledgement after SD flush.
Missing/reordered fragments are discarded and retransmitted. No synchronous
BLE characteristic read is added to the sampling loop.
Retries are deduplicated; the last durable cursor is recovered from log tails
after Logger reboot. Older Dash firmware without this characteristic remains
compatible with sensor transport.

Each device has a bounded 64-event RAM queue. Overflow drops new events and
increments a visible counter. Unsent Dash events and unflushed Logger events can
be lost on power loss. This does not claim power-loss-safe SD hardware or repair
of torn final lines. BLE retains its existing trusted-nearby-device assumptions.

## Remote retrieval

ESP32 HTTPS loggers use a separate FreeRTOS worker and outbound
`<configured ingest path>/logs` requests. Requests use the configured scoped app
bearer, Cloudflare Access headers and verified TLS, without redirects or inbound
ports. ESP8266 and MQTT-only remote downloads are not supported.

Only the Arduino loop accesses SD. Idle polling is every 15 seconds; active
chunks are no faster than 500 ms, retries after failure no faster than 5 seconds.
Each chunk has at most 2 KiB of file data, hex encoded. The initial file length
defines the snapshot; subsequent appends are excluded. Source files are retained.
Unreadable, truncated or oversized snapshots fail; the maximum is 16 MiB.

Both local and app remote-management switches must be enabled. The app checks
ownership and permissions on every operation. Disabling management stops later
steps; an already in-flight request can finish before the change is observed.
The app owns job expiry and private artifact retention; see its
`docs/logger-logs.md`.

## Acceptance

Run host tests and build `logger-tinyc6` and `dash-waveshare-s3-128`. Deploy the
app before firmware; update both boards for Dash forwarding. Physical acceptance
requires disconnect/reconnect, SD failure, power-loss and large-download cadence
tests. A successful build does not establish these hardware properties.
