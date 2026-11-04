"""生成武器音效 WAV（原创程序化，无外部素材）。

输出：host/audio/shot.wav（枪声）、host/audio/hit.wav（命中提示）。
确定性：固定随机种子；44.1kHz 16-bit 单声道。
"""
from __future__ import annotations

import math
import random
import struct
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "host/audio"
RATE = 44100


def _write(path: Path, samples: list[float]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as handle:
        handle.setnchannels(1)
        handle.setsampwidth(2)
        handle.setframerate(RATE)
        frames = b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s)) * 32767)) for s in samples)
        handle.writeframes(frames)


def shot() -> None:
    random.seed(7)
    samples: list[float] = []
    length = int(RATE * 0.22)
    for i in range(length):
        t = i / RATE
        env = math.exp(-t * 26.0)
        noise = (random.random() * 2.0 - 1.0) * 0.75
        thump = math.sin(2.0 * math.pi * (95.0 - 40.0 * t) * t) * 0.55
        crack = math.sin(2.0 * math.pi * 1800.0 * t) * math.exp(-t * 90.0) * 0.35
        samples.append((noise + thump + crack) * env)
    _write(OUT / "shot.wav", samples)


def hit() -> None:
    random.seed(11)
    samples: list[float] = []
    length = int(RATE * 0.10)
    for i in range(length):
        t = i / RATE
        env = math.exp(-t * 42.0)
        tone = math.sin(2.0 * math.pi * 1250.0 * t) * 0.55
        tick = (random.random() * 2.0 - 1.0) * 0.20
        samples.append((tone + tick) * env)
    _write(OUT / "hit.wav", samples)


if __name__ == "__main__":
    shot()
    hit()
    print("wrote", OUT / "shot.wav", OUT / "hit.wav")
