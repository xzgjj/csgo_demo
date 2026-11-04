"""程序化生成占位贴图（噪声/砖块/木纹/金属划痕/布料/泥土）到 assets/generated/。

纯标准库（zlib + struct 写 PNG），固定种子，可重复运行且产物字节一致；无第三方依赖。
用法：python -X utf8 tools/assets/make_placeholder_assets.py [--out DIR] [--size N]
"""

from __future__ import annotations

import argparse
import math
import random
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_OUT = ROOT / "assets/generated"
SIZE = 128
SEED = 20261104


def _lerp(a: float, b: float, t: float) -> float:
    return a + (b - a) * t


def _byte(value: float) -> int:
    return max(0, min(255, int(value)))


def _noise_field(cells: int, rng: random.Random) -> list[list[float]]:
    return [[rng.random() for _ in range(cells + 1)] for _ in range(cells + 1)]


def _sample(field: list[list[float]], u: float, v: float) -> float:
    """双线性插值取值噪声（u/v ∈ [0, 1)）。"""
    cells = len(field) - 1
    x = u * cells
    y = v * cells
    x0 = int(x) % cells
    y0 = int(y) % cells
    fx = x - int(x)
    fy = y - int(y)
    top = _lerp(field[y0][x0], field[y0][x0 + 1], fx)
    bottom = _lerp(field[y0 + 1][x0], field[y0 + 1][x0 + 1], fx)
    return _lerp(top, bottom, fy)


def _chunk(tag: bytes, data: bytes) -> bytes:
    crc = zlib.crc32(tag + data) & 0xFFFFFFFF
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)


def write_png(path: Path, width: int, height: int, pixels: bytes) -> int:
    """把 RGB 像素写成 8bit PNG（color type 2，每行 filter 0）。返回文件字节数。"""
    stride = width * 3
    raw = b"".join(b"\x00" + pixels[y * stride:(y + 1) * stride] for y in range(height))
    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    png = (
        b"\x89PNG\r\n\x1a\n"
        + _chunk(b"IHDR", header)
        + _chunk(b"IDAT", zlib.compress(raw, 9))
        + _chunk(b"IEND", b"")
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(png)
    return len(png)


def gen_concrete(size: int, rng: random.Random) -> bytes:
    """混凝土：粗噪声底 + 细颗粒 + 随机麻点。"""
    coarse = _noise_field(8, rng)
    fine = _noise_field(32, rng)
    out = bytearray()
    for y in range(size):
        for x in range(size):
            u, v = x / size, y / size
            base = 148 + 34 * _sample(coarse, u, v) + 12 * _sample(fine, u, v)
            speck = rng.randint(-26, 22) if rng.random() < 0.06 else 0
            value = _byte(base + speck)
            out.extend((value, value, _byte(value * 0.97)))
    return bytes(out)


def gen_brick(size: int, rng: random.Random) -> bytes:
    """砖墙：错缝砖块 + 砂浆缝 + 灰泥颗粒。"""
    brick_h, brick_w = 16, 32
    noise = _noise_field(16, rng)
    tints: dict[tuple[int, int], int] = {}
    out = bytearray()
    for y in range(size):
        row = y // brick_h
        for x in range(size):
            shift = brick_w // 2 if row % 2 else 0
            col = (x + shift) // brick_w
            if (row, col) not in tints:
                tints[(row, col)] = rng.randint(-24, 18)
            mortar = (y % brick_h < 2) or ((x + shift) % brick_w < 2)
            grain = 12 * _sample(noise, x / size, y / size)
            if mortar:
                value = _byte(178 + grain)
                out.extend((value, _byte(value * 0.98), _byte(value * 0.92)))
            else:
                value = _byte(136 + tints[(row, col)] + grain)
                out.extend((value, _byte(value * 0.62), _byte(value * 0.48)))
    return bytes(out)


def gen_wood(size: int, rng: random.Random) -> bytes:
    """木纹：弦切年轮 + 纵向纤维条纹。"""
    noise = _noise_field(24, rng)
    out = bytearray()
    for y in range(size):
        for x in range(size):
            u, v = x / size, y / size
            warp = 0.06 * _sample(noise, u, v * 0.5)
            rings = math.sin((v + warp) * math.pi * 22.0) * 0.5 + 0.5
            streak = 0.5 + 0.5 * _sample(noise, v, u * 0.25)
            base = 104 + 58 * (0.55 * rings + 0.45 * streak)
            out.extend((_byte(base), _byte(base * 0.64), _byte(base * 0.38)))
    return bytes(out)


def gen_metal(size: int, rng: random.Random) -> bytes:
    """金属：拉丝底 + 水平划痕。"""
    noise = _noise_field(16, rng)
    scratches = [
        (rng.randrange(size), rng.randrange(size), rng.randint(18, 64))
        for _ in range(14)
    ]
    out = bytearray()
    for y in range(size):
        for x in range(size):
            base = 138 + 18 * _sample(noise, x / size, y / size)
            hit = any(
                abs(y - sy) <= 1 and 0 <= x - sx <= length for sx, sy, length in scratches
            )
            value = _byte(base + (46 if hit else 0))
            out.extend((value, value, _byte(value * 1.04)))
    return bytes(out)


def gen_fabric(size: int, rng: random.Random) -> bytes:
    """布料：4px 经纬交织 + 线材噪声。"""
    noise = _noise_field(32, rng)
    out = bytearray()
    for y in range(size):
        for x in range(size):
            weave = 1.0 if ((x // 4) + (y // 4)) % 2 == 0 else 0.82
            thread = 0.9 + 0.2 * _sample(noise, x / size, y / size)
            value = 104 * weave * thread
            out.extend((_byte(value * 1.02), _byte(value * 0.92), _byte(value * 0.72)))
    return bytes(out)


def gen_dirt(size: int, rng: random.Random) -> bytes:
    """泥土：大尺度土块 + 细砂 + 随机砾石。"""
    coarse = _noise_field(6, rng)
    fine = _noise_field(24, rng)
    out = bytearray()
    for y in range(size):
        for x in range(size):
            u, v = x / size, y / size
            clump = _sample(coarse, u, v)
            grain = _sample(fine, u, v)
            pebble = 34 if rng.random() < 0.02 else 0
            value = 96 + 40 * clump + 14 * grain + pebble
            out.extend((_byte(value), _byte(value * 0.74), _byte(value * 0.52)))
    return bytes(out)


ASSETS = [
    ("concrete_noise.png", gen_concrete),
    ("brick_wall.png", gen_brick),
    ("wood_grain.png", gen_wood),
    ("metal_scratch.png", gen_metal),
    ("fabric_weave.png", gen_fabric),
    ("dirt_noise.png", gen_dirt),
]


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="程序化生成 6 类占位贴图")
    parser.add_argument("--out", default=str(DEFAULT_OUT), help="输出目录")
    parser.add_argument("--size", type=int, default=SIZE, help="边长（像素）")
    args = parser.parse_args(argv)
    if args.size < 8 or args.size > 1024:
        print(f"size 越界: {args.size}（允许 8..1024）")
        return 2
    out_dir = Path(args.out)
    rng = random.Random(SEED)
    total = 0
    for name, generator in ASSETS:
        pixels = generator(args.size, rng)
        written = write_png(out_dir / name, args.size, args.size, pixels)
        total += written
        print(f"wrote {name} {args.size}x{args.size} {written} bytes")
    print(f"summary: {len(ASSETS)} textures, {total} bytes, seed={SEED}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
