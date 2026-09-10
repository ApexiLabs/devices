# Periodic logger status diagnostics (APE-83)

Schema version 1 status payloads add an optional top-level `system` object on both MQTT and HTTPS. Existing fields and live sample payloads retain their meanings. Status is emitted every 30 seconds while live upload is enabled, independently of sample recording, sensor validity, or microSD. Upload disabled or networking unavailable prevents delivery; the receiver must use heartbeat expiry, not infer healthy diagnostics from silence.

Every field below is optional. Missing means unavailable or not observed; it must never be converted to zero or false. Numeric values are nonnegative unsigned 32-bit integers and booleans are JSON booleans. Explicit zero and false are valid observations. The gateway accepts these signals only after durable TSDB write and expires each signal after 90 seconds.

| Field under `system` | Meaning and lifecycle |
| --- | --- |
| `store_forward_pending_records` | Current unread queue record count after successful mount/scan. Omitted when disabled, unsupported, or unmounted. Recovered segments can establish this gauge even if metadata was lost. |
| `store_forward_dropped_records` | Cumulative capacity-rotation and permanent-rejection drops committed by the queue. Retained in queue metadata across boots; saturates at 4,294,967,295. Omitted when the queue is unavailable or historical metadata was lost. Recovery persists an uncertainty flag, so a later boot cannot turn an unknown historical count into observed zero. Full queue erase starts a new counter history. |
| `clock_fault` | True when the enabled RTC's last observed initialization/NTP-write readiness failed, or the timestamp source cannot emit UTC RFC 3339 transport time. False means both checks pass (an intentionally disabled RTC does not itself cause a fault). Updated from the same clock state used to build readings, before normal periodic status. Omitted until the first clock observation. It does not certify drift, backup battery, or hardware health between RTC accesses. |
| `config_stale` | Highest desired configuration version observed for this authenticated device exceeds the successfully applied version. True remains until that version or a newer one is applied; an older replay cannot clear it. Omitted before a desired version has been observed in the current boot. This is unapplied state, not an elapsed-time threshold. |
| `config_rejected` | True after a received desired document fails validation or local settings persistence. False after a valid desired document is accepted or configuration is applied. Omitted until one of those events. An identity-invalid/malformed request can establish rejection while leaving desired-version knowledge absent. Credential/assignment validation inside a desired document also counts as rejection; subsequent credential proof transport errors are not configuration rejection. |
| `reconnects_total` | Transport recovery attempts in this boot, excluding the initial connection attempt. MQTT counts subsequent connect calls. HTTPS counts attempts following a failed HTTP request (including rejection) or observed Wi-Fi loss; healthy requests and healthy TCP/TLS connection reuse/recreation do not increment it. Failed recovery retries each increment; success ends recovery. Saturates at 4,294,967,295 and resets with the per-boot source session. Omitted until a first transport attempt is observed. |
| `reboots_total` | Durably recorded completed setup runs, including the first as 1, in ESP32 NVS namespace `apexi_diag`, key `boots`. One atomic commit per completed setup; no heartbeat writes. Retained across normal restart, firmware upgrade, and owner reset. Omitted on ESP8266 or if NVS open/read/write cannot establish a trustworthy count. An incomplete boot or failed commit is not counted. Full NVS erase starts a new history. Saturates at 4,294,967,295. This detects repeated completed boots, not failures before setup completes. |

Example after an observed configuration has applied and a queue is mounted:

```json
{
  "system": {
    "store_forward_pending_records": 0,
    "store_forward_dropped_records": 0,
    "clock_fault": false,
    "config_stale": false,
    "config_rejected": false,
    "reconnects_total": 2,
    "reboots_total": 5
  }
}
```

The diagnostics object accepts no free text or credential inputs. It contributes fewer than 320 bytes at maximum counter values. The MQTT packet buffer is 3072 bytes to accommodate this additive status payload. MQTT last-will diagnostics are a snapshot from connection setup, not a fresh observation; consumers must also inspect `connected=false` and avoid using retained offline messages as evidence of current health.

## Verification and release evidence

`./scripts/verify-repo.sh --fast` runs the host diagnostics tests for ESP32 and ESP8266. They cover absence versus observed zero/false, pending/rejected/applied configuration, old desired replay, recovery attempts, reboot persistence/failure, saturation, and diagnostics JSON size. Queue tests cover missing mount and metadata-loss uncertainty through another reboot. Full builds cover both supported MCU families and the production-candidate environment.

Physical acceptance remains open: capture status through actual Cloudflare Access and gateway/TSDB, interrupt/recover the network, force a rejected and then applied configuration, cold-boot twice, interrupt NVS boot-counter commit, induce RTC/time failures, and exercise queue retention/drops. Verify the exact firmware/app/gateway revisions and that signals clear or expire according to the receiver contract. Do not use passing host tests as proof of flash endurance, battery holdover, network timings, or completed production qualification.
