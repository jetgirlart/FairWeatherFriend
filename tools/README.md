# FairWeather Friend USB backup utility

Export and restore buddy saves without Arduino IDE. This is a command-line
Python 3 utility, not a GUI. **Python 3.8 or newer** is required; **pyserial** is
the only external dependency. Firmware and its USB protocol are unchanged.
Run the commands below from the repository root (`FairWeatherFriend`).

## Install

macOS (Terminal; install Python 3 first if `python3` is unavailable):

```sh
python3 -m venv tools/.venv
source tools/.venv/bin/activate
python -m pip install -r tools/requirements.txt
```

Windows (PowerShell; install Python 3 first if `py` is unavailable):

```powershell
py -3 -m venv tools/.venv
# Use the environment's interpreter directly; no activation/script-policy change needed.
.\tools\.venv\Scripts\python.exe -m pip install -r tools/requirements.txt
```

Linux (Terminal; install Python 3 and its venv support using your distribution
if needed):

```sh
python3 -m venv tools/.venv
source tools/.venv/bin/activate
python -m pip install -r tools/requirements.txt
```

On Windows, replace `python` in every command below with
`.\tools\.venv\Scripts\python.exe`. On macOS/Linux, activate the environment in
each new terminal, or use `tools/.venv/bin/python` directly.

## Export

1. Connect the XIAO with a USB **data** cable. Close Arduino Serial Monitor and
   other applications using its serial port. Wake the buddy with B.
2. Run:

```sh
python tools/fairweather_backup.py export
```

The utility selects a likely XIAO port and sends `EXPORT_BUDDY` at 115200 baud.
The EXPORT BUDDY screen is optional for this command; it shows EXPORT COMPLETE
if open. Exit the device's IMPORT BUDDY screen before exporting.
Startup/clock/weather logs are skipped. The returned JSON is checked before
saving, for example:

```text
backups/fairweather_buddy_20261008T154500_123456Z.json
```

Timestamps are UTC with microseconds. Backups are never overwritten. The default
`backups/` directory is relative to your current working directory (gitignored
when run from the repository root). Choose another folder if desired:

```sh
python tools/fairweather_backup.py export --output-dir "$HOME/Desktop/BuddyBackups"
```

Windows example:

```powershell
.\tools\.venv\Scripts\python.exe tools/fairweather_backup.py export --output-dir "$HOME\Desktop\BuddyBackups"
```

## Import

```sh
python tools/fairweather_backup.py import backups/fairweather_buddy_20261008T154500_123456Z.json
```

The utility validates the local JSON before opening a port. It then asks you to
open **SETTINGS → IMPORT BUDDY** on the device and type **IMPORT** on the computer.
Any other response cancels without sending the backup. After you approve, it
sends the compact one-line `IMPORT_BUDDY` request. The firmware validates the
full save schema, version, checksum, gear, palette, and records.

When the buddy displays **REPLACE CURRENT BUDDY?**, press **B** on the device to
commit or **C** to cancel. The tool does not send Serial confirmation and cannot
bypass the B requirement. Success is reported only after receiving
`Buddy import committed and verified.` from the device. Rejected data, canceled
or expired confirmation, save failures and disconnects produce clear failures.
On interruption or failure after sending an import, the utility attempts
`CANCEL_IMPORT`; reset, sleep or the firmware's timeout also discards staging.

Confirm promptly: **normal 30-second inactivity sleep still applies**, and
firmware staging expires after 60 seconds. The utility does not keep the device
awake, change power policy, upload firmware, or intentionally reset the board.
If opening the port resets your particular USB configuration, reopen the IMPORT
screen before typing IMPORT. Export again afterward to verify your restored save.

Local validation rejects malformed/non-object JSON, duplicate keys, non-finite
numbers and missing/invalid saveVersion/checksum metadata. It does not duplicate
the firmware's complete schema or recompute its binary-payload checksum. The
device remains authoritative. Import commands over the firmware's 8205-byte
line limit (8192 JSON bytes plus `IMPORT_BUDDY `) are rejected locally. Reading a file or staging it never overwrites
buddy data.

## Ports and troubleshooting

```sh
python tools/fairweather_backup.py list-ports
python tools/fairweather_backup.py export --port /dev/cu.usbmodem123
python tools/fairweather_backup.py import buddy.json --port COM5
python tools/fairweather_backup.py export --port /dev/ttyACM0 --timeout 30
```

Typical names are `/dev/cu.usbmodem…` on macOS, `COM…` on Windows, and
`/dev/ttyACM…` on Linux. Detection prefers XIAO ESP32-S3 USB IDs, then XIAO
labels, then an Espressif USB device. Multiple equally likely devices require
an explicit `--port`; unidentified ports also require an override.

- No port: wake with B, check the data cable, and run `list-ports` again. Deep
  sleep can disconnect USB; the port may change after reconnection.
- Busy port: close Serial Monitor and other serial tools, then retry.
- Linux permission denied: grant your account access to the serial device using
  your distribution's serial-access group (often `dialout`), then sign out/in.
  Do not run the backup utility as root to work around permissions.
- Timeout: ensure the buddy is awake and the port is correct. Imports require
  its IMPORT BUDDY screen. Do not interpret a timeout as a confirmed save.
- Export response timeout defaults to 20 seconds; import timeout defaults to
  70 seconds to allow the device confirmation. `--timeout` changes only the
  computer's wait, not device sleep or confirmation limits.

Exit codes: `0` success or user cancellation before sending, `1` operation
failure, `2` invalid CLI arguments, `130` keyboard interruption.

## Host tests

```sh
python -m unittest discover -s tools/tests -v
python tools/fairweather_backup.py --help
```

Tests use simulated ports/Serial responses and temporary files. Where pyserial
and POSIX pseudo-terminals are available, an integration test also runs the CLI
against a virtual serial device. No physical device is opened or uploaded to.

Version 6 backups include severe-weather event records and the bounded recent-alert
fingerprint cache; versions 1–5 remain supported by firmware migration.
