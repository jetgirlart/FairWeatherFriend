#!/usr/bin/env python3
"""Strict, deterministic RGBA PNG -> firmware palette roles (48x48 sprites, 240x240 scenes). Standard library only."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
import zlib

SIZE = 48
FRAME_BYTES = SIZE * SIZE // 2
COLORS = {(0x1A, 0x1A, 0x1A): 1, (255, 0, 255): 2,
          (0, 255, 255): 3, (0, 255, 0): 4}
# Stable registry; only IDLE is required. All others retain embedded fallbacks.
BUDDY = ("idle", "blink", "look_left", "look_right", "happy", "excited",
         "sleepy", "sleep", "bounce", "look_up", "focus")
GEAR = ("field_cap", "sunglasses", "umbrella", "raincoat", "winter_scarf", "winter_coat", "boots")
GEAR_ALIASES = {"winter_scarf": "scarf", "boots": "bootsf"}
BACKGROUND_SIZE = 240
BACKGROUNDS = ("environment", "foreground", "clouds_light", "clouds_heavy", "storm_clouds")
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
ADAM7 = ((0, 0, 8, 8), (4, 0, 8, 8), (0, 4, 4, 8), (2, 0, 4, 4),
         (0, 2, 2, 4), (1, 0, 2, 2), (0, 1, 1, 2))


class AssetError(ValueError):
    pass


def paeth(a, b, c):
    p = a + b - c
    distances = (abs(p - a), abs(p - b), abs(p - c))
    return (a, b, c)[distances.index(min(distances))]


def decode_png(blob, size=SIZE):
    """Decode raw 8-bit RGBA samples, without gamma conversion or color approximation.

    Supports all five PNG row filters and both plain and Adam7 PNGs. Palette,
    grayscale, RGB-only, 16-bit and animated PNGs intentionally fail validation.
    """
    if len(blob) > 1024 * 1024 or not blob.startswith(PNG_SIGNATURE):
        raise AssetError("Expected a PNG file, at most 1 MiB")
    position, header, compressed = 8, None, bytearray()
    have_data = data_ended = ended = False
    while position < len(blob):
        if position + 12 > len(blob):
            raise AssetError("Truncated PNG chunk")
        length, kind = struct.unpack_from(">I4s", blob, position)
        stop = position + 12 + length
        if stop > len(blob):
            raise AssetError("Truncated PNG chunk")
        data = blob[position + 8:stop - 4]
        crc = struct.unpack_from(">I", blob, stop - 4)[0]
        if zlib.crc32(kind + data) & 0xFFFFFFFF != crc:
            raise AssetError("PNG chunk checksum mismatch")
        if not all(65 <= c <= 90 or 97 <= c <= 122 for c in kind) or kind[2] & 32:
            raise AssetError("Invalid PNG chunk type")
        if header is None and kind != b"IHDR":
            raise AssetError("IHDR must be the first PNG chunk")
        if kind == b"IHDR":
            if header is not None or length != 13:
                raise AssetError("Invalid or duplicate IHDR")
            header = struct.unpack(">IIBBBBB", data)
            width, height, depth, color, compression, filtering, interlace = header
            if (width, height) != (size, size):
                raise AssetError(f"Dimensions must be exactly {size}x{size}, got {width}x{height}")
            if depth != 8 or color != 6 or compression or filtering or interlace not in (0, 1):
                raise AssetError("Expected an 8-bit RGBA PNG (color type 6)")
        elif kind == b"IDAT":
            if data_ended:
                raise AssetError("IDAT chunks must be consecutive")
            have_data = True
            compressed.extend(data)
        elif kind == b"IEND":
            if length or not have_data or stop != len(blob):
                raise AssetError("Invalid IEND or trailing PNG data")
            ended = True
            break
        else:
            if have_data:
                data_ended = True
            if kind in (b"acTL", b"fcTL", b"fdAT", b"tRNS"):
                raise AssetError("Animated PNGs and additional transparency chunks are unsupported")
            if kind == b"PLTE":
                if have_data or not length or length % 3 or length > 768:
                    raise AssetError("Invalid PLTE chunk")
            elif not kind[0] & 32:
                raise AssetError(f"Unsupported critical PNG chunk {kind!r}")
        position = stop
    if not ended or header is None:
        raise AssetError("Missing PNG data/IEND")
    passes = ADAM7 if header[-1] else ((0, 0, 1, 1),)
    lengths = [(len(range(x, size, dx)), len(range(y, size, dy))) for x, y, dx, dy in passes]
    expected = sum(h * (1 + w * 4) for w, h in lengths if w and h)
    try:
        decoder = zlib.decompressobj()
        raw = decoder.decompress(bytes(compressed), expected + 1)
    except zlib.error as exc:
        raise AssetError("Invalid PNG compressed pixels") from exc
    if len(raw) != expected or not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
        raise AssetError("PNG pixel stream length/trailing data mismatch")
    pixels, offset = [None] * (size * size), 0
    for (start_x, start_y, dx, dy), (width, height) in zip(passes, lengths):
        if not width or not height:
            continue
        previous = bytearray(width * 4)
        for row_index in range(height):
            method = raw[offset]
            row = bytearray(raw[offset + 1:offset + 1 + width * 4])
            offset += 1 + width * 4
            if method > 4:
                raise AssetError(f"Invalid PNG row filter {method}")
            for i in range(len(row)):
                a, b, c = (row[i - 4] if i >= 4 else 0), previous[i], (previous[i - 4] if i >= 4 else 0)
                row[i] = (row[i] + (0, a, b, (a + b) // 2, paeth(a, b, c))[method]) & 255
            y = start_y + row_index * dy
            for x in range(width):
                pixels[y * size + start_x + x * dx] = tuple(row[x * 4:x * 4 + 4])
            previous = row
    return pixels


def palette_roles(pixels, size=SIZE):
    roles = []
    for i, (r, g, b, alpha) in enumerate(pixels):
        x, y = i % size, i // size
        if alpha == 0:
            roles.append(0)  # Hidden RGB values remain irrelevant, and are never rewritten.
        elif alpha != 255:
            raise AssetError(f"Unexpected alpha {alpha} at ({x},{y}); only 0 or 255 allowed")
        elif (r, g, b) not in COLORS:
            raise AssetError(f"Unsupported color #{r:02X}{g:02X}{b:02X} at ({x},{y})")
        else:
            roles.append(COLORS[(r, g, b)])
    return roles


def pack_roles(roles, size=SIZE):
    if len(roles) != size * size or any(role not in range(5) for role in roles):
        raise AssetError(f"Expected exactly {size * size} palette roles in range 0..4")
    return bytes(roles[i] << 4 | roles[i + 1] for i in range(0, len(roles), 2))


def read_roles(path, label):
    try:
        source = path.read_bytes()
        return source, palette_roles(decode_png(source))
    except (OSError, AssetError) as exc:
        raise AssetError(f"{label}: {exc}") from exc


def compose_layer(base, overlay, clear_regions):
    roles = list(base)
    for region in clear_regions:
        if (not isinstance(region, list) or len(region) != 5 or
                any(type(v) is not int for v in region)):
            raise AssetError("Layer clearRegions must contain [x,y,width,height,role]")
        x, y, width, height, role = region
        if x < 0 or y < 0 or width < 1 or height < 1 or x + width > SIZE or y + height > SIZE or role not in range(5):
            raise AssetError("Layer clearRegions out of bounds or invalid role")
        for row in range(y, y + height):
            roles[row * SIZE + x:row * SIZE + x + width] = [role] * width
    return [pixel if pixel else roles[i] for i, pixel in enumerate(overlay)]


def layer_manifest(project):
    path = project / "assets/buddy/layers.json"
    if not path.exists():
        return {}
    try:
        layers = json.loads(path.read_text())
    except (OSError, ValueError) as exc:
        raise AssetError(f"{path}: invalid layer manifest: {exc}") from exc
    if not isinstance(layers, dict):
        raise AssetError("Layer manifest must be an object")
    for name, config in layers.items():
        if name not in BUDDY or name == "idle" or not isinstance(config, dict) or set(config) - {"clearRegions"}:
            raise AssetError(f"Invalid buddy layer definition: {name}")
        if not isinstance(config.get("clearRegions", []), list):
            raise AssetError(f"Invalid clearRegions for {name}")
        compose_layer([0] * (SIZE * SIZE), [0] * (SIZE * SIZE), config.get("clearRegions", []))
    return layers


def asset_array(path, label, symbol, gear=False, base=None, clear_regions=()):
    source, roles = read_roles(path, label)
    if base is not None:
        roles = compose_layer(base, roles, clear_regions)
    packed = pack_roles(roles)
    text = f"// Source: {label}\n// SHA256: {hashlib.sha256(source).hexdigest()}\n"
    if base is not None:
        text += "// Composed over kitsune_idle.png per assets/buddy/layers.json.\n"
    text += f"{'static ' if gear else ''}const uint8_t {symbol}[{FRAME_BYTES}] PROGMEM = {{\n"
    for start in range(0, len(packed), SIZE // 2):
        text += "  " + ", ".join(f"0x{v:02X}" for v in packed[start:start + SIZE // 2]) + ",\n"
    return text + "};\n\n"


def background_assets(project):
    text = "// GENERATED by tools/convert_sprites.py. DO NOT EDIT.\n"
    text += "// Authoritative 240x240 source PNGs in assets/backgrounds/.\n"
    text += "// 0=transparent, 1=outline, 2=primary, 3=secondary, 4=detail.\n"
    text += "// Row-major, two roles/byte, high nibble first; no cropping or scaling.\n"
    text += '#pragma once\n#include <Arduino.h>\n\n'
    for name in BACKGROUNDS:
        path = project / f"assets/backgrounds/{name}.png"
        if name == "environment":
            alias = project / "assets/backgrounds/background.png"
            if path.exists() and alias.exists():
                raise AssetError("Ambiguous background sources: environment.png and background.png")
            if alias.exists():
                path = alias
        label = path.relative_to(project).as_posix()
        if not path.exists():
            raise AssetError(f"{label}: required background source is missing")
        try:
            source = path.read_bytes()
            roles = palette_roles(decode_png(source, BACKGROUND_SIZE), BACKGROUND_SIZE)
        except (OSError, AssetError) as exc:
            raise AssetError(f"{label}: {exc}") from exc
        data = pack_roles(roles, BACKGROUND_SIZE)
        text += f"// Source: {label}\n// SHA256: {hashlib.sha256(source).hexdigest()}\n"
        text += f"static const uint8_t BACKGROUND_{name.upper()}[{len(data)}] PROGMEM = {{\n"
        for start in range(0, len(data), 24):
            text += "  " + ", ".join(f"0x{value:02X}" for value in data[start:start + 24]) + ",\n"
        text += "};\n\n"
    return text.encode("utf-8")


def generate(project):
    """Validate every registered source before touching any output file."""
    banner = "// GENERATED by tools/convert_sprites.py. DO NOT EDIT.\n// Authoritative sources: assets/buddy/ and assets/gear/.\n// 0=transparent, 1=outline, 2=primary, 3=secondary/accent, 4=detail.\n// Row-major, two 4-bit roles/byte; left pixel is the high nibble.\n"
    buddy = banner + '#pragma once\n#include "../sprites.h"\n\n'
    gear = banner + '#pragma once\n#include "../sprites.h"\n\n// Optional palette layers; presence flags select PNGs over embedded fallbacks.\n'
    layers = layer_manifest(project)
    idle_path = project / "assets/buddy/kitsune_idle.png"
    if not idle_path.exists():
        raise AssetError("assets/buddy/kitsune_idle.png: required source is missing")
    _, idle_roles = read_roles(idle_path, "assets/buddy/kitsune_idle.png")
    for name in BUDDY:
        label = f"assets/buddy/kitsune_{name}.png"
        path = project / label
        if not path.exists():
            if name == "idle":
                raise AssetError(f"{label}: required source is missing")
            continue
        symbol = "KITSUNE_" + name.upper()
        buddy += f"#define FWF_PNG_HAS_{symbol} 1\n"
        buddy += asset_array(path, label, symbol, base=idle_roles if name in layers else None,
                             clear_regions=layers.get(name, {}).get("clearRegions", []))
    for name in GEAR:
        label = f"assets/gear/{name}.png"
        path = project / label
        alias = project / f"assets/gear/{GEAR_ALIASES[name]}.png" if name in GEAR_ALIASES else None
        if alias is not None and alias.exists():
            if path.exists():
                raise AssetError(f"Ambiguous gear sources: {path.name} and {alias.name}; keep only one")
            path = alias
            label = f"assets/gear/{alias.name}"
        if path.exists():
            gear += f"#define FWF_PNG_HAS_GEAR_{name.upper()} 1\n"
            gear += asset_array(path, label, "PNG_GEAR_" + name.upper() + "_ROLES", gear=True)
    return {"buddy_assets.inc": buddy.encode("utf-8"), "gear_assets.h": gear.encode("utf-8"),
            "background_assets.h": background_assets(project)}


def convert(project, check=False):
    outputs = generate(project)
    directory = project / "firmware/altoids_pet/generated"
    stale = []
    for name, content in outputs.items():
        destination = directory / name
        if destination.exists() and destination.read_bytes() == content:
            continue
        stale.append(name)
        if check:
            continue
        directory.mkdir(parents=True, exist_ok=True)
        temporary = None
        try:
            with tempfile.NamedTemporaryFile(dir=directory, delete=False) as stream:
                temporary = Path(stream.name)
                stream.write(content)
            os.replace(temporary, destination)
        finally:
            if temporary and temporary.exists():
                temporary.unlink()
    if check and stale:
        raise AssetError("Generated assets are missing/stale: " + ", ".join(stale) + "; run python3 tools/convert_sprites.py")
    return outputs


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=Path(__file__).resolve().parents[1] / "altoids-pet")
    parser.add_argument("--check", action="store_true", help="Validate sources and check generated files without writing")
    args = parser.parse_args(argv)
    try:
        outputs = convert(args.project_root, args.check)
    except AssetError as exc:
        print(f"Sprite conversion FAILED: {exc}", file=sys.stderr)
        return 1
    except OSError as exc:
        print(f"Sprite conversion FAILED: {exc}", file=sys.stderr)
        return 1
    print(f"{'Checked' if args.check else 'Generated'} {len(outputs)} asset files; 48x48 palette frame = {FRAME_BYTES} bytes. Missing buddy frames reuse authored art; missing gear keeps embedded fallbacks.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
