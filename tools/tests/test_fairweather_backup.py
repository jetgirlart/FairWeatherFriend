"""Host-only tests: no device is opened, no firmware is uploaded."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import types
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("backup", Path(__file__).parents[1] / "fairweather_backup.py")
backup = importlib.util.module_from_spec(spec)
spec.loader.exec_module(backup)
DATA = {"saveVersion": 4, "checksum": "1234abcd", "equippedSlots": {"HEAD": 1},
        "description": 'brace } and quote " and slash \\'}


def port(device, vid=None, pid=None, description="serial"):
    return types.SimpleNamespace(device=device, vid=vid, pid=pid, description=description)


class FakeSerial:
    def __init__(self, reply=b"", step=5):
        self.reply = bytearray(reply)
        self.step = step
        self.writes = []
        self.is_open = False
        self.port = None

    @property
    def in_waiting(self):
        return len(self.reply)

    def read(self, length):
        result = bytes(self.reply[:min(self.step, length)])
        del self.reply[:len(result)]
        return result

    def write(self, wire):
        self.writes.append(wire)
        return len(wire)

    def open(self):
        self.is_open = True

    def close(self):
        self.is_open = False


class BackupTests(unittest.TestCase):
    def test_field_note_backup_and_expanded_command_limit(self):
        data = dict(DATA, saveVersion=9, fieldNoteNext=0, fieldNotes=[{
            "timestamp": 1800000000 - age * 3600, "temperatureMilliC": 22000,
            "weatherCode": 61, "category": 4, "location": 0, "validMetrics": 31,
            "metrics": {"humidityCentiPercent": 8100, "windCentiKmh": 2000,
                        "gustCentiKmh": 4000, "pressureCentiHpa": 101300,
                        "precipitationCentiMm": 200}, "outcomes": 1, "severeEvents": 0
        } for age in range(16)], padding="x" * 9000)
        wire = json.dumps(data, indent=2).encode() + b"\n"
        self.assertGreater(len(wire), 12288)
        self.assertEqual(backup.receive_export(FakeSerial(wire, step=128), 1), data)
        with tempfile.TemporaryDirectory() as directory:
            file = backup.write_backup(data, Path(directory))
            command = backup.import_command(file)
            self.assertGreater(len(command), 12301)
            self.assertLess(len(command), backup.MAX_COMMAND_BYTES)
            self.assertEqual(json.loads(command[len(b"IMPORT_BUDDY "):]), data)

    def test_ports_exact_fallback_override_ambiguous(self):
        xiao = port("/dev/cu.usbmodem1", 0x2886, 0x8056)
        esp = port("COM7", 0x303A, 0x1001)
        self.assertEqual(backup.select_port([esp, xiao]), xiao.device)
        self.assertEqual(backup.select_port([esp]), "COM7")
        self.assertEqual(backup.select_port([], "COM9"), "COM9")
        duplicate = port("/dev/tty.usbmodem1", 0x2886, 0x8056)
        self.assertEqual(backup.select_port([duplicate, xiao]), xiao.device)
        for ports in ([], [port("COM1")], [xiao, port("COM4", 0x2886, 0x0056)]):
            with self.assertRaises(backup.BackupError):
                backup.select_port(ports)

    def test_strict_json_local_validation(self):
        self.assertEqual(backup.parse_backup(json.dumps(DATA)), DATA)
        for text in ('[]', '{}', '{bad}', '{"saveVersion":true,"checksum":"1234abcd"}',
                     '{"saveVersion":4,"checksum":"wrong"}',
                     '{"saveVersion":4,"saveVersion":3,"checksum":"1234abcd"}',
                     '{"saveVersion":4,"checksum":"1234abcd","bad":NaN}',
                     '{"saveVersion":4,"checksum":"1234abcd","bad":1e999}'):
            with self.assertRaises(backup.BackupError):
                backup.parse_backup(text)

    def test_export_fragmented_json_with_logs_and_line_endings(self):
        for ending in ("\n", "\r", "\r\n"):
            payload = ("Weather cached." + ending + json.dumps(DATA, indent=2).replace("\n", ending) + ending).encode()
            self.assertEqual(backup.receive_export(FakeSerial(payload, step=1), 1), DATA)
        with self.assertRaises(backup.BackupError):
            backup.receive_export(FakeSerial(b"ERROR: Buddy export unavailable\n"), 1)
        with self.assertRaises(backup.BackupError):
            backup.receive_export(FakeSerial(b"{bad}\n"), 1)
        with self.assertRaises(backup.BackupError):
            backup.receive_export(FakeSerial(b'{"saveVersion":4,\n'), 0.01)

    def test_file_format_collision_and_command_limit(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            file = backup.write_backup(DATA, root)
            self.assertRegex(file.name, r"fairweather_buddy_\d{8}T\d{6}_\d{6}Z\.json")
            self.assertEqual(json.loads(file.read_text()), DATA)
            command = backup.import_command(file)
            self.assertTrue(command.startswith(b"IMPORT_BUDDY "))
            self.assertNotIn(b"\n", command)
            self.assertEqual(json.loads(command[13:]), DATA)
            large = dict(DATA, note="x" * backup.MAX_COMMAND_BYTES)
            file.write_text(json.dumps(large))
            with self.assertRaises(backup.BackupError):
                backup.import_command(file)
            with patch.object(backup.datetime, "datetime") as clock:
                clock.now.return_value.strftime.return_value = "same"
                first = backup.write_backup(DATA, root)
                with self.assertRaises(FileExistsError):
                    backup.write_backup(DATA, root)
                self.assertEqual(json.loads(first.read_text()), DATA)

    def test_import_waits_for_device_commit_and_reports_failures(self):
        success = b"Import validated; press B\nBuddy import committed and verified.\n"
        connection = FakeSerial(success)
        with contextlib.redirect_stdout(io.StringIO()) as output:
            backup.perform_import(connection, b"IMPORT_BUDDY {}", 1)
        self.assertIn("Press B", output.getvalue())
        self.assertIn("Import complete", output.getvalue())
        self.assertEqual(connection.writes, [b"IMPORT_BUDDY {}\n"])
        for response in (b"Import rejected: checksum\n", b"Import canceled.\n",
                         b"Import confirmation expired; buddy preserved\n",
                         b"Open SETTINGS > IMPORT BUDDY, then resend\n",
                         b"Import NVS commit failed; buddy preserved\n",
                         b"Buddy import committed and verified.\n"):
            with self.assertRaises(backup.BackupError):
                backup.perform_import(FakeSerial(response), b"IMPORT_BUDDY {}", 1)
        with contextlib.redirect_stdout(io.StringIO()), self.assertRaises(backup.BackupError):
            backup.perform_import(FakeSerial(b"Import validated; press B\n"), b"IMPORT_BUDDY {}", 0.01)

    def run_cli(self, args, connection, answer="IMPORT"):
        # Supply the sole dependency as a fake; tests require only the stdlib.
        serial = types.ModuleType("serial")
        tools = types.ModuleType("serial.tools")
        ports = types.ModuleType("serial.tools.list_ports")
        serial.Serial = lambda **kwargs: connection
        ports.comports = lambda: [port("COM5", 0x2886, 0x8056)]
        tools.list_ports = ports
        with patch.dict("sys.modules", {"serial": serial, "serial.tools": tools, "serial.tools.list_ports": ports}), \
             patch("builtins.input", return_value=answer), \
             contextlib.redirect_stdout(io.StringIO()) as out, contextlib.redirect_stderr(io.StringIO()) as err:
            status = backup.main(args)
        return status, out.getvalue(), err.getvalue()

    def test_cli_export_import_host_confirmation_and_cleanup(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            connection = FakeSerial((json.dumps(DATA) + "\n").encode())
            status, out, _ = self.run_cli(["export", "--output-dir", directory], connection)
            self.assertEqual(status, 0)
            self.assertIn("Export complete", out)
            self.assertEqual(connection.writes, [b"EXPORT_BUDDY\n"])
            self.assertFalse(connection.rts)
            self.assertTrue(connection.dtr)
            self.assertFalse(connection.is_open)
            file = next(root.glob("*.json"))
            for answer in ("no", "import", ""):
                connection = FakeSerial()
                status, _, _ = self.run_cli(["import", str(file)], connection, answer)
                self.assertEqual(status, 0)
                self.assertEqual(connection.writes, [])
            connection = FakeSerial(b"Import validated; press B\nBuddy import committed and verified.\n")
            status, _, _ = self.run_cli(["import", str(file)], connection)
            self.assertEqual(status, 0)
            self.assertEqual(len(connection.writes), 1)
            self.assertNotIn(b"CONFIRM_IMPORT", connection.writes[0])
            connection = FakeSerial(b"Import rejected: checksum\n")
            status, _, err = self.run_cli(["import", str(file)], connection)
            self.assertEqual(status, 1)
            self.assertIn("checksum", err)
            self.assertEqual(connection.writes[-1], b"CANCEL_IMPORT\n")
            file.write_text("invalid")
            connection = FakeSerial()
            status, _, _ = self.run_cli(["import", str(file)], connection)
            self.assertEqual(status, 1)
            self.assertIsNone(connection.port)
            self.assertEqual(connection.writes, [])

    def test_cli_list_ports_missing_dependency_and_connection_error(self):
        status, out, _ = self.run_cli(["list-ports"], FakeSerial())
        self.assertEqual(status, 0)
        self.assertIn("COM5", out)
        self.assertIn("likely buddy", out)
        with patch.dict("sys.modules", {"serial": None}), contextlib.redirect_stderr(io.StringIO()) as err:
            status = backup.main(["export"])
        self.assertEqual(status, 1)
        self.assertIn("pip install pyserial", err.getvalue())
        connection = FakeSerial()
        def fail_open():
            raise OSError("port busy")
        connection.open = fail_open
        status, _, err = self.run_cli(["export"], connection)
        self.assertEqual(status, 1)
        self.assertIn("port busy", err)
        self.assertEqual(connection.writes, [])

    def test_response_bound_and_partial_write(self):
        with self.assertRaises(backup.BackupError):
            backup.receive_export(FakeSerial(b"x" * (backup.MAX_RESPONSE_BYTES + 1), step=1024), 1)
        connection = FakeSerial()
        connection.write = lambda wire: 0
        with self.assertRaises(backup.BackupError):
            backup.send(connection, b"EXPORT_BUDDY")

    def test_location_commands_validation_and_cli(self):
        cases = [
            (["locations", "set", "1", "FIELD CAMP", "41.5", "-87.5"], b"UPSERT_LOCATION 1 41.500000 -87.500000 FIELD CAMP\n"),
            (["locations", "active", "1"], b"ACTIVE_LOCATION 1\n"),
            (["locations", "delete", "1", "--replacement", "0"], b"DELETE_LOCATION 1 0\n"),
        ]
        for args, wire in cases:
            connection = FakeSerial(b"LOCATION_OK saved\n")
            status, out, _ = self.run_cli(args, connection)
            self.assertEqual(status, 0)
            self.assertEqual(connection.writes, [wire])
            self.assertIn("Location complete", out)
            self.assertFalse(connection.is_open)
        for args in (
            ["locations", "set", "8", "CAMP", "0", "0"],
            ["locations", "set", "1", "x" * 16, "0", "0"],
            ["locations", "set", "1", " NEW", "0", "0"],
            ["locations", "set", "1", "CAMP\n", "0", "0"],
            ["locations", "set", "1", "CAMP", "nan", "0"],
            ["locations", "set", "1", "CAMP", "91", "0"],
            ["locations", "set", "1", "CAMP", "0", "181"],
            ["locations", "delete", "1", "--replacement", "1"],
        ):
            connection = FakeSerial()
            status, _, _ = self.run_cli(args, connection)
            self.assertEqual(status, 1)
            self.assertIsNone(connection.port)
            self.assertEqual(connection.writes, [])
        connection = FakeSerial(b"LOCATION_ERROR active location needs replacement\n")
        status, _, err = self.run_cli(["locations", "delete", "1"], connection)
        self.assertEqual(status, 1)
        self.assertIn("replacement", err)
        data = {"activeLocation": 1, "fieldSitesVisited": 2, "locations": [
            {"id": 1, "name": "FIELD CAMP", "latitude": 41.5, "longitude": -87.5, "visited": True}]}
        connection = FakeSerial(("Clock restored\nLOCATIONS " + json.dumps(data) + "\n").encode())
        status, out, _ = self.run_cli(["locations", "list"], connection)
        self.assertEqual(status, 0)
        self.assertEqual(connection.writes, [b"LIST_LOCATIONS\n"])
        self.assertIn("* ACTIVE", out)
        self.assertIn("FIELD SITES VISITED: 2", out)


if __name__ == "__main__":
    unittest.main()
