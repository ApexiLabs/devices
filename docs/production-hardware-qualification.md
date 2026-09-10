# Production Hardware Qualification

This is the revision-controlled qualification plan and result index for the production logger. It separates firmware assumptions from facts demonstrated on a physical assembly.

> **Status: not qualified.** Blank or `TBD` fields are required operator inputs. The checked-in templates contain no implied component approval, calibration result, or environmental test result.

## Release gate

A hardware revision is production-qualified only when all of the following are attached to its release record:

- a completed hardware profile and as-built BOM with manufacturer part numbers and board revisions;
- an as-built pinout checked against [`include/PinDefinitions.h`](../include/PinDefinitions.h);
- calibration results for every fitted sensor channel at 4, 8, 12, 16, and 20 mA;
- power, RTC, reboot, network-outage/replay, thermal, vibration, and soak results;
- a completed manufacturing checklist for the exact assembly;
- firmware version, commit SHA, build environment, partition table, test-equipment identifiers, operator, and UTC timestamps;
- an approval decision naming the reviewer and any accepted deviations.

Copy the CSV templates under [`fixtures/qualification/`](../fixtures/qualification/) into a result directory named for the hardware revision and assembly serial. Do not overwrite the templates or commit credentials, Wi-Fi details, bearer tokens, or Cloudflare secrets in results.

## Hardware profile

Complete this table before electrical testing. Values shown as firmware assumptions still require physical verification.

| Field | Required value | Current firmware assumption | Qualification result |
| --- | --- | --- | --- |
| Hardware revision | Drawing/BOM revision | None | TBD |
| Assembly serial | Unique traceable serial | None | TBD |
| MCU manufacturer and exact board SKU | Manufacturer part number and board revision | Classic ESP32 DevKit/WROOM-compatible | TBD |
| ESP32 module marking | Exact can/module marking | None | TBD |
| Physical flash capacity | Read-back and tool used | 16 MB build target | TBD |
| PlatformIO environment | Exact environment | `logger-esp32-production-candidate` for acceptance | TBD |
| Partition table | File and checksum | `partitions/esp32-16mb-store-forward.csv` | TBD |
| ADC | Manufacturer, SKU, revision, I2C address | ADS1115-compatible, address `0x48` | TBD |
| Pressure receiver | Manufacturer, SKU, revision | DFRobot SEN0262-compatible | TBD |
| Temperature receiver | Manufacturer, SKU, revision | DFRobot SEN0262-compatible | TBD |
| RTC | Manufacturer, SKU, revision, address | RV-3028-C7-compatible, address `0x52` | TBD |
| RTC backup source | Chemistry/type, rated capacity, fitted state | None assumed | TBD |
| Removable storage | Socket/module and supported media | Optional FAT32 microSD | TBD |
| Onboard store-and-forward | Capacity and retention estimate | 10 MiB firmware cap | TBD |
| Input protection | Fuse, reverse polarity, transient suppression | Required, exact parts unapproved | TBD |
| DC/DC conversion | Manufacturer, SKU, input/output rating | Regulated 5 V rail required | TBD |
| Connectors and harness | Manufacturer, series, keying, wire gauge | None | TBD |
| Enclosure and mounting | Material, ingress rating, mounting method | None | TBD |
| Pressure transmitter | Manufacturer, SKU, range, supply, thread | 0–8 bar, 4–20 mA model configured | TBD |
| Temperature transmitter | Manufacturer, SKU, range, supply, thread | 0–150 °C, 4–20 mA model configured | TBD |

Record datasheet revisions or durable vendor documents with the result set. A retail listing alone is not sufficient evidence for voltage, loop-compliance, temperature, transient, or vibration ratings.

## As-built pinout

Verify continuity with power removed and record the actual harness colour and connector cavity. The GPIO column is the current firmware contract, not proof of as-built wiring.

| Signal | ESP32 GPIO | Destination | Harness colour/cavity | Verified by/date | Result |
| --- | ---: | --- | --- | --- | --- |
| 3.3 V | n/a | ADS1115 VDD and 3.3 V I2C pull-ups | TBD | TBD | TBD |
| Ground | n/a | ADC, receivers, RTC, storage and protected power common | TBD | TBD | TBD |
| I2C SDA | 21 | ADS1115 SDA + RTC SDA | TBD | TBD | TBD |
| I2C SCL | 22 | ADS1115 SCL + RTC SCL | TBD | TBD | TBD |
| SPI MOSI | 23 | Optional microSD/TFT MOSI | TBD | TBD | TBD |
| SPI MISO | 19 | Optional microSD/TFT MISO | TBD | TBD | TBD |
| SPI SCLK | 18 | Optional microSD/TFT SCLK | TBD | TBD | TBD |
| microSD CS | 5 | Optional microSD CS | TBD | TBD | TBD |
| TFT CS | 27 | Optional TFT CS | TBD | TBD | TBD |
| TFT DC | 26 | Optional TFT D/C | TBD | TBD | TBD |
| TFT reset | 25 | Optional TFT reset | TBD | TBD | TBD |
| UI button | 32 | Active-low service button | TBD | TBD | TBD |
| Status LED | 2 | Board LED if fitted | TBD | TBD | TBD |
| Pressure analog | ADS1115 A0 | Pressure receiver analog output | TBD | TBD | TBD |
| Temperature analog | ADS1115 A1 | Temperature receiver analog output | TBD | TBD | TBD |

Any deviation requires a matching firmware pin/configuration change and a fresh build plus qualification run. Do not infer the pinout of an unidentified board from its connector position.

## Calibration procedure

### Equipment and setup

