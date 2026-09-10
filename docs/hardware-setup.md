# Hardware Setup

Logger web pages use the app's triangular ApexiLabs logo and `ApexiLabs Logger` branding. No hardware changes are required. Inter loads from Google Fonts when reachable; offline devices use system fonts and retain the embedded SVG logo. Dashboard/Diagnostics time, timezone and uptime remain live device data.

The dashboard fault summary treats up to two queued upload records as normal in-flight work when the server is connected, queue storage is ready, and an upload was acknowledged within ten seconds. This suppresses only the informational replay message; real HTTP/storage errors, stale acknowledgements and larger backlogs remain visible. Full diagnostics retain the queue count and replay message.

Dashboard CSV file sizes use decimal megabytes (1 MB = 1,000,000 bytes), rounded
to two decimal places. The file API continues to return exact byte counts.

System logs share the existing microSD wiring and require no additional pins.
Update both Logger and Dash firmware for Dash event forwarding; remote retrieval
requires ESP32, HTTPS and management enabled locally and in the app. See
[system-log storage, retention and acceptance](system-logs.md).

Logger `/diagnostics` includes an Apexi Dash card with Bluetooth connected/disconnected/disabled state and the current discovery or connection status. The upstream endpoint display shows only the hostname (not the port or ingest path); this does not change the configured destination. `/api/live` adds `dash_enabled`, `dash_connected`, and `dash_status` for the local UI.

## Core modules
- MCU: NodeMCU 1.0 / ESP-12E DevKit V2 (`logger-nodemcuv2`), classic ESP32 DevKit / ESP32-WROOM-32 (`logger-esp32`), or Unexpected Maker TinyC6 (`logger-tinyc6`)
- ADC: ADS1115 on I2C, using the Adafruit ADA1085 board
- Sensor interface: 2x DFRobot SEN0262 current-to-voltage modules
- Power: DFRobot DFR1015 buck converter for the regulated rail
- UI button: DFRobot DFR0029-W digital push button
- RTC: RV-3028-C7 on the Unexpected Maker RTC Logger Shield, sharing the primary I2C bus with the ADS1115
- Storage: microSD slot on the Unexpected Maker RTC Logger Shield, using FAT32 media
- Dash: Waveshare ESP32-S3-Touch-LCD-1.28, running its own `dash-waveshare-s3-128` firmware

## Recommended wiring

