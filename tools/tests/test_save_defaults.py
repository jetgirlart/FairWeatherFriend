"""Regression for firmware constants being mistaken for missing macros."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(shutil.which("c++"), "C++ compiler required for defaults regression")
class SaveDefaultsTests(unittest.TestCase):
    def test_firmware_constants_and_host_defaults(self):
        source = (ROOT / "altoids-pet/firmware/altoids_pet/save.cpp").read_text()
        # Exercise the actual configuration prologue without Arduino/NVS libraries.
        prologue = source.split('#include "gear.h"', 1)[0]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for header in ["save.h", "achievements.h", "field_notes.h"]:
                (root / header).write_text("")
            (root / "config.h").write_text(
                "const float LATITUDE = 32.664021;\nconst float LONGITUDE = -95.487892;\n")
            for defines, check in [
                (["-DARDUINO=100"], "LATITUDE > 32 && LATITUDE < 33 && LONGITUDE > -96 && LONGITUDE < -95"),
                ([], "LATITUDE == 0 && LONGITUDE == 0"),
                (["-DLATITUDE=41.5", "-DLONGITUDE=-87.5"], "LATITUDE == 41.5 && LONGITUDE == -87.5"),
            ]:
                with self.subTest(defines=defines):
                    (root / "check.cpp").write_text(prologue + "\nint main(){return (" + check + ") ? 0 : 1;}\n")
                    subprocess.run([shutil.which("c++"), "-std=c++11", *defines,
                                    str(root / "check.cpp"), "-o", str(root / "check")],
                                   check=True, capture_output=True)
                    subprocess.run([str(root / "check")], check=True, capture_output=True)