Record the model, serial, calibration due date, stated accuracy, and range for the current source, DMM, reference pressure/temperature source, power supply, and ambient probe. Use equipment whose combined uncertainty is lower than the acceptance tolerance selected by the hardware owner.

1. Load the exact production-candidate artifact and record its SHA-256, firmware version, git SHA, and PlatformIO environment.
2. Warm the complete powered assembly for the operator-defined stabilization period and record supply voltage and ambient temperature.
3. Connect a traceable current source in place of the transmitter. Verify loop topology and receiver output range before connecting the ADS1115.
4. Apply 4, 8, 12, 16, and 20 mA in ascending order, then repeat in descending order to expose hysteresis.
5. At every point, allow the reading to settle, then record reference current, receiver voltage, ADC raw value, reported loop current, engineering value, expected engineering value, error, and pass/fail in `calibration-points-template.csv`.
6. Repeat for every channel and every assembly. Do not copy coefficients or results between receiver boards without evidence.
7. Repeat any failing point only after recording the original failure and the corrective action. Never replace a failed row silently.

Expected engineering values must be calculated from the configured mapping for the tested artifact. Acceptance tolerances are an explicit hardware-owner input; this document intentionally does not invent them.

## RTC and time acceptance

The production decision must state whether the RTC is fitted and whether a backup source is required.

- If fitted, verify the exact RTC identity/address, backup source, power-loss flag behavior, cold-start recovery, NTP write, hourly refresh, offline holdover, and return-to-network correction.
- If no backup source is fitted, record that the RTC cannot satisfy unpowered holdover and define the allowed boot behavior. Valid NTP time may supply timestamps while online, but it is not evidence of RTC retention.
- If the RTC is intentionally omitted, disable it in the qualified configuration and record the rationale and offline timestamp limitation.
- Measure drift over an operator-defined interval against the reference clock. Record start/end UTC, elapsed time, drift, temperature range, and acceptance tolerance.
- Reject timestamp regressions across reboot, network loss, NTP recovery, and RTC fallback. Transport timestamps remain UTC RFC 3339; local UI/CSV time follows the configured timezone.

## Electrical and environmental procedure

Use current-limited, instrumented test equipment and an approved automotive transient simulator where required. The operator must define voltage profiles and limits from the selected component ratings and the intended vehicle environment.

| Test | Required evidence | Pass/fail value to define before run |
| --- | --- | --- |
| Reverse polarity | Applied voltage/current limit, duration, observed current, post-test function | Maximum safe leakage/damage criteria |
| Undervoltage and brownout | Voltage ramp/dips, reset reason, queue integrity, timestamp behavior | Minimum operating voltage and allowed reset behavior |
| Cranking profile | Programmed waveform, rail captures, resets/sample loss | Vehicle-specific waveform and allowed loss |
| Load dump/transients | Standard/pulse, protection-node captures, post-test function | Applicable pulse levels and damage/reset criteria |
| Conducted/radiated noise | Injection method, sensor error, transport/RTC state | Allowed engineering-value error and faults |
| Ground offset | Applied offset/current, ADC error, communications state | Allowed offset and measurement error |
| Thermal | Chamber/fixture profile, internal rail and reading logs | Operating/storage range and drift limit |
| Vibration | Fixture, axes, frequency/amplitude/duration, connector inspection | Applicable profile and discontinuity limit |

Stop immediately on overheating, smoke, unexpected current, damaged insulation, unstable protection behavior, or an unsafe fixture condition. Record the stopped run as a failure; do not continue merely to complete the table.

## Reboot, outage, and soak procedure

1. Start with an empty, healthy onboard queue and a known server session. Record device ID without credentials, firmware SHA, initial boot/reset reason, queue counters, RTC state, and server time.
2. Perform cold-power, reset-button, software, and brownout reboots. For each, confirm boot identity/session behavior, sensor recovery, time monotonicity, and queue readability.
3. Block upstream connectivity while acquisition continues. Exercise outages shorter than, near, and longer than the calculated onboard retention window.
4. Restore connectivity and verify oldest-first replay, acknowledged removal, drop/corruption counters, no duplicate sequence within a source session, and expected server receipt count.
5. Run the complete assembly continuously for 24–48 hours using `soak-observations-template.csv`. Record periodic health snapshots and every state change or intervention.
6. Reconcile generated, queued, replayed, dropped, rejected, and server-accepted records. Any unexplained difference fails the run.

Before each test, define numeric limits for reset count, timestamp regression, acceptable sample loss, queue drops, sensor error, temperature, and supply rails. A run without predeclared limits is evidence gathering, not acceptance.

## Manufacturing and field checks

Use `manufacturing-checklist-template.csv` for each assembly. At minimum it must capture:

- assembly serial, hardware revision, inspector, UTC timestamp, and firmware artifact checksum;
- visual inspection, polarity/keying, fuse, earth/ground, strain relief, fastener and connector checks;
- powered rail measurements and current consumption;
- immutable device identity and provisioning result without secret values;
- I2C detection, ADC/RTC status, sensor open/short fault behavior, button/status LED, optional SD, local diagnostics, live upload, queue/replay, and NTP/timezone checks;
- calibration record identifier and final disposition.

Field diagnostics should begin with power/ground and connector inspection, then local diagnostics, ADC/RTC state, current-loop measurement, network/server status, and queue counters. Factory reset or queue erase are destructive recovery actions and require evidence capture first.

## Qualification decision

| Decision field | Value |
| --- | --- |
| Hardware revision | TBD |
| Qualified assembly serials | TBD |
| Firmware artifact/version/SHA | TBD |
| Result directory or release attachment | TBD |
| Deviations accepted | TBD |
| Reviewer | TBD |
| Review date (UTC) | TBD |
| Decision | **NOT QUALIFIED** |
