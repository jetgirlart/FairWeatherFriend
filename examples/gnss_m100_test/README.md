# Standalone HGLRC M100 Mini GNSS test

For the **spare Seeed Studio XIAO ESP32-S3**. This example is separate from
`altoids-pet/firmware/altoids_pet/` and does not use or modify its firmware,
configuration, Wi-Fi, saves, display, or power behavior. Uploading this sketch
replaces the program on whichever board you select, so choose the spare board.

## Wiring

| M100 Mini | Spare XIAO ESP32-S3 |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| TX | D7 (GPIO44) |
| RX | Not connected |

Use the confirmed **115200 baud**, 8 data bits, no parity, 1 stop bit.
`HardwareSerial(1)` receives on D7 with UART TX disabled (`-1`); no commands are
sent to the M100 and no second GPIO is required. Power down while wiring.

## Parsing and acceptance

No external GNSS library is used. `nmea_gga.h` is a small fixed-size parser for
checksum-validated GGA sentences, including GP/GN/GL/GA/GB/BD talkers. GGA carries
coordinates, quality, satellites **used**, HDOP and mean-sea-level altitude; see
[u-blox's GGA field definitions](https://content.u-blox.com/sites/default/files/documents/u-blox-F9-HPG-1.32_InterfaceDescription_UBX-22008968.pdf).
The parser does not mix fields from different sentences. Other sentence types,
bad checksums, malformed coordinates and overflowing lines are ignored.

A single GGA sentence must meet **all** of these conditions:

- Latitude and longitude parse correctly, including degree/minute ranges and
  N/S/E/W signs; decimal degrees are within ±90/±180.
- The coordinate pair is not `(0,0)`. A zero in just one coordinate is legitimate.
- Quality is 1 (autonomous), 2 (differential), 4 (RTK fixed), or 5 (RTK float).
  Invalid, estimated/dead-reckoning, manual and simulated fixes are rejected.
- At least **4 satellites are used**.
- HDOP is present, finite, **greater than 0 and ≤3.0**. Missing HDOP cannot pass
  this quality gate. HDOP is a geometry indicator, not measured error in metres.

Altitude is optional and reported only if finite with the NMEA `M` unit. Missing
altitude does not reject an otherwise acceptable fix. Altitude is above mean sea
level, not the height of the receiver above nearby ground.

Acquisition starts after UART initialization and a bounded, up-to-2-second USB
readiness wait. `loop()` processes at most 512 bytes per call with no animation
or acquisition delays. It accepts the first qualifying fix within **20 seconds**;
otherwise it fails. The UART is stopped after the final report and there are no
repetitive updates or raw NMEA dumps. Press RESET for another acquisition.

## Serial output

A successful acquisition produces this once (actual values vary):

```text
Waiting for GNSS fix (20 s)...
GPS FIX OK
Fix: valid
Lat: 32.710183
Lon: -95.496105
Satellites: 10
HDOP: 1.03
Altitude: 162.4 m
```

Failure starts with:

```text
GPS FIX FAILED
Fix: invalid
```

It then reports the last checksum-valid GGA's fields, or `unavailable` if absent.
Those last coordinates are diagnostic only: they are **not** an accepted fix.
Nothing else is printed after either result. The USB startup message is the only
message preceding the final result.

## Upload and test manually

1. Leave the main FairWeather Friend board disconnected from USB. Wire the M100
   to the spare XIAO as shown, then connect the spare to USB.
2. In Arduino IDE, use the existing Espressif ESP32 board package. Select
   **XIAO_ESP32S3**, enable **USB CDC On Boot** for USB Serial Monitor, and choose
   the spare board's USB port. No new Arduino libraries are required.
3. Open **`examples/gnss_m100_test/gnss_m100_test.ino`**. Its containing folder
   matches the sketch name. Do not open/upload the main `altoids_pet.ino`.
4. Verify, then click Upload yourself. This task does not upload automatically.
5. Open Serial Monitor at **115200 baud**. Place the module with a clear view of
   the sky, antenna facing upward. Press the spare XIAO's RESET button with the
   monitor open so you see the entire acquisition.
6. Confirm a single success within 20 seconds, reasonable coordinates, at least
   four used satellites and HDOP ≤3.0. Observe that output then stops. Optional
   altitude should include `m` or explicitly say unavailable.
7. For failure, run with the GNSS TX disconnected or without a usable fix: after
   20 seconds it must print one `GPS FIX FAILED` report and then stay quiet.
8. Reconnect TX and press RESET to retry. A cold receiver can need longer than
   this intentionally short window; keep it powered outdoors and retry. This
   test does not configure the receiver or lengthen the specified timeout.

Command-line **build only**, from the repository root:

```sh
arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32S3 examples/gnss_m100_test
```

If your board-package menu requires an explicit override, append
`:CDCOnBoot=default` to that FQBN. Upload only from Arduino IDE to the verified spare
board. Host tests exercise the same parser used by the sketch:

```sh
c++ -std=c++17 examples/gnss_m100_test/tests/test_nmea.cpp -o /tmp/fwf-gnss-parser-test
/tmp/fwf-gnss-parser-test
c++ -std=c++17 -Iexamples/gnss_m100_test/tests/stubs examples/gnss_m100_test/tests/test_sketch.cpp -o /tmp/fwf-gnss-sketch-test
/tmp/fwf-gnss-sketch-test
```
