# Spare-board M100 Mini GNSS / standby test

This example is separate from the main FairWeather Friend firmware. It uses the
**spare XIAO ESP32-S3**, with full-duplex UART 1 at **115200 baud, 8N1**.

| M100 Mini | Spare XIAO |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| TX | D7 / GPIO44 (UART RX) |
| RX | D5 / GPIO6 (UART TX) |

D5 is UART TX in this standalone sketch; D4 is unused here. The main firmware now uses the same D7 RX / D5 TX wiring for standby/wake;
D4 remains TFT RESET there. Keep this standalone test cycle on the spare.

## Test sequence

1. Wait up to 20 seconds for an acceptable checksum-valid GGA fix. The original
   criteria are unchanged: valid non-(0,0) coordinates, quality 1/2/4/5, ≥4 used
   satellites, and finite HDOP greater than zero and ≤3.0. Optional altitude does
   not affect acceptance. No raw NMEA stream or GPS library is used.
2. After reporting the accepted fix, poll **UBX-MON-VER** (`0A 04`, zero payload)
   to identify software/hardware. A missing response is explicitly reported;
   it does not fabricate firmware support.
3. Send **UBX-RXM-PMREQ** (`02 41`), version 0, 16-byte payload, duration
   **10000 ms**, flags **0x00000006** (backup + force), wakeupSources
   **0x00000008** (UART RX edge). The complete transmitted packet is:

   ```text
   B5 62 02 41 10 00 00 00 00 00 10 27 00 00 06 00 00 00 08 00 00 00 98 D8
   ```

4. Allow a 1-second settling window, then count incoming bytes and checksum-valid
   GGA sentences for 3 seconds. Keep UART reception enabled. A true
   checksum-valid ACK-ACK for class/id `02 41` prints **GNSS standby acknowledged**.
   A matching ACK-NAK is reported separately. Other ACKs and bad checksums do not
   count. **Silence never substitutes for an acknowledgment.**
5. After that observation window, send **0xFF** on UART TX, providing an edge on
   the module's configured UART-RX wake input. There is no invented UBX resume
   command. Wait 1500 ms non-blockingly for startup, then poll MON-VER again.
6. Within 5 seconds of the wake edge, require a new checksum-valid GGA to print
   **GNSS awake** and **NMEA resumed**. Report whether the position fix is already
   acceptable or is still being reacquired. If standby was not confirmed, the
   awake message says so. No resume data means an explicit failure.
7. Stop the test/UART and repetitive output. RESET repeats the sequence. A failed
   initial fix ends the test without sending any standby command.

The 10-second standby duration is a safety fallback. The intended UART wake and
its observation deadline occur before automatic expiry, so the test does not
mistake the timer's wake for UART wake. Configuration is not written to flash or
BBR, and no reset, baud-changing, power-pin, or main-firmware command is sent.

This sequence follows the M10 PMREQ fields and UART wake source in the
[u-blox M10 SPG 5.10 interface description, pp. 116–117](https://content.u-blox.com/sites/default/files/u-blox-M10-SPG-5.10_InterfaceDescription_UBX-21035062.pdf)
and software standby guidance in the
[M10 evaluation user guide, §8.6](https://content.u-blox.com/sites/default/files/documents/EVK-M10_UserGuide_UBX-21003949.pdf).

## Expected output

Actual coordinates vary. A receiver that returns a matching ACK and resumes
NMEA can produce:

```text
Waiting for GNSS fix (20 s)...
GPS FIX OK
Fix: valid
Lat: 32.710292
Lon: -95.496410
Satellites: 6
HDOP: 2.26
Altitude: 185.0 m
Receiver MON-VER: SW ... / HW ...
Entering GNSS standby...
GNSS standby acknowledged
Standby observation: 0 RX bytes, 0 GGA sentences in 3 s
NMEA stopped; UART silence observed (power state not measured)
Waking GNSS...
GNSS awake
NMEA resumed (checksum-valid GGA)
Fix after wake: valid
```

If no matching ACK is received, the exact message is:

```text
No UBX standby ACK received (silence is not an ACK)
```

It still observes output and attempts UART wake. Unsupported commands, continuing
output, missing MON-VER, wake timeout and invalid post-wake fix are explicitly
reported. Success messages are based on received data, not unconditional prints.

## Limitations

- Actual module firmware/support, ACK behavior, output cessation and resume must
  be measured on hardware. Host tests simulate these paths; they cannot establish
  the module's power state.
- UART silence plus subsequent NMEA is evidence of the requested output change,
  not a current measurement. Measure module current separately to verify savings.
- M10 software standby may clear RAM settings. Volatile UART baud/message settings
  may revert; this test listens at the confirmed 115200 only. A wake failure can
  mean changed baud rather than failed power management. No persistent settings
  are modified to compensate. The receiver's timed wake remains available.
- Awake means fresh GGA output, not necessarily a recovered position fix. After
  standby the receiver may need more time to reacquire satellites.
- Silence could also indicate a disconnected TX wire. The initial valid fix,
  optional ACK, and fresh post-wake GGA are independent checks.

## Upload manually to the spare and test

1. Disconnect the main board from USB. With power off, add M100 RX → spare D5;
   retain TX → D7, VCC → 3V3 and GND → GND.
2. Open **this existing `gnss_m100_test.ino`** in Arduino IDE so the accompanying
   `nmea_gga.h` and `ubx_power.h` are included. Do not paste only the .ino into an
   unsaved sketch.
3. Select **XIAO_ESP32S3**, **USB CDC On Boot: Enabled**, and the spare USB port.
   Verify, then upload yourself. Nothing is uploaded automatically by this task.
4. Open Serial Monitor at **115200**, place the antenna outdoors facing up, then
   press RESET. Record MON-VER, any ACK/NAK, quiet-window counts, wake outcome and
   post-wake fix validity. Output stops after one test cycle.
5. If no initial fix is acquired, leave the module powered with a clear sky view
   and RESET to retry. If wake fails, wait past timed expiry; RESET or power-cycle
   to establish the normal baud/output again and retain the diagnostic log.

Build only from repository root:

```sh
arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32S3 examples/gnss_m100_test
```

The installed package defaults USB CDC to enabled (`CDCOnBoot=default`), not
`CDCOnBoot=cdc` (which denotes disabled in this board package).

Host tests share the sketch's parser and exercise the actual state machine:

```sh
c++ -std=c++17 examples/gnss_m100_test/tests/test_nmea.cpp -o /tmp/fwf-nmea-test
/tmp/fwf-nmea-test
c++ -std=c++17 -Iexamples/gnss_m100_test/tests/stubs examples/gnss_m100_test/tests/test_sketch.cpp -o /tmp/fwf-standby-test
/tmp/fwf-standby-test
```
