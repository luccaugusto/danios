#!/usr/bin/env python3
"""Convert an SVG icon into an LVGL v8 TRUE_COLOR_ALPHA .bin.

Pixel format: RGB565 with NO byte swap (LV_COLOR_16_SWAP is 0 in
include/lv_conf.h) + a straight (non-premultiplied) 8-bit alpha channel:
3 bytes per pixel, row-major: [rgb565_lo, rgb565_hi, alpha].

4-byte little-endian LVGL v8 image header (matches lv_img_header_t on a
little-endian target, see lv_img_buf.h):
    bits 0-4   cf            = 5 (LV_IMG_CF_TRUE_COLOR_ALPHA)
    bits 5-7   always_zero   = 0
    bits 8-9   reserved      = 0
    bits 10-20 w             (11 bits)
    bits 21-31 h             (11 bits)

Pipeline: rsvg-convert renders the SVG to a PNG at the target size, then PIL
loads it as RGBA and each pixel is quantized/packed as above. Where alpha is
0 the RGB is forced to black before quantizing (LVGL never reads RGB at
alpha=0, but writing black keeps the file deterministic instead of leaking
whatever an SVG renderer happened to put in fully-transparent pixels).

Usage:
    python3 assets/icons/svg_to_lvgl_bin.py <input.svg> <output.bin> [--size WxH]

Reusable for future launcher icons — just point it at a new SVG. Every run
verifies its own output (parses the header back, checks the file size) and
prints the result; a bad conversion exits non-zero instead of shipping a
silently-wrong binary.
"""
import argparse
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

LV_IMG_CF_TRUE_COLOR_ALPHA = 5


def render_svg_to_png(svg_path: Path, png_path: Path, width: int, height: int) -> None:
    subprocess.run(
        [
            "rsvg-convert",
            "-w", str(width),
            "-h", str(height),
            "-o", str(png_path),
            str(svg_path),
        ],
        check=True,
    )


def pack_header(cf: int, w: int, h: int) -> bytes:
    if not (0 <= cf < 32):
        raise ValueError(f"cf out of range (5 bits): {cf}")
    if not (0 <= w < 2048 and 0 <= h < 2048):
        raise ValueError(f"w/h out of range (11 bits each): {w}x{h}")
    value = (cf & 0x1F) | ((w & 0x7FF) << 10) | ((h & 0x7FF) << 21)
    return struct.pack("<I", value)


def unpack_header(data: bytes):
    (value,) = struct.unpack_from("<I", data, 0)
    return {
        "cf": value & 0x1F,
        "always_zero": (value >> 5) & 0x7,
        "reserved": (value >> 8) & 0x3,
        "w": (value >> 10) & 0x7FF,
        "h": (value >> 21) & 0x7FF,
    }


def rgb888_to_rgb565(r: int, g: int, b: int) -> int:
    r5 = r >> 3
    g6 = g >> 2
    b5 = b >> 3
    return (r5 << 11) | (g6 << 5) | b5


def convert(svg_path: Path, bin_path: Path, width: int, height: int) -> None:
    with tempfile.TemporaryDirectory() as tmp:
        png_path = Path(tmp) / "render.png"
        render_svg_to_png(svg_path, png_path, width, height)
        img = Image.open(png_path).convert("RGBA")
        if img.size != (width, height):
            raise ValueError(
                f"rsvg-convert produced {img.size}, expected {(width, height)}"
            )
        pixels = img.load()

        body = bytearray(width * height * 3)
        idx = 0
        for y in range(height):
            for x in range(width):
                r, g, b, a = pixels[x, y]
                if a == 0:
                    r = g = b = 0  # LVGL ignores RGB here; keep it deterministic
                rgb565 = rgb888_to_rgb565(r, g, b)
                body[idx] = rgb565 & 0xFF            # rgb565 lo byte
                body[idx + 1] = (rgb565 >> 8) & 0xFF  # rgb565 hi byte
                body[idx + 2] = a                     # straight alpha
                idx += 3

    header = pack_header(LV_IMG_CF_TRUE_COLOR_ALPHA, width, height)
    bin_path.parent.mkdir(parents=True, exist_ok=True)
    bin_path.write_bytes(header + bytes(body))


def verify(bin_path: Path, expected_w: int, expected_h: int) -> bool:
    data = bin_path.read_bytes()
    header = unpack_header(data)
    expected_size = 4 + expected_w * expected_h * 3
    ok = (
        header["cf"] == LV_IMG_CF_TRUE_COLOR_ALPHA
        and header["w"] == expected_w
        and header["h"] == expected_h
        and len(data) == expected_size
    )
    print(f"file:   {bin_path}")
    print(f"size:   {len(data)} bytes (expected {expected_size})")
    print(
        "header: cf={cf} always_zero={always_zero} reserved={reserved} "
        "w={w} h={h}".format(**header)
    )
    print("verify:", "OK" if ok else "FAIL")
    return ok


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("svg", type=Path, help="source .svg path")
    parser.add_argument("bin", type=Path, help="output .bin path")
    parser.add_argument("--size", default="64x64", help="WxH in pixels, default 64x64")
    args = parser.parse_args()

    width_str, height_str = args.size.lower().split("x")
    width, height = int(width_str), int(height_str)

    convert(args.svg, args.bin, width, height)
    if not verify(args.bin, width, height):
        sys.exit(1)


if __name__ == "__main__":
    main()
