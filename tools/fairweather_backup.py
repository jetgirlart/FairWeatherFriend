#!/usr/bin/env python3
"""USB backup/restore CLI for FairWeather Friend. Python 3 + pyserial only."""
import argparse
import datetime
import json
import math
import os
from pathlib import Path
import re
import sys
import time

BAUDRATE = 115200
MAX_COMMAND_BYTES = 4096  # Firmware's line limit, excluding terminating newline.
MAX_RESPONSE_BYTES = 65536
XIAO_IDS = {(0x2886, 0x0056), (0x2886, 0x8056)}


class BackupError(Exception):
    """A failure suitable for displaying without a Python traceback."""


def reject_constant(value):
    raise ValueError("Non-JSON numeric value: " + value)


def finite_float(text):
    value = float(text)
    if not math.isfinite(value):
        raise ValueError("Non-finite JSON number: " + text)
    return value


def unique_keys(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("Duplicate JSON key: " + key)
        result[key] = value
    return result


def parse_backup(text):
    """Check JSON locally; firmware remains authoritative for schema/checksum."""
    try:
        data = json.loads(text, parse_constant=reject_constant, parse_float=finite_float, object_pairs_hook=unique_keys)
    except (ValueError, RecursionError) as exc:
        raise BackupError("Invalid backup JSON: " + str(exc)) from exc
    if not isinstance(data, dict):
        raise BackupError("Backup must be a JSON object.")
    if type(data.get("saveVersion")) is not int or data["saveVersion"] < 1:
        raise BackupError("Backup needs a positive integer saveVersion.")
    if not isinstance(data.get("checksum"), str) or not re.fullmatch(r"[0-9a-fA-F]{8}", data["checksum"]):
        raise BackupError("Backup needs its eight-digit hexadecimal checksum.")
    return data


def import_command(path):
    try:
        if path.stat().st_size > MAX_RESPONSE_BYTES:
            raise BackupError("Backup file is too large (maximum 64 KiB).")
        data = parse_backup(path.read_text(encoding="utf-8-sig"))
    except (OSError, UnicodeError) as exc:
        raise BackupError("Cannot read backup: " + str(exc)) from exc
    compact = json.dumps(data, separators=(",", ":"), ensure_ascii=True, allow_nan=False)
    command = ("IMPORT_BUDDY " + compact).encode("utf-8")
    if len(command) > MAX_COMMAND_BYTES:
        raise BackupError("Import exceeds the firmware's 4096-byte command limit.")
    return command


def port_score(port):
    if (port.vid, port.pid) in XIAO_IDS:
        return 3
    label = " ".join(str(getattr(port, name, "") or "") for name in ("description", "manufacturer", "product"))
    if "xiao" in label.lower():
        return 2
    # Native ESP32 USB Serial/JTAG; less specific, so never prefer it over XIAO.
    if port.vid == 0x303A:
        return 1
    return 0


def select_port(ports, override=None):
    if override:
        return override
    # On macOS prefer outgoing /dev/cu.* to duplicate incoming /dev/tty.* nodes.
    ports = [p for p in ports if not (p.device.startswith("/dev/tty.") and
             any(q.device == p.device.replace("/dev/tty.", "/dev/cu.", 1) for q in ports))]
    best = max((port_score(p) for p in ports), default=0)
    matches = [p for p in ports if port_score(p) == best] if best else []
    if len(matches) == 1:
        return matches[0].device
    if matches:
        raise BackupError("Multiple likely buddies: " + ", ".join(p.device for p in matches) + ". Use --port.")
    raise BackupError("No likely XIAO port found. Wake with B, check the USB data cable, run list-ports, then use --port.")


def read_lines(connection, seconds):
    deadline = time.monotonic() + seconds
    pending = bytearray()
    received = 0
    while time.monotonic() < deadline:
        chunk = connection.read(min(connection.in_waiting or 1, 1024))
        received += len(chunk)
        if received > MAX_RESPONSE_BYTES:
            raise BackupError("Device response exceeded 64 KiB.")
        for byte in chunk:
            if byte in (10, 13):
                if pending:
                    try:
                        yield pending.decode("utf-8")
                    except UnicodeError as exc:
                        raise BackupError("Invalid UTF-8 received from device.") from exc
                    pending.clear()
            else:
                pending.append(byte)
    raise BackupError("Timed out. Keep the buddy awake, check the correct port, and close Arduino Serial Monitor. No success was confirmed.")


def send(connection, command):
    wire = command + b"\n"
    if connection.write(wire) != len(wire):
        raise BackupError("Serial write was incomplete; no success was confirmed.")


def device_error(line):
    prefixes = ("ERROR:", "Import rejected:", "Import disabled:", "Import NVS commit failed;",
                "Open SETTINGS > IMPORT BUDDY", "Import confirmation expired;", "Buddy command too long;",
                "Import canceled.", "Cancel import before exporting.")
    if line.startswith(prefixes):
        raise BackupError("Device: " + line)


def receive_export(connection, timeout):
    text = None
    depth = 0
    quoted = escaped = False
    for line in read_lines(connection, timeout):
        device_error(line)
        if text is None:
            if not line.lstrip().startswith("{"):
                continue  # Ignore startup/clock/weather logs before the JSON.
            text = ""
        text += line + "\n"
        for char in line:
            if quoted:
                if escaped:
                    escaped = False
                elif char == "\\":
                    escaped = True
                elif char == '"':
                    quoted = False
            elif char == '"':
                quoted = True
            elif char in "{[":
                depth += 1
            elif char in "}]":
                depth -= 1
        if depth <= 0 and not quoted:
            return parse_backup(text)


def write_backup(data, directory):
    directory.mkdir(parents=True, exist_ok=True)
    timestamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S_%fZ")
    path = directory / ("fairweather_buddy_" + timestamp + ".json")
    # Exclusive creation protects existing backups, including timestamp collisions.
    output = path.open("x", encoding="utf-8", newline="\n")
    try:
        with output:
            json.dump(data, output, indent=2, ensure_ascii=True, allow_nan=False)
            output.write("\n")
    except Exception:
        path.unlink(missing_ok=True)
        raise
    return path


def perform_import(connection, command, timeout):
    send(connection, command)
    staged = False
    for line in read_lines(connection, timeout):
        device_error(line)
        if line.startswith("Import validated;"):
            staged = True
            print("Backup validated by device. Press B on REPLACE CURRENT BUDDY? to commit, or C to cancel.", flush=True)
            print("Confirm promptly: normal 30-second inactivity sleep still applies.", flush=True)
        elif line == "Buddy import committed and verified.":
            if not staged:
                raise BackupError("Received a commit without staging confirmation; result is uncertain.")
            print("Import complete: device confirmed the NVS save.")
            return


def positive_timeout(text):
    value = float(text)
    if not math.isfinite(value) or value <= 0:
        raise argparse.ArgumentTypeError("timeout must be a positive finite number")
    return value


def parser():
    cli = argparse.ArgumentParser(description=__doc__)
    commands = cli.add_subparsers(dest="action", required=True)
    commands.add_parser("list-ports", help="show available serial ports")
    for name in ("export", "import"):
        command = commands.add_parser(name)
        command.add_argument("--port", help="serial port override, e.g. /dev/cu.usbmodem123 or COM5")
        command.add_argument("--timeout", type=positive_timeout, default=20 if name == "export" else 70,
                             help="response timeout in seconds (default: %(default)s)")
        if name == "export":
            command.add_argument("--output-dir", type=Path, default=Path("backups"), help="backup directory (default: backups)")
        else:
            command.add_argument("file", type=Path, help="buddy JSON backup to restore")
    return cli


def main(argv=None):
    args = parser().parse_args(argv)
    connection = None
    restoring = args.action == "import"
    committed = False
    import_sent = False
    try:
        # Local validation happens before opening a port or prompting to send.
        command = import_command(args.file) if restoring else None
        try:
            import serial
            from serial.tools import list_ports
        except ImportError as exc:
            raise BackupError("Install the only dependency: python -m pip install pyserial") from exc
        ports = list(list_ports.comports())
        if args.action == "list-ports":
            if not ports:
                print("No serial ports found. Wake the buddy with B and check its USB data cable.")
            for port in ports:
                label = " [likely buddy]" if port_score(port) else ""
                print(f"{port.device}: {port.description}{label}")
            return 0
        selected = select_port(ports, args.port)
        print(f"Using {selected} at {BAUDRATE} baud. Close other Serial tools and keep the buddy awake.", flush=True)
        connection = serial.Serial(port=None, baudrate=BAUDRATE, timeout=0.1, write_timeout=3,
                                   exclusive=True if os.name == "posix" else None)
        # Native USB CDC needs DTR; leave RTS inactive and never perform a reset
        # sequence or 1200-baud bootloader touch.
        connection.dtr = True
        connection.rts = False
        connection.port = selected
        connection.open()
        if restoring:
            print("On the buddy, open SETTINGS > IMPORT BUDDY before continuing.", flush=True)
            answer = input(f"This will REPLACE the current buddy with {args.file}. Type IMPORT to send: ")
            if answer != "IMPORT":
                print("Import canceled; no backup data sent.")
                return 0
            import_sent = True
            perform_import(connection, command, args.timeout)
            committed = True
        else:
            send(connection, b"EXPORT_BUDDY")
            data = receive_export(connection, args.timeout)
            path = write_backup(data, args.output_dir)
            print(f"Export complete: {path.resolve()}")
        return 0
    except (BackupError, OSError, ValueError, EOFError) as exc:
        print("Failed: " + str(exc), file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("Canceled; no success was confirmed.", file=sys.stderr)
        return 130
    finally:
        if connection is not None and connection.is_open:
            if import_sent and not committed:
                try:
                    send(connection, b"CANCEL_IMPORT")
                except (OSError, ValueError):
                    pass
            try:
                connection.close()
            except OSError:
                pass


if __name__ == "__main__":
    sys.exit(main())
