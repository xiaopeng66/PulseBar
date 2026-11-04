#!/usr/bin/env python3
"""Generate the three PulseBar icon styles from the original blue master.

Requires Pillow. Run with Python -I; the default (or --check) only compares
expected ICO bytes and exits 1 on a mismatch. Only --write changes outputs.

The master is the unchanged 256px image from the existing PulseBar icon, not
the legacy circle brand. Its opaque white pixels define the pulse and
its alpha defines the rounded square. Recoloring therefore cannot drift to a
different silhouette. All frames are scaled directly from this master;
the seven original blue sizes, including 16px, keep their exact pixels.

Style order shared with the app: blue=0 (IDR_MAINFRAME), light=1 (349), dark=2
(350). The updated about bitmap is a preserved asset, not regenerated here.
"""
from __future__ import annotations

import argparse
from io import BytesIO
from pathlib import Path
import struct
import sys

from PIL import Image

SIZES = (16, 20, 24, 28, 32, 40, 48, 64, 128, 256)
STYLES = (
    ("pulsebar.ico", None, None),
    ("pulsebar-tray-light.ico", (242, 245, 250), (25, 34, 52)),
    ("pulsebar-tray-dark.ico", (27, 35, 50), (255, 255, 255)),
)
DEFAULT_ASSET_DIR = Path(__file__).resolve().parents[1] / "PulseBar/res"


def load_master(asset_dir: Path) -> Image.Image:
    with Image.open(asset_dir / "pulsebar-master.png") as source:
        master = source.convert("RGBA")
    if master.size != (256, 256):
        raise ValueError("pulsebar-master.png must be the canonical 256x256 image")
    # The original master has hard-edged opaque art; downsampling antialiases
    # both masks together. Reject an unrelated/flattened source before writing.
    pixels = list(master.get_flattened_data())
    if any(a not in (0, 255) for _, _, _, a in pixels):
        raise ValueError("Master alpha must be 0/255, as in the original design")
    if not any(a == 0 for _, _, _, a in pixels):
        raise ValueError("Master must retain transparent rounded corners")
    if not any((r, g, b, a) == (255, 255, 255, 255) for r, g, b, a in pixels):
        raise ValueError("Master must contain its original white pulse")
    if any(a and (r, g, b) != (255, 255, 255) and not (r < 32 and b > g > r)
           for r, g, b, a in pixels):
        raise ValueError("Master must be the blue PulseBar square with a white pulse")
    return master


def recolor(master: Image.Image, background: tuple[int, int, int],
            foreground: tuple[int, int, int]) -> Image.Image:
    image = Image.new("RGBA", master.size)
    image.putdata([
        (*foreground, a) if (r, g, b) == (255, 255, 255)
        else (*background, a) if a else (0, 0, 0, 0)
        for r, g, b, a in master.get_flattened_data()
    ])
    return image


def encode_ico(master: Image.Image) -> bytes:
    # Build the directory explicitly so Pillow's default ICO size list cannot
    # silently drop 20/28/40px. PNG payloads preserve full RGBA at every size.
    payloads = []
    for size in SIZES:
        frame = master.copy() if size == 256 else master.resize((size, size), Image.Resampling.LANCZOS)
        # Lanczos can ring into the 20px frame's four empty corner pixels.
        # Keep corners truly transparent, as in every original blue frame.
        for corner in ((0, 0), (size - 1, 0), (0, size - 1), (size - 1, size - 1)):
            frame.putpixel(corner, (0, 0, 0, 0))
        stream = BytesIO()
        frame.save(stream, format="PNG", optimize=False, compress_level=9)
        payloads.append(stream.getvalue())
    directory = bytearray(struct.pack("<HHH", 0, 1, len(SIZES)))
    offset = 6 + 16 * len(SIZES)
    for size, payload in zip(SIZES, payloads):
        directory += struct.pack("<BBBBHHII", size % 256, size % 256, 0, 0,
                                 1, 32, len(payload), offset)
        offset += len(payload)
    return bytes(directory) + b"".join(payloads)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true", help="compare only (the default)")
    mode.add_argument("--write", action="store_true", help="write missing or changed ICO files")
    parser.add_argument("--asset-dir", type=Path, default=DEFAULT_ASSET_DIR,
                        help="directory containing pulsebar-master.png and the generated ICOs")
    args = parser.parse_args()
    try:
        master = load_master(args.asset_dir)
        expected = {
            filename: encode_ico(master if background is None else recolor(master, background, foreground))
            for filename, background, foreground in STYLES
        }
        different = False
        for filename, payload in expected.items():
            path = args.asset_dir / filename
            if path.is_file() and path.read_bytes() == payload:
                print(f"OK {filename}: {len(SIZES)} sizes")
                continue
            different = True
            if args.write:
                path.write_bytes(payload)
                print(f"WROTE {filename}: {len(SIZES)} sizes")
            else:
                print(f"MISMATCH {filename}: run with --write to regenerate")
        return 1 if different and not args.write else 0
    except (OSError, ValueError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
