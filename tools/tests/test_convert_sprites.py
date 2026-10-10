"""Source PNG validation and generation tests; no device or image dependency."""
import contextlib
import importlib.util
import io
from pathlib import Path
import struct
import tempfile
import unittest
import zlib

spec = importlib.util.spec_from_file_location("sprites", Path(__file__).parents[1] / "convert_sprites.py")
sprites = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sprites)
ROOT = Path(__file__).resolve().parents[2] / "altoids-pet"


def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)


def png(pixels, width=48, height=48, depth=8, color=6, interlace=0, methods=(0,)):
    raw = bytearray()
    passes = sprites.ADAM7 if interlace else ((0, 0, 1, 1),)
    for start_x, start_y, dx, dy in passes:
        xs, ys = list(range(start_x, width, dx)), list(range(start_y, height, dy))
        if not xs or not ys:
            continue
        previous = bytes(len(xs) * 4)
        for y in ys:
            row = bytes(value for x in xs for value in pixels[y * width + x])
            method = methods[y % len(methods)]
            encoded = bytearray()
            for i, value in enumerate(row):
                a, b, c = (row[i - 4] if i >= 4 else 0), previous[i], (previous[i - 4] if i >= 4 else 0)
                predictor = (0, a, b, (a + b) // 2, sprites.paeth(a, b, c))[method] if method <= 4 else 0
                encoded.append((value - predictor) & 255)
            raw.extend(bytes([method]) + encoded)
            previous = row
    header = struct.pack(">IIBBBBB", width, height, depth, color, 0, 0, interlace)
    return sprites.PNG_SIGNATURE + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b"")


class SpriteTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.project = Path(self.directory.name)
        (self.project / "assets/buddy").mkdir(parents=True)
        (self.project / "assets/gear").mkdir(parents=True)
        self.idle = self.project / "assets/buddy/kitsune_idle.png"
        self.samples = [(3, 7, 99, 0)] + [rgb + (255,) for rgb in sprites.COLORS]
        self.pixels = [self.samples[i % 5] for i in range(2304)]
        self.idle.write_bytes(png(self.pixels))

    def test_exact_roles_transparency_and_packing(self):
        decoded = sprites.decode_png(self.idle.read_bytes())
        self.assertEqual(decoded, self.pixels)
        roles = sprites.palette_roles(decoded)
        self.assertEqual(roles, [i % 5 for i in range(2304)])
        packed = sprites.pack_roles(roles)
        self.assertEqual(len(packed), 1152)
        self.assertEqual(packed[:5], bytes([0x01, 0x23, 0x40, 0x12, 0x34]))

    def test_all_filters_and_adam7(self):
        for interlace in (0, 1):
            for methods in [(0,), (1,), (2,), (3,), (4,), (0, 1, 2, 3, 4)]:
                with self.subTest(interlace=interlace, methods=methods):
                    self.assertEqual(sprites.decode_png(png(self.pixels, interlace=interlace, methods=methods)), self.pixels)

    def test_dimensions_and_rgba_format(self):
        for width, height in [(47, 48), (48, 47), (49, 48), (24, 24)]:
            with self.assertRaisesRegex(sprites.AssetError, "exactly 48x48"):
                sprites.decode_png(png([self.samples[0]] * width * height, width=width, height=height))
        for depth, color in [(8, 2), (8, 3), (8, 0), (16, 6)]:
            with self.assertRaisesRegex(sprites.AssetError, "8-bit RGBA"):
                sprites.decode_png(png(self.pixels, depth=depth, color=color))

    def test_palette_and_alpha_fail_loudly_with_coordinate(self):
        for sample in [(255, 0, 254, 255), (0, 0, 0, 255), (26, 26, 27, 255)]:
            with self.assertRaisesRegex(sprites.AssetError, r"Unsupported color.*\(0,0\)"):
                sprites.palette_roles([sample] + self.pixels[1:])
        for alpha in (1, 127, 254):
            with self.assertRaisesRegex(sprites.AssetError, "Unexpected alpha"):
                sprites.palette_roles([(255, 0, 255, alpha)] + self.pixels[1:])
        self.assertEqual(sprites.palette_roles([(255, 255, 255, 0)] * 2304), [0] * 2304)

    def test_bad_png_checksum_stream_filter_and_animation(self):
        blob = self.idle.read_bytes()
        for malformed in [b"not png", blob[:-1], blob + b"trailing", blob[:25] + bytes([blob[25] ^ 1]) + blob[26:]]:
            with self.assertRaises(sprites.AssetError):
                sprites.decode_png(malformed)
        animated = blob[:33] + chunk(b"acTL", struct.pack(">II", 2, 0)) + blob[33:]
        with self.assertRaisesRegex(sprites.AssetError, "Animated"):
            sprites.decode_png(animated)
        with self.assertRaisesRegex(sprites.AssetError, "filter"):
            sprites.decode_png(png(self.pixels, methods=(5,)))
        for raw in [b"", b"\0" * (48 * 193 + 1), b"\0" * (48 * 193 - 1)]:
            bad = blob[:33] + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b"")
            with self.assertRaisesRegex(sprites.AssetError, "stream"):
                sprites.decode_png(bad)

    def test_deterministic_outputs_no_source_edits_and_check(self):
        before = self.idle.read_bytes()
        first = sprites.convert(self.project)
        directory = self.project / "firmware/altoids_pet/generated"
        mtimes = [p.stat().st_mtime_ns for p in sorted(directory.iterdir())]
        second = sprites.convert(self.project)
        self.assertEqual(first, second)
        self.assertEqual(mtimes, [p.stat().st_mtime_ns for p in sorted(directory.iterdir())])
        self.assertEqual(self.idle.read_bytes(), before)
        self.assertIn(b"DO NOT EDIT", first["buddy_assets.inc"])
        self.assertEqual(first, sprites.convert(self.project, check=True))
        (directory / "buddy_assets.inc").write_text("stale")
        with self.assertRaisesRegex(sprites.AssetError, "stale"):
            sprites.convert(self.project, check=True)
        self.assertEqual((directory / "buddy_assets.inc").read_text(), "stale")

    def test_optional_assets_fallback_and_required_idle(self):
        outputs = sprites.generate(self.project)
        self.assertIn(b"FWF_PNG_HAS_KITSUNE_IDLE", outputs["buddy_assets.inc"])
        self.assertNotIn(b"FWF_PNG_HAS_KITSUNE_BLINK", outputs["buddy_assets.inc"])
        blink = self.project / "assets/buddy/kitsune_blink.png"
        blink.write_bytes(png(self.pixels))
        self.assertIn(b"FWF_PNG_HAS_KITSUNE_BLINK", sprites.generate(self.project)["buddy_assets.inc"])
        self.idle.unlink()
        with self.assertRaisesRegex(sprites.AssetError, "required source is missing"):
            sprites.generate(self.project)

    def test_invalid_optional_leaves_previous_output_intact(self):
        sprites.convert(self.project)
        destination = self.project / "firmware/altoids_pet/generated/buddy_assets.inc"
        before = destination.read_bytes()
        (self.project / "assets/buddy/kitsune_sleep.png").write_bytes(b"bad png")
        with self.assertRaisesRegex(sprites.AssetError, "kitsune_sleep.png"):
            sprites.convert(self.project)
        self.assertEqual(destination.read_bytes(), before)

    def test_gear_uses_identical_roles_without_substitute_art(self):
        path = self.project / "assets/gear/field_cap.png"
        path.write_bytes(png(self.pixels))
        gear = sprites.generate(self.project)["gear_assets.h"]
        self.assertIn(b"#define FWF_PNG_HAS_GEAR_FIELD_CAP 1", gear)
        self.assertNotIn(b"FWF_PNG_HAS_GEAR_BOOTS", gear)
        self.assertIn(b"PNG_GEAR_FIELD_CAP_ROLES[1152]", gear)
        self.assertIn(b"0x01, 0x23, 0x40", gear)
        self.assertNotIn(b"PNG_GEAR_BOOTS_ROLES", gear)

    def test_current_source_and_checked_in_art_match(self):
        sources = list((ROOT / "assets/buddy").glob("*.png")) + list((ROOT / "assets/gear").glob("*.png"))
        before = {p: p.read_bytes() for p in sources}
        sprites.convert(ROOT, check=True)
        self.assertEqual(before, {p: p.read_bytes() for p in sources})
        self.assertEqual(len(sprites.pack_roles(sprites.palette_roles(sprites.decode_png(before[ROOT / "assets/buddy/kitsune_idle.png"])))), 1152)

    def test_cli_failure_is_nonzero_and_named(self):
        self.idle.write_bytes(b"bad png")
        with contextlib.redirect_stderr(io.StringIO()) as output:
            self.assertEqual(sprites.main(["--project-root", str(self.project)]), 1)
        self.assertIn("Sprite conversion FAILED", output.getvalue())
        self.assertIn("kitsune_idle.png", output.getvalue())


if __name__ == "__main__":
    unittest.main()
