"""Exercise installed pyserial against a virtual device, never physical USB."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import unittest


@unittest.skipUnless(os.name == "posix" and importlib.util.find_spec("serial"), "requires POSIX PTY and pyserial")
class SerialIntegrationTests(unittest.TestCase):
    def test_export_with_real_pyserial(self):
        import pty
        master, slave = pty.openpty()
        device = os.ttyname(slave)
        commands = []
        errors = []
        payload = {"saveVersion": 4, "checksum": "abcdef12", "totalObservations": 7}

        def emulate():
            try:
                wire = b""
                while not wire.endswith(b"\n"):
                    wire += os.read(master, 1024)
                commands.append(wire)
                os.write(master, b"Weather cached.\r\n")
                for line in json.dumps(payload, indent=2).splitlines():
                    os.write(master, (line + "\r\n").encode())
            except OSError as exc:
                errors.append(exc)

        worker = threading.Thread(target=emulate, daemon=True)
        worker.start()
        try:
            with tempfile.TemporaryDirectory() as directory:
                result = subprocess.run([sys.executable, str(Path(__file__).parents[1] / "fairweather_backup.py"),
                                         "export", "--port", device, "--output-dir", directory, "--timeout", "3"],
                                        capture_output=True, text=True, timeout=8)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(commands, [b"EXPORT_BUDDY\n"])
                files = list(Path(directory).glob("*.json"))
                self.assertEqual(len(files), 1)
                self.assertEqual(json.loads(files[0].read_text()), payload)
                self.assertIn("Export complete", result.stdout)
                self.assertEqual(errors, [])
        finally:
            os.close(master)
            os.close(slave)
            worker.join(timeout=1)


if __name__ == "__main__":
    unittest.main()