### Power front end
- Vehicle 12 V input -> fused lead or fuse holder -> off-the-shelf reverse-polarity/transient protection module -> buck converter module to 5 V
- 5 V rail -> NodeMCU `VIN` input and any peripheral explicitly rated for 5 V power; do not back-feed a board-dependent `VU`/USB rail
- NodeMCU `3V3` -> ADS1115 VDD and all ESP8266-side I2C/SPI logic; do not pull an ESP8266 GPIO up to 5 V
- Power the ADS1115 at 3.3 V. The SEN0262's 0-3 V output remains inside the ADC supply range while its I2C pull-ups remain safe for the ESP8266
- The selected field sensors should be ordered for direct operation from the protected 12 V rail; confirm the vendor's supported voltage range and 4-20 mA loop compliance rather than relying on a nominal "12 V" label
- If using the `DFR1015`, set and verify the 5 V output before connecting the NodeMCU and peripherals
- For a 24 V loop-powered sensor, the preferred branch is fused 12 V -> [Pololu 5380 reverse-voltage protector](https://core-electronics.com.au/pololu-reverse-voltage-protector-4-60v-10a.html) -> [Pololu U3V9F24 step-up regulator (item 5588)](https://core-electronics.com.au/catalog/product/view/sku/POLOLU-5588) -> sensor supply

### Current loop receivers
- Use one off-the-shelf 4-20 mA receiver/current-to-voltage module per sensor channel
- Wire each transmitter loop into its receiver module according to that module's datasheet
- Feed each module's analog voltage output into the ADS1115 input channel configured for that sensor
- Keep the module output range within the ADS1115 input range selected by the firmware
- Prefer receiver modules that already include the current sense, filtering, and input protection stages

### Sensor loop topology
- Protected 12 V -> sensor `+`
- Sensor loop output/current return -> 4-20 mA receiver module input
- Receiver module analog output -> ADS1115 A0 for oil pressure
- Second receiver module analog output -> ADS1115 A1 for oil temperature
- ADS1115 GND -> system ground

### Shared buses
- I2C bus:
  - NodeMCU `D2` / GPIO 4 -> ADS1115 SDA + optional RTC SDA
  - NodeMCU `D1` / GPIO 5 -> ADS1115 SCL + optional RTC SCL
- SPI bus:
  - NodeMCU `D7` / GPIO 13 -> optional TFT + microSD MOSI
  - NodeMCU `D6` / GPIO 12 -> optional TFT + microSD MISO
  - NodeMCU `D5` / GPIO 14 -> optional TFT + microSD SCLK
  - Separate chip select lines for TFT and SD when those peripherals are fitted

## Field sensor ordering specification

The field transmitters are external inputs to the logger rather than part of the core electronics BOM. The selected configuration keeps the industrial 4-20 mA interface while ordering both transmitters for direct operation from the protected 12 V vehicle supply.

| Qty | Sensor | Required configuration |
| --- | --- | --- |
| 1 | Oil pressure transmitter | 0-0.8 MPa (0-8 bar), 1/8-inch NPT male, 4-20 mA output, protected 12 V supply compatible, 5 m cable |
| 1 | Oil temperature transmitter | 0-150 degrees Celsius, 1/8-inch NPT male, 4-20 mA output, protected 12 V supply compatible, 5 m cable, dimensional limit below |

Temperature probe dimensional limit:

- Maximum total insertion length is **23.5 mm**, measured from the probe tip to the mounting shoulder and including the threaded section.
- Allocate **15 mm** to the threaded section, leaving no more than **8.5 mm** of unthreaded probe beyond the threads.
- The rejected existing 24 V temperature transmitter measures 45 mm total: 35 mm unthreaded probe plus 10 mm of thread. Do not reorder that geometry for the replacement 12 V, 4-20 mA transmitter.
- Require a vendor dimensioned drawing or written confirmation that both the total 23.5 mm limit and the 8.5 mm exposed-probe limit are met. A bare "probe length" value is ambiguous and is not sufficient for approval.
- Confirm the transmitter can drive the selected 4-20 mA receiver at 20 mA across the vendor's full stated 12 V operating range before ordering. If it cannot, use the documented 24 V boost branch instead.

## Default NodeMCU pin map

Use the board's printed `D` label when wiring. The firmware stores the corresponding raw GPIO number.

| Function | NodeMCU label | GPIO | Default use |
| --- | --- | --- | --- |
| I2C SDA | D2 | GPIO 4 | ADS1115 SDA; required |
| I2C SCL | D1 | GPIO 5 | ADS1115 SCL; required |
| SPI MOSI | D7 | GPIO 13 | Optional TFT/microSD |
| SPI MISO | D6 | GPIO 12 | Optional TFT/microSD |
| SPI SCLK | D5 | GPIO 14 | Optional TFT/microSD |
| TFT CS | D8 | GPIO 15 | Optional; must remain low during boot |
| TFT DC | D3 | GPIO 0 | Optional; must remain high during boot |
| TFT RST | D4 | GPIO 2 | Optional; must remain high during boot |
| Built-in status LED | D4 | GPIO 2 | Active low; steady on after firmware setup confirms MCU power/running state |
| TFT BL | Supply | n/a | Hard-wire to the display's rated supply; no GPIO default |
| SD CS | D0 | GPIO 16 | Optional microSD chip select |
| UI button | RX | GPIO 3 | Optional active-low button; serial diagnostics use TX only |

For the current two-sensor build, both receiver signals terminate at the ADS1115, so only power, ground, D1, and D2 are required between the NodeMCU and ADS1115. D3, D4, and D8 are ESP8266 boot-strapping pins; never attach a peripheral that drives them to the wrong level during reset. The built-in LED and optional TFT reset currently share D4, so remap `PIN_TFT_RST` before enabling the TFT. Update [`include/PinDefinitions.h`](../include/PinDefinitions.h) and re-verify the boot state if the optional pin assignment changes.

Update the values in [`include/PinDefinitions.h`](../include/PinDefinitions.h) if the actual wiring differs.

## Classic ESP32 DevKit pin map

Use the raw GPIO numbers printed on a classic ESP32 DevKit/WROOM-class board.

| Function | ESP32 GPIO | Default use |
| --- | --- | --- |
| I2C SDA | GPIO 21 | ADS1115 SDA + RTC SDA |
| I2C SCL | GPIO 22 | ADS1115 SCL + RTC SCL |
| SPI MOSI | GPIO 23 | Optional TFT + microSD MOSI |
| SPI MISO | GPIO 19 | Optional TFT + microSD MISO |
| SPI SCLK | GPIO 18 | Optional TFT + microSD SCLK |
| microSD CS | GPIO 5 | RTC Logger Shield microSD CS |
| TFT CS | GPIO 27 | Optional TFT chip select |
| TFT DC | GPIO 26 | Optional TFT data/command |
| TFT RST | GPIO 25 | Optional TFT reset |
| UI button | GPIO 32 | Optional active-low button |
| Built-in status LED | GPIO 2 | Common DevKit LED assignment; active high |

The sensor receiver boards still connect to ADS1115 A0 and A1 rather than to an ESP32 ADC pin. Power the ADS1115 from the ESP32 `3V3` pin so its I2C pull-ups remain at 3.3 V. Some DevKit variants omit the GPIO 2 LED; that does not affect logging.

The detected ESP32 target has 16 MB flash. Its firmware reserves dual 2 MB OTA slots and an approximately 12 MB LittleFS partition, of which at most 10 MB is used for the circular HTTPS store-and-forward queue. This makes microSD optional for transient outage recovery, and SD logging is disabled by default for the ESP32 target. Fit microSD and enable the feature only when long-duration CSV archives or removable media are required; the onboard queue is not exposed as a user filesystem and automatically acknowledges replayed records.

The current `logger-esp32` environment is headless and uses the separate BLE dash. This removes the legacy TFT driver from the logger image. The legacy directly wired TFT remains available only to older configurations that deliberately restore its build dependency and remove `MDA_HEADLESS_DISPLAY`.

Do not flash the current classic ESP32 image without checking its final binary size: the 2026-09-08 local build produced a 2,097,264-byte `firmware.bin`, exceeding its 2,097,152-byte OTA slot by 112 bytes even though PlatformIO's ELF size check passed. This target needs an image-size reduction or a separately planned partition migration before flashing. The TinyC6 and Waveshare Dash targets use different partition layouts and are not affected by this limit.

## TinyC6 pin map

Battery diagnostics: a conventional single-cell 4.2V LiPo on the stacked shield/VBAT is monitored through GPIO4. The [TinyC6 P1 schematic](https://github.com/UnexpectedMaker/esp32c6/blob/main/TinyC6/TinyC6_Schematic_P1.pdf) specifies R6=442k and R7=160k (3.7625 multiplier). GPIO10 detects USB/5V power. Eight calibrated ADC samples are averaged once per second and smoothed. The resting-voltage percentage is approximate, affected by charging, load, temperature and chemistry. Readings outside 2.5–4.35V are unavailable. A disconnected battery can still produce a plausible charger voltage; this is not a battery-presence detector.

The two-minute trend has a 30mV deadband and resets on power changes or sample gaps over 15 seconds. External power plus rising voltage means "likely charging"; no external power means "discharging (inferred)" under normal board wiring. Stable voltage on external power means unknown charge state, never confirmed charge completion. No current or charger-status sensor is read. Diagnostics and additive `battery_*` / `external_power` API fields expose the readings; unsupported boards report unavailable. CSV and BLE sensor contracts are unchanged.

Battery bench verification (2026-09-08): application-only OTA to `10.0.40.177` succeeded. At uptime 2m27s the API reported 4.084V, estimated 85%, external power present, and a steady trend with unknown charge state; SD and Dash remained ready. The diagnostics UI was checked live. USB-removal/discharge behaviour has not been physically tested. Host tests cover the estimate curve, invalid readings, trend transitions, power changes and timer rollover.

Battery calibration (2026-09-09): the user measured 4.160V at the battery while diagnostics showed 4.050V. `APEXI_BATTERY_VOLTAGE_GAIN` defaults to `4.160f / 4.050f` (1.02716049) for this bench TinyC6. It is applied after the physical divider conversion and before smoothing, validity checks, percentage and trend calculation. Override it in local `AppSecrets.h` or build flags for another board; use `1.0f` for no correction. This one-point calibration corrects the observed gain error, not ADC nonlinearity or charger behaviour. Verify again at a lower battery voltage before assuming accuracy across the discharge range. Application-only OTA was verified on the bench logger, which subsequently reported 4.152V; this is not a second simultaneous multimeter comparison.

SD bench verification (2026-09-08, Logger `10.0.40.177`): after application-only OTA with SD enabled and CS corrected to GPIO18, the stacked shield mounted successfully. `/logs-20260908.csv` grew from 6,237 to 22,158 bytes; HTTP readback returned 361 data rows with consistent eight-column headers/rows and valid pressure/temperature samples. ADC, RTC and Dash link reported ready. This verifies live write/readback, not power-loss durability; upstream HTTP 401 and unavailable onboard queue remain separate faults.

The `logger-tinyc6` environment uses the Unexpected Maker board definition and native USB CDC/JTAG. Its default assignments are:

| Function | TinyC6 GPIO | Default use |
| --- | --- | --- |
| I2C SDA | GPIO 6 | ADS1115 SDA + RTC SDA |
| I2C SCL | GPIO 7 | ADS1115 SCL + RTC SCL |
| SPI MOSI | GPIO 21 | Optional TFT + microSD MOSI |
| SPI MISO | GPIO 20 | Optional TFT + microSD MISO |
| SPI SCLK | GPIO 19 | Optional TFT + microSD SCLK |
| microSD CS | GPIO 18 | Stacked RTC Logger Shield; enabled for TinyC6 |
| TFT CS | GPIO 18 | Optional TFT chip select |
| TFT DC | GPIO 8 | Optional TFT data/command |
| TFT RST | GPIO 9 | Optional TFT reset |
| UI button | GPIO 5 | Optional active-low button |
| Status LED | Board RGB LED | Firmware-running indication |

The TinyC6 has 8 MB flash and no PSRAM. Its default OTA partition table leaves a 1.5 MB LittleFS partition; after the firmware's filesystem reserve, approximately 1 MB is available to the circular store-and-forward queue. TinyC6 SD logging is enabled for the directly stacked RTC Logger Shield, using GPIO18 for CS and the default SPI pins above. GPIO10 is VBUS sense, not SD CS. The optional legacy TFT CS also uses GPIO18, so do not enable that display alongside this shield without remapping it. Classic ESP32 SD logging remains disabled by default. Native USB serial requires the `ARDUINO_USB_MODE=1` and `ARDUINO_USB_CDC_ON_BOOT=1` build flags already present in the `logger-tinyc6` environment.

The `logger-tinyc6` environment is currently headless. TFT_eSPI 2.5.x does not support the ESP32-C6 register interface, so this target omits the optional local TFT dashboard while retaining the web dashboard, sensor acquisition, local store-and-forward logging, live upload, and OTA services. The TFT pins above are reserved for a future C6-compatible display driver.

## Waveshare ESP32-S3 dash

The dash is a separate computer, not an SPI peripheral wired to the logger. It uses the onboard 1.28-inch, 240x240 GC9A01A display and connects wirelessly to ESP32-family logger firmware over BLE. The logger is the BLE central; the dash advertises as the peripheral. This keeps connection recovery under the logger's control and leaves the dash Wi-Fi radio available for its own web UI.

At logger boot it scans immediately, connects to the advertised Apexi dash service, and writes `mda-logger/1`. If discovery, connection, service lookup, or handshake fails, another attempt starts after `AppConfig::kDashLink.retryIntervalMs` (5 seconds by default). The scan window is 2 seconds. ESP8266 builds remain supported but cannot use this link because the hardware has no BLE radio.

After the handshake, Logger writes version-1 telemetry to the additive characteristic `8f771002-6d7a-4f48-9f8a-67a8c14b6c01`. `include/DashTelemetry.h` owns its 20-byte wire format, catalog decoding, and freshness rules. Repeated ID/name/units/sample frames fit the default BLE MTU; metadata is repeated so a dropped frame can recover. The supported catalog contains up to eight sensors. Stable IDs must fit 15 ASCII characters; display names and units are shortened to 15 characters. Logger targets one complete cycle every 250 ms, but actual delivery is limited by its loop, BLE scheduling, and other work. Frames carry filtered values, validity, active faults, and threshold warnings. A handshake-only peer stays compatible but cannot supply readings.

The LCD has two configurable slots, automatic first/second sensor selection by default. Open `/settings` on Dash and authenticate as `admin` with the OTA password to choose either sensor, hide a slot, or select automatic mode. Select a refresh interval from 250 to 5000 ms in 250 ms steps (default 1000 ms). This controls LCD redraw and web polling, not Logger acquisition or telemetry production. The interval and effective requested frequency are shown on the status page. Settings are stored in Dash NVS; saved IDs remain selected if a sensor disappears instead of silently switching to another measurement.

For one large centred reading, set either slot to **Hidden**. With both slots active, readings are stacked using 48-pixel digits. Single-reading mode uses up to 96-pixel digits, shrinking longer values to fit. Labels, units, and fault explanations use smaller text; device branding, IP address, and refresh timing are omitted from the sensor screen to preserve space. Both layouts keep the black background. `include/DashDisplayLayout.h` owns their geometry.

The LCD is rendered into a single 240×240, 8-bit off-screen sprite (about 57.6 KB) before transfer. The live panel is not cleared during periodic redraws, and identical visible frames are not transferred at all. `lcdBuffered` and `lcdFrameCount` in `/api/status` expose buffer allocation and actual frame pushes; `lcdValuesShown` reports the configured active slot count. If allocation fails, the panel shows a static error while Wi-Fi/OTA remain available. The refresh setting is an upper update cadence, not a requirement to rewrite unchanged pixels. This avoids erase/redraw flashing; it is not hardware tear-synchronised scanout.

Buffered-layout verification (2026-09-08): Dash OTA succeeded, `lcdBuffered` was true, and its frame counter remained 5 across three 1.1-second observation windows with unchanged readings at a 500 ms refresh interval. Both first-slot-only and second-slot-only configurations reported one active reading; the original two-sensor selection and 500 ms setting were restored. Host layout checks verify the single-value centre/scale and non-overlapping two-value geometry. Physical flicker and glyph appearance still require visual confirmation on the device.

Settings writes use authenticated `POST /api/settings`, a per-boot CSRF token from authenticated `GET /api/settings`, validation, and checked NVS persistence. Empty OTA passwords disable settings writes. The live status endpoint adds `refreshMs`, `slots`, `settingsWritable`, `telemetryIntervalMs`, and `sensors` (ID/name/units/value/fresh/valid/fault/warning). Data older than three seconds or from a disconnected Logger is unavailable; invalid/faulted values are JSON `null` and LCD `--`, not misleading zero readings. With no ADC on the bench, expect `adc_unavailable` rather than numeric measurements.

Sensor-display verification (2026-09-08): both Dash and the TinyC6 Logger at `10.0.40.177` were application-only OTA updated. Dash received `oil_pressure` / `bar` and `oil_temperature` / `C` with fresh frames and the expected `adc_unavailable` faults. Numeric decoding/rendering, malformed frames, stale data, timer rollover, and refresh limits passed host tests; real numeric sensor readings remain unverified because the bench ADC is absent. Unauthenticated settings returned 401; invalid CSRF tokens, intervals, and IDs returned 400. Explicit Oil Pressure / Oil Temp selections and 500 ms refresh survived a second Dash OTA/reboot, and telemetry resumed after reconnection. Desktop and 390-pixel web layouts were checked. The LCD's physical appearance was not camera-verified.

The onboard display wiring is fixed by the Waveshare PCB and is encoded in [`include/WaveshareDashTFTSetup.h`](../include/WaveshareDashTFTSetup.h):

The setup explicitly selects `USE_FSPI_PORT` for TFT_eSPI 2.5.x with Arduino ESP32 3.x. Without it, the S3 driver uses the Arduino FSPI identifier as a hardware register index and crashes during LCD initialization. Dash diagnostics use UART0 (`Serial0`) through the board's USB serial adapter at 115200 baud, not native USB CDC.

| Function | ESP32-S3 GPIO |
| --- | --- |
| LCD backlight | GPIO 2 |
| LCD DC | GPIO 8 |
| LCD CS | GPIO 9 |
| LCD clock | GPIO 10 |
| LCD MOSI | GPIO 11 |
| LCD MISO | GPIO 12 |
| LCD reset | GPIO 14 |
| Touch interrupt | GPIO 5 |
| Touch SDA / SCL | GPIO 6 / GPIO 7 |
| Touch reset | GPIO 13 |

Touch and the onboard IMU are intentionally not enabled in this first connection milestone. The dash creates a WPA2 access point named `APEXI-DASH` with password `apexi-dash`; its status page and JSON endpoint are available at `http://192.168.4.1/` and `/api/status`. This is a commissioning surface, not yet a complete settings UI.

Dash also joins the station network configured by `APEXI_WIFI_STATION_SSID` and `APEXI_WIFI_STATION_PASSWORD` in the ignored `include/AppSecrets.h`, using the same credentials as Logger. It requests DHCP with hostname `apexi-dash`, starts connecting without blocking the BLE/UI loop, and retries every 30 seconds while disconnected. The retry policy lives in `include/DashWifiPolicy.h`. Empty station credentials leave only the recovery AP active. Never commit credentials or distribute locally built credential-bearing images publicly.

Test Dash on the IoT network before changing or flashing Logger:

1. Connect the Waveshare board over USB and verify the selected serial port belongs to it before uploading.
2. Build and upload Dash using the commands below, then monitor serial at 115200 baud. Look for `DASH_WIFI=connected` and `DASH_STATION_WEB=http://<DHCP address>`. The LCD footer also shows the station IP.
3. From a client permitted to access the IoT network, request `http://<DHCP address>/api/status`. Confirm `device` is `APEXI-DASH`, `wifiConnected` is `true`, and `stationIp` matches the DHCP address. `apIp` separately identifies the recovery AP. A successful build alone is not a network test.
4. Power-cycle Dash and repeat the check. If association succeeds but HTTP cannot be reached, check IoT client isolation and firewall rules before attempting OTA.

BLE `CONNECTED OK` in the web UI remains a Logger handshake indicator, not a Wi-Fi or sensor-health indicator. After connection, the LCD shows the chosen readings on a black background. The web page uses Logger's theme and polls at the configured interval; unavailable readings are cleared when requests fail.

Dash supports password-protected ArduinoOTA with hostname `apexi-dash` and port 3232. It uses `APEXI_OTA_PASSWORD` from the ignored secrets header, remains disabled when that password is empty, and starts when station Wi-Fi connects. Status exposes `otaEnabled`, `otaReady`, `build`, and `uptimeSeconds`, never the password. The LCD shows an update notice on a black background during transfer. Install the first OTA-capable image over USB, then build the Dash target and run:

```sh
./.venv/bin/python scripts/upload-dash-ota.py 10.0.40.183
```

Use the current station IP if DHCP changes it. The helper sends only the prebuilt application image, checks it fits the generated OTA slots, checks the target identifies as an OTA-ready Dash, and reads the password without placing it in the process command line. This helper is for Dash devices commissioned with this repo's partition layout; do not use it for partition migration. Do not run concurrent PlatformIO builds against the same build directory, since other environments' artifacts may be cleaned. OTA is password authenticated, not an encrypted firmware transport; use only a trusted network. The host must allow the device's TCP callback for the transfer. There is no unauthenticated browser upload endpoint.

Hardware verification (2026-09-08): flashed the Waveshare Dash over USB with hash verification, reset it, and observed `Apexi Dash ready` followed by `DASH_WIFI=connected`. A request from the local Mac to its DHCP address returned `device: APEXI-DASH`, `wifiConnected: true`, and both BLE state fields false (Logger was not connected). The observed station address was `10.0.40.183`; DHCP may change it. Logger firmware and OTA were not changed or tested in this step.

Subsequent OTA/UI verification on the same date: installed the OTA-capable image over USB, then successfully uploaded the 1,291,840-byte application over Wi-Fi using PBKDF2-HMAC-SHA256 authentication. After reboot, `/api/status` reported Wi-Fi and OTA ready, and both `bleConnected` and `loggerReady` true with the separately updated Logger. The Logger-themed web page refreshed live status successfully; desktop and 390-pixel layouts were inspected with no horizontal overflow at phone width. The LCD renderer now uses `TFT_BLACK` for normal and update screens; its physical appearance was not camera-verified.

Build and upload the dash independently:

```sh
pio run -e dash-waveshare-s3-128
pio run -e dash-waveshare-s3-128 -t upload --upload-port /dev/cu.usbmodem1101
```

The generated dash update image is `.pio/build/dash-waveshare-s3-128/firmware.bin`; the combined bootloader, partition-table, and application image for a first flash is `.pio/build/dash-waveshare-s3-128/firmware.factory.bin`. Keep both boards powered during commissioning. A successful handshake changes the round LCD from `WAITING` to its sensor view; unplugging or resetting the logger returns it to `WAITING` and restarts BLE advertising.

This commissioning milestone does not enable BLE bonding or application-layer authentication. Do not treat the handshake as a trusted vehicle-control channel. Add pairing, authorization, and command validation before the dash can change logger settings or receive sensitive data.

### Dash troubleshooting log

Logging is always enabled on Dash without flash writes. `GET /api/diagnostics` (also linked under Dash Link in the web UI) returns a bounded JSON snapshot: boot ID, uptime, received/accepted/rejected BLE writes, connection/disconnection counts, per-index sample counts, current sample ages, maximum inter-sample gaps, metadata masks and faults, free heap, Wi-Fi RSSI, maximum main-loop/web-handler duration, and the latest 64 observed sensor-state transitions. Transitions also appear on UART0 at 115200 baud as `DASH_DIAG`. Counters and history reset at reboot/OTA; save the download first. Indices refer to the current sensor catalog, not permanent IDs. Maximum gaps include disconnections. Events are observed by the main loop, so transitions entirely within a blocked loop may be missed; callback receive counters continue independently.

- `stale` with sample gaps at least 3000 ms indicates missing timely BLE sample writes at Dash, not necessarily a failed physical sensor. Logger scheduling, transmission and radio delivery remain possible causes.
- Increasing disconnect counts indicate BLE link interruption. Rejected writes indicate bad-length/protocol/pre-handshake traffic.
- `sensor_fault` with small sample ages means Logger is delivering readings marked invalid or faulted.
- Large web/loop maxima suggest Dash servicing delays. Compare behaviour with Live LCD paused to investigate preview overhead; maxima are lifetime values, not exact event correlations.
- `renderClockRaces` counts a received timestamp newer than the time captured at the start of LCD rendering. This can produce an unsigned-age false stale indication; the counter measures the suspected race without changing freshness behaviour.

This endpoint has the same read-only trusted-LAN boundary as status and does not expose credentials. It does not prove whether a missing packet was delayed at Logger or lost on the radio; correlated Logger sender logs are needed for that distinction. No Logger firmware or stale timeout is changed by this instrumentation.

The dashboard downloads `/api/diagnostics.log` as `text/plain` with snapshot counters, sensor summaries, and severity-labelled state transitions, one record per line. Event timestamps are device uptime seconds with millisecond precision; no wall-clock synchronization is implied. This is a syslog-style download, not a network syslog sender. `/api/diagnostics` remains unchanged JSON for tools. Both exports retain the RAM-only 64-transition limit. Help icons expose log-retention, battery-estimate, and OTA notes on click/tap or keyboard activation, with Escape to dismiss.

Dash uses the app's embedded ApexiLabs SVG mark and Inter font via the same Google Fonts stylesheet (system fallback without internet). Its header shows browser date/time and time zone explicitly labelled `(browser)`, plus Dash uptime, following Logger's information layout without implying a shared clock. Dashboard/Settings navigation leads to a dedicated authenticated settings view; LCD selection, refresh interval, colours, and alarms remain protected by the existing admin/CSRF checks. Settings does not fetch LCD images. The firmware card uses a single grid column.

Initial bench capture (2026-09-08, boot `c298bbe78eb8355`, uptime 96,993 ms): 401 accepted writes, zero rejects/disconnects, per-sensor maximum sample gaps 3,400/3,301 ms, and repeated stale→live transitions recovering 50–400 ms after expiry. Maximum Dash loop/web durations were 28/7 ms and render-clock-race count zero. This demonstrates incoming sample gaps, not a proven Logger or radio root cause. Logger upload/SD work was being investigated separately.

The LCD and web UI retain the last finite, valid, fault-free reading during stale data, faults, and BLE disconnection. Held readings are amber and explicitly labelled; never-valid readings remain `--`/unavailable. The API keeps `value:null` and `valid:false` when unavailable and provides separate `lastGoodValue`, `lastGoodAgeMs`, and `displayState` fields. Reconnection retains the catalog for display but invalidates received/valid flags until new packets arrive. Sensor ID changes, catalog-size changes, and Dash reboot clear affected history. Browser network failures also mark retained readings as held/offline. Last-good data stays in RAM and is not recorded as a new sensor sample.

Held-value verification (2026-09-08): Dash build, host tests, and OTA passed; Logger reconnected with the saved two slots/500 ms cadence. A live stale event reported Oil Temp `lastGoodValue:22.61716652`, age 3023 ms, `displayState:Stale`, `valid:false`, and `value:null`. Automated UI checks cover amber held-state text and browser-offline retention. Physical LCD/interactive browser appearance for this change was not visually rechecked.

### Remote LCD preview

The status page omits the explanatory preview paragraph, labels the configured timing simply as `Refresh: <interval> (<frequency>)`, and leaves 12 pixels below the capture status before the preview controls. The technical preview and telemetry behaviour described here is unchanged.

The Live LCD card occupies one desktop grid column beside Sensor readings (both stack on small screens). The image stays 240 pixels wide, shrinking only if necessary. Link-summary text is compact, and automatic status polling no longer toggles the Refresh status button's disabled appearance; only a manually initiated request does. Held sensor text wraps within the narrower card.

Compact-card verification (2026-09-08): host checks and Dash build passed. OTA emitted an unexpected-response warning, but the rebooted device served the new 20:39:39 build. Browser inspection confirmed two adjacent 405-pixel cards, 16-pixel link-summary text, enabled refresh button during polling, and no horizontal overflow at the inspected desktop width. Regression tests distinguish automatic polling from manual-button busy feedback.

### Dash battery and power

Dash measures its **own** battery, not the Logger battery. The [Waveshare ESP32-S3-Touch-LCD-1.28 Rev3 schematic](https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.28/ESP32-S3-Touch-LCD-1.28-Sch.pdf) connects BAT_ADC to GPIO1 through R13=200k/R14=100k (3:1). Eight calibrated ADC millivolt samples are averaged once per second, converted to battery voltage and smoothed with a 0.2 coefficient. The separate `APEXI_DASH_BATTERY_VOLTAGE_GAIN` defaults to 1.0; override in local secrets/build flags only after measuring this Dash battery. The Logger's gain is never reused.

The Battery & power card matches Logger's styling and supported rows: estimated charge, battery voltage and voltage trend. Unsupported USB/5V and charging rows are omitted. The shared approximate resting-voltage curve assumes a conventional 1S 4.2V LiPo; charging/load/temperature affect accuracy. Out-of-range (outside 2.5–4.35V), nonfinite or older-than-five-second readings are unavailable, not 0%. The two-minute voltage trend does not establish charging or discharging. No processor-readable VBUS or charger-status signal is used on this board, so external power and charging remain Unknown. A plausible ADC reading does not prove a battery is fitted.

`/api/status.battery` contains `battery_supported`, nullable `battery_voltage`/`battery_percent`, `external_power: null`, `battery_state: "unknown"`, and `battery_trend`. Loss of the Dash web connection clears these readings. This feature does not alter Logger battery sensing or BLE payloads. Host tests cover conversion, smoothing, invalid input, sampling/freshness, rollover and card formatting. Voltage calibration, unplugged-battery behaviour and power-source transitions require bench confirmation; see the deployment verification below.

### Telemetry upload status dot

Battery deployment verification (2026-09-09): USB flash of Dash build `Sep 9 2026 12:50:33` succeeded. Twenty consecutive status requests retained the same boot, with Logger ready, two saved sensor slots and 500 ms refresh preserved. Voltage reported 4.112–4.115V (approximately 89–90%); this has not been compared with a multimeter. The served HTML contains battery voltage/percentage/trend and omits unsupported USB-power/charging rows. Trend was still collecting during this short check. Logger firmware was not changed by this deployment.

The steady four-pixel-radius dot at (231,120) occupies the right gap between the arcs and remains clear of the alarm banner. Its six-pixel black backing separates it from the arc in single-reading mode. The web UI's Dash Link card provides the matching text, also available as `upload.state` in `/api/status`:

- Green / `accepted`: Logger received HTTP 200 with JSON `status: "ok"` and boolean `accepted: true` for a telemetry **snapshot**. Queued/replayed snapshots count; this does not assert current sensor freshness or downstream persistence.
- Amber / `unconfirmed`: MQTT QoS0 publish returned success locally. There is no server receipt acknowledgement, so this is never shown as green.
- Red / `failed`: the latest snapshot attempt failed, or the transport is unavailable. A later status heartbeat or local queue append cannot clear this.
- Grey / `disabled`, `unknown`, or `stale`: uploads disabled, no supported status received, Bluetooth/handshake unavailable, or expired evidence. A lost browser connection also clears the web indicator to Unknown.

An optional write-without-response characteristic `8f771004-6d7a-4f48-9f8a-67a8c14b6c01` carries a 20-byte v1 packet every 1000 ms when Logger's loop can service it. Byte 0 is version 1; byte 1 is state (0 unknown, 1 disabled, 2 failed, 3 server accepted, 4 transport sent); byte 2 is transport (0 unknown, 1 HTTPS, 2 MQTT); byte 3 is zero. Bytes 4–7 are little-endian last successful snapshot send age in milliseconds (`UINT32_MAX` means never); bytes 8–11 are the expected upload interval; bytes 12–19 are zero. Invalid versions, fields and impossible success/transport combinations are ignored without refreshing freshness. Both sides retain compatibility with older firmware: missing support shows Unknown and sensor telemetry continues unchanged.

Dash expires received status after 5000 ms and successful-send evidence after `max(5000, 3 × expected upload interval)` ms, advancing the reported age locally with rollover-safe elapsed time. Disconnect/reconnect clears the cached evidence. Colour/state, not every heartbeat timestamp, participates in the LCD redraw key. `upload.successAgeMs` and `upload.transport` provide supporting API diagnostics. Settings and sensor alarms are independent of this indicator.

Host checks cover packet validation, expiry, rollover, disconnect/unknown behaviour, colour states and browser-offline clearing. This feature requires coordinated Logger and Dash firmware deployment; a build or mocked preview alone does not verify end-to-end upload delivery.

### Gauge rendering and thresholds

An active configured low or high alarm makes the affected sensor's arc and label pure red, matching its red numeral. This overrides its value-based colour gradient only while the alarm is active. Other sensors keep their own colours. Disabled alarms do not override colours; stale/faulted readings retain the grey arc and amber held value rather than an alarm colour. The LCD framebuffer preview uses the same rendering.

Celsius units are presented as °C on the LCD, web readings, settings legends and offline preview. Telemetry and rule matching retain the canonical `C` unit. The LCD draws a small degree ring beside C rather than relying on a Unicode glyph in its bitmap font.
Units use the larger 16-pixel bitmap font; stale/fault captions remain compact. Numerals have an eight-pixel top-to-bottom italic shear within the existing framebuffer, with reserved width to avoid clipping. No additional sprite allocation is required.
The separate one-pixel outer rim is removed. Gauge arcs extend to radius 120 (the display edge), keeping their inner radius at 104 so label, digit and alarm clearance is unchanged. The overlaid highlight is removed too: its black-background anti-aliasing created a dark internal seam. Each arc is one uninterrupted colour band. Dash formats bar readings to two decimals, including held values, while other units retain one decimal.

USB diagnostic bench result (2026-09-09): the initial gauge firmware repeatedly panicked with MMU/cache errors and a corrupted application-core backtrace. Instrumentation showed complete gauge rendering before failure, and also a failure before any gauge render. The status handler's 6144-byte stack JSON document produced a 6960-byte compiled function frame on the 8192-byte Arduino loop stack, before web-server callers. Moving that document to the heap reduced the function frame to 848 bytes; the diagnostic build served repeated status polls with the Logger connected and at least 4308 bytes of measured stack high-water headroom. The diagnostic variant retains UART status-entry/exit traces and is not a final release qualification. Legacy two-slot selection and 500 ms refresh were retained. Intermittent stale samples still occurred without resets; this is a separate acquisition/transport investigation. No Logger firmware was changed.

The redesign adapts the reference to a 240×240 display: black face, solid fixed-width arcs, large white numerals, colour-matched labels, and a conditional red alarm band. It intentionally omits the reference's dense carbon texture and decorative traces to retain contrast at native resolution. Arc angle, radius and width never depend on the measurement: only colour changes. The sprite remains RGB332, so smooth RGB interpolation is quantised by the panel buffer. No flashing animation is used.

In the authenticated Dash `/settings` page, each rule is keyed by sensor ID **and units**, not display-slot position:

- Four strictly increasing engineering values define blue, green, yellow and red. Values between points interpolate linearly in RGB; values outside the range clamp to the endpoint colour.
- Independent **Enable low alarm** and **Enable high alarm** switches are off by default. Limits trigger inclusively (`value <= low`, `value >= high`). If both are enabled, low must be less than high. Reaching a colour point alone does not trigger an alarm.
- Default known-sensor colour points divide the `AppConfig::kSensorConfigs` engineering range into thirds. Suggested low/high limits come from that configuration, but remain disabled. Unknown sensors receive editable generic 0/33/66/100 colour points in the form; review them before saving.
- All configured sensors are evaluated, including hidden/unselected sensors. Multiple simultaneous breaches show a count on the LCD and sensor names/directions in the web UI. No latch, hysteresis, audible output, or safety interlock is implemented.
- Only fresh, finite, valid, fault-free samples trigger alarms. A stale/disconnected/faulted reading retains its last good number in amber and uses a muted grey arc; it does not retain or generate a threshold alarm. Never-valid readings remain unavailable. Unconfigured sensors use cyan while fresh. A unit mismatch disables the rule until reconfigured, preventing bar limits from being applied to another unit.
- Remove an obsolete rule using its checkbox and save. If a sensor changed units, remove/save/reload to create its new-unit rule. Newly discovered sensors append controls without resetting unsaved edits. Maximum eight rules; remove obsolete rules before adding more.

Rules are included as `gauges` in read-only `/api/status`; each sensor has an `alarm` field (`""`, `LOW`, `HIGH`). Authenticated, CSRF-protected `POST /api/settings` accepts an optional complete `gauges` array of `{id, units, points:[blue,green,yellow,red], low, high, lowEnabled, highEnabled}` along with existing refresh/slots. Omission preserves rules for older clients; an empty array removes them. Requests are capped at 4096 bytes. Duplicate/empty IDs, overlong strings, nonnumeric/nonfinite values, unordered points, and invalid enabled limits are rejected before persistence. Numeric inputs are bounded to ±1,000,000. No Logger payload or acquisition threshold is changed.

Slots, cadence, and rules save atomically in the versioned NVS `display-v2` blob. On first upgrade, the legacy `display` blob migrates slots/cadence while new alarms stay disabled; the legacy key remains for rollback. Settings changed in v2 do not propagate back to old firmware. Invalid v2 settings fall back to validated legacy slots/cadence and disabled-alarm defaults. Hardware power-loss/reboot migration still needs bench qualification.

The dual layout reserves y=110–130 for alarms, keeping labels/values/units outside it. With one selected reading, the alarm appears below the large number instead. Captions and long values shrink/ellipsise within circular bounds (100px labels, 88px details, 124px dual/170px single values). An alarm from a hidden sensor can display even when both slots are hidden. The freshness timestamp is taken after the sensor snapshot to avoid the prior render-clock race.

Verification: `./scripts/verify-repo.sh --fast` covers policy boundaries, stale suppression, colour interpolation/clamping, form validation/edit preservation, circular bounds, and JSON parsing/round-trip (when ArduinoJson dependencies are installed). CI reruns host tests after building firmware so JSON integration checks run. `node scripts/preview-dash-gauge.cjs` generates ignored offline gauge/settings fixtures under `.build-tests/`; these use production policy/layout and exact web-page source, but approximate TFT fonts with SVG. No device deployment is part of the redesign PR. Validate actual glyph fit, NVS migration, and thresholds with controlled hardware samples before use as an alarm instrument.

The Dash web UI's **Live LCD** card displays the actual 240×240 RGB332 sprite sent to the panel, clipped to its round shape. It fetches only changed frames, at most once per second while the page is visible, using the configured web polling interval. Pause/resume retains the last image; lost connectivity marks it stale. This supports remote layout development, but cannot prove physical panel wiring, brightness, tearing, or flicker.

Read-only `GET /api/lcd.bmp` serves a 58,678-byte, top-down indexed BMP directly from the existing sprite without allocating a second frame buffer. The endpoint shares the status page's LAN access boundary; do not expose it publicly. `X-LCD-Boot`, `X-LCD-Frame`, and `ETag` identify the captured frame. Matching `If-None-Match` returns 304; image transfers are globally limited to one per second (429 with `Retry-After: 1`), and unavailable buffers return 503. `/api/status` includes `lcdBootId` so a reboot invalidates a cached frame even if the counter repeats. The synchronous web handler and renderer run on the same loop, keeping each capture consistent. OTA temporarily interrupts web serving; the preview is not an OTA progress stream.

Preview verification (2026-09-08): Dash build and OTA passed; after reboot Wi-Fi, OTA, and Logger handshake were ready, with the original two slots and 500 ms refresh retained. The live bitmap returned HTTP 200, the expected 58,678 bytes and 240×240 top-down dimensions; conditional requests returned 304 and immediate repeat transfers returned 429. The decoded device image was visually checked: black background, two labels, amber `--`, and sensor-fault details (the bench ADC is absent). Host tests cover BMP headers/palette and preview change detection, rate cap, pause/resume, hidden tabs, and failed requests. Browser card visual verification was blocked by the locked workstation; physical panel behaviour remains unverified.

## Core BOM

| Qty | Item | Purpose | Notes |
| --- | --- | --- | --- |
| 1 | NodeMCU 1.0 / ESP-12E DevKit V2 | Main controller | Supported default target; PlatformIO board ID `nodemcuv2` |
| 1 | Waveshare ESP32-S3-Touch-LCD-1.28 | Separate round dash | PlatformIO environment `dash-waveshare-s3-128`; onboard GC9A01A LCD |
| 2 | [Gravity Analog Current to Voltage Converter (DFRobot SEN0262)](https://core-electronics.com.au/gravity-analog-current-to-voltage-converter-for-4-20ma-application.html) | Converts each loop signal into a board-friendly voltage | One module per sensor channel |
| 1 | [ADS1115 16-bit ADC breakout (Adafruit ADA1085)](https://core-electronics.com.au/ads1115-16-bit-adc-4-channel-with-programmable-gain-amplifier.html) | Reads the module voltage outputs | ADS1115 board for the receiver outputs |
| 1 | [Digital Push Button, white (DFRobot DFR0029-W)](https://core-electronics.com.au/digital-push-button-white.html) | UI mode toggle and latched fault clear | Connect to the configured button input |
| 1 | [Fused 12 V input path](https://www.bluesea.com/products/5064/) | Protects the incoming 12 V feed | Inline fuse holder or a prebuilt fused automotive input lead |
| 1 | [12 V reverse-polarity/transient protection module (Pololu 5380)](https://core-electronics.com.au/pololu-reverse-voltage-protector-4-60v-10a.html) | Protects the electronics from common vehicle power faults | Prefer a prebuilt automotive power protection module |
| 1 | [DC-DC Multi-output Buck Converter (DFRobot DFR1015)](https://core-electronics.com.au/dc-dc-multi-output-buck-converter-33v5v9v12v.html) | Generates the regulated supply rail | Use the 5 V rail and keep upstream automotive protection |
| 1 | [Enclosure and wiring hardware](https://www.printables.com/tag/projectbox) | Physical integration | Printed enclosure or purchased box, plus harness, terminals, mounting hardware, and grounding hardware |

## Optional additions

| Qty | Item | Purpose | Notes |
| --- | --- | --- | --- |
| 1 | [3.5 inch 480x320 SPI TFT with ST7796S controller](https://core-electronics.com.au/catalog/product/view/sku/WS-15811) | Legacy directly wired local display | Optional for older builds; the separate Waveshare dash is the current direction |
| 1 | Unexpected Maker RTC Logger Shield with RV-3028-C7 | Timestamps without network time | Enabled; shield GPIO 8/SDA connects to D2 and GPIO 9/SCL connects to D1 |
| 1 | FAT32 microSD card in the Unexpected Maker RTC Logger Shield | Durable local CSV storage | Enabled; shield pins 36/37/35/34 map to D5/D6/D7/D0 respectively |
| 1 | [24 V boost regulator for loop-powered sensors (Pololu U3V9F24, item 5588)](https://core-electronics.com.au/catalog/product/view/sku/POLOLU-5588) | Generates a dedicated 24 V sensor supply from the 12 V system rail | Optional; use only when a transmitter needs 24 V loop power and place it after the [Pololu 5380 reverse-voltage protector](https://core-electronics.com.au/pololu-reverse-voltage-protector-4-60v-10a.html) |

Recommended example module:
- [DFRobot Gravity Analog Current to Voltage Converter (SEN0262)](https://core-electronics.com.au/gravity-analog-current-to-voltage-converter-for-4-20ma-application.html) for the current breakout-style implementation.

Optional hardware toggles:
- Set `AppConfig::kFeatures.displayEnabled` to `false` when no TFT is fitted.
- Set `AppConfig::kDashLink.enabled` to `false` when an ESP32-family logger should not search for the separate dash.
- Set `AppConfig::kFeatures.rtcEnabled` to `false` when no RTC hardware is fitted.
- Set `AppConfig::kFeatures.sdLoggingEnabled` to `false` when no SD hardware is fitted.

When microSD logging is disabled, the local dashboard leaves the CSV card visible but disabled and does not request the SD file list. The main page otherwise stays focused on compact sensor readings and a fault summary; use its **Diagnostics** action for detailed connectivity, hardware, time, storage, transport, and per-sensor state.

The logger web dashboard displays bar readings to two decimal places (for example, `0.12 bar` or `8.00 bar`), with other units retaining one decimal place. This display precision does not change sensor accuracy, sampling, API values or CSV data.

ESP32 app credential provisioning is moving to hardware-derived public identity plus owner-approved, NVS-stored issued credentials. See [device authorization](device-authorization.md) for the draft connect/recovery/rotation flow, preserved legacy identities, local authentication requirements, and deployment gates. A MAC address is not a secret, and development-board NVS must not be described as encrypted unless separately commissioned that way.

The ESP32 HTTPS path now uses a shared asynchronous worker with same-origin connection reuse and continuous FIFO replay. Sensor capture remains configured at 250 ms for uploads and 10 ms for acquisition; the network no longer runs on that loop. All captures are durably queued when LittleFS is ready, including online captures, so flash endurance remains a production qualification item. See [HTTPS replay](https-replay.md) for counters, ownership, compatibility and throughput acceptance. ESP8266 remains synchronous.

Authorization bench update (2026-09-09): dev app `5333a88` deployed and TinyC6 `10.0.40.177` flashed application-only. Start/poll succeeds and pending authorization survives OTA. A remote-log worker previously attempted empty-bearer HTTPS requests and competed with authorization for TLS memory; it now stays disabled without a token, and HTTPS clients share a single-session budget. ADC/RTC/SD remained ready and Dash reconnected without a Dash flash. Owner approval and subsequent ingest/rotation acceptance are still pending; queue overflow during the earlier outage has occurred, so retained SD CSVs remain important.

Later acceptance the same day: owner approval and credential acknowledgement completed; stored credentials survived application-only OTA and reboot. App logs confirmed snapshot HTTP 200 and independent InfluxDB readback confirmed historical queued samples. Configuration version 6 restored AWST. Three readings showed stable uptime (34/62/85 seconds), queue counts 1501/1497/1493, an unchanged 14677 dropped records from the earlier outage, fresh server acceptance evidence, and ready SD/Dash. The final build backs off unsupported remote-log requests for 60 seconds. Full backlog drain, fresh capture readback, management-off rotation and revoked-token recovery remain unverified; the earlier static-token transfer proposal is superseded by this owner-approved flow.

Logger upload evidence: the optional Dash upload-status characteristic is sent once per second without response and does not change sensor frames. HTTPS snapshot success requires HTTP 200 with a JSON object containing `status: "ok"` and boolean `accepted: true`; successful status heartbeats and local queue writes do not count. Replay acknowledgements count as accepted telemetry, not current-sample freshness. MQTT QoS0 publish success is only a local transport write, not telemetry-server acceptance. Failed snapshot attempts and unavailable network transport are distinct from disabled or not-yet-attempted uploads. The last successful snapshot age is retained across failures. Invalid 2xx snapshot acknowledgements are retryable failures and never remove a queued record. The indicator does not itself prove downstream persistence. `/api/live` includes `upload_http_status`, numeric `upload_evidence_state` (0 unknown, 1 disabled, 2 failed, 3 server accepted, 4 transport sent), and nullable `upload_success_age_ms`. Physical mixed-version BLE and failure/recovery testing are still required before rollout.

TinyC6 queue recovery: its default 8MB partition table contains a 1.5MiB data partition labelled `spiffs` at `0x670000`; the label does not dictate the filesystem format. The logger first selects a data/SPIFFS-subtype partition named `littlefs`, then `spiffs`. It mounts as LittleFS without format-on-failure. Only if every byte is readable and erased (`0xff`) does it initialize that selected partition. Foreign/corrupt filesystems are preserved and reported as unavailable; recovery requires separate review. Bootloader, app partitions, NVS, SD and partition table are unchanged. With the 512KiB reserve, the default partition provides approximately 1MiB queue capacity. Initialization failures and upstream HTTP errors remain visible rather than being overwritten by a generic queue error.

Bench verification (2026-09-09): TinyC6 application-only OTA succeeded; queue capacity was 1,048,576 bytes and pending records survived a subsequent application-only OTA (277 pending, zero dropped). Logger reported HTTP401 and evidence state2; connected Dash `/api/status` reported `upload.state: "failed"`, transport HTTPS. The logger's compiled bearer resolved as anonymous on the deployed app; credential synchronization requires a separately approved protected transfer. Successful upstream acceptance/replay drain has not yet been verified. Fast host/JS/contract checks and TinyC6 firmware build passed.

The default is station mode using credentials from the ignored `include/AppSecrets.h`. The firmware performs a normal all-channel scan rather than pinning a BSSID or channel. At every networked boot, station-mode firmware obtains NTP time and writes configured local time into the RV-3028, even when its retained calendar is already valid. It refreshes the RTC hourly while online and uses the hardware clock as holdover while offline. When an RTC is absent or cannot be written, valid NTP/system time still supplies timestamps while the UI correctly reports `rtc_ready=false`. UI and CSV timestamps use configured local wall time; live transport payloads use timezone-qualified UTC RFC 3339 timestamps. The authenticated `/settings` page configures both NTP servers, a POSIX timezone rule, and the short label shown beside device time; defaults are `pool.ntp.org`, `time.google.com`, `AWST-8`, and `AWST`. The web UI reports NTP synchronization state and the last successful RTC update. If association fails within 30 seconds, it exposes the open fallback SoftAP `MDA-LOGGER` on 2.4 GHz channel 6 at `http://192.168.44.1`. Leave `apPassword` empty for an open recovery AP or set an 8+ character WPA2 password. Change `AppConfig::kWifi.apAddress` if that subnet is already in use.

For station Wi-Fi or live-upload commissioning, copy [`include/AppSecrets.example.h`](../include/AppSecrets.example.h) to the git-ignored `include/AppSecrets.h`. Keep Wi-Fi and transport credentials in that local file. An authenticated production broker requires `APEXI_MQTT_USERNAME` to equal the normalized `AppConfig::kLiveUpload.deviceId`. When direct MQTT is unavailable, enable HTTPS and provision a dedicated Cloudflare Access service token plus a scoped app device token. The Access application must use a **Service Auth** policy for that token; the device sends both Access headers and its app bearer on every request. The Access client ID and secret may alternatively be replaced from the authenticated local settings page. Both fields are write-only: the page reports only whether a value exists, a blank submission preserves it, and neither value is returned to the browser or local status API.

Remote management is optional and starts disabled. To pair a logger, first enable both **Live upload** and **Allow remote management** on the digest-authenticated local `/settings` page. Reopen `/settings`, then enter its temporary eight-character code in the app's single **User profile → Paired devices** field; no logger ID is required. The page shows when the code will refresh, and the device generates and publishes a replacement every ten minutes. After the app identifies and claims it as a telemetry logger, explicitly enable remote configuration for that logger. Each management heartbeat reports the logger's current live-upload and NTP/timezone values so the app can pre-fill its form without exposing Cloudflare Access, app, MQTT, Wi-Fi, or OTA credentials. The device checks desired configuration identity, completeness, and monotonic version before persisting it. If recovery is needed, disable remote management locally; no app command may change the broker host, MQTT credential, OTA password, sensor calibration, or remote-management opt-in.

For the managed development environment, use broker hostname `apexlabs-dev`, port `1883`, and topic prefix `motorsport/logger` from an allowed LAN or VPN. Set `AppConfig::kWifi.mode` to `WifiMode::Station` and `AppConfig::kFeatures.liveUploadEnabled` to `true` before building the track firmware.

Commissioning is complete only when the device UI reports `MQTT LIVE` or `HTTPS LIVE`, `/api/live` reports a current `system.upload_session_id` and increasing `system.upload_sequence`, and the same source session appears in the telemetry app. Each reboot intentionally creates a new source session. HTTPS is outbound-only and polls desired configuration in status responses, so it does not require an SSH tunnel or inbound device route.

When using receiver modules:
- Treat the field 4-20 mA transmitters as external inputs to the logger rather than part of the logger BOM.
- Treat the removable microSD card as runtime media rather than part of the logger BOM when SD logging hardware is installed.
- Confirm the module output range before wiring it to the ADS1115. The supported design does not use the NodeMCU's direct `A0` input.
- The `DFR1015` power module does not replace the need for a fuse and upstream automotive protection when installed in a vehicle.
- If a sensor needs 24 V loop power from a 12 V vehicle supply, prefer the explicit [Pololu 5380 reverse-voltage protector](https://core-electronics.com.au/pololu-reverse-voltage-protector-4-60v-10a.html) plus [Pololu U3V9F24 (item 5588)](https://core-electronics.com.au/catalog/product/view/sku/POLOLU-5588) stack instead of a generic high-power adjustable module.
- The NodeMCU has fewer GPIOs than the former TinyS3 design. Validate boot-strap levels and power requirements before enabling the optional TFT, RTC, SD, or button paths together.
- Update the engineering conversion assumptions in [`include/AppConfig.h`](../include/AppConfig.h) if the module output scaling no longer matches the original shunt-based design.

## Commissioning checklist
1. Confirm the sensor supply voltage and compliance requirement from the actual transmitter datasheets.
2. Verify the receiver module output voltage at 4 mA and 20 mA before connecting it to the ADS1115.
3. Confirm the legacy directly wired TFT controller is ST7796S. If it is ILI9488 or another controller, update [`include/LoggerDisplayTFTSetup.h`](../include/LoggerDisplayTFTSetup.h).
4. Set the RTC to the correct time before field logging.
5. Inject 4, 8, 12, 16, and 20 mA into each channel and verify the receiver modules and displayed engineering units match the configured ranges.
6. Confirm the serial boot report shows `wifiReady=1`, station mode, and a DHCP address before installing the logger in the vehicle.
