"""mapgen 公共路径常量（避免各脚本重复 8 行头触发 CI 重复块）。"""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LAYOUT = ROOT / "content/maps/de_ash.layout.json"
BOXES = ROOT / "host/scenes/de_ash_boxes.json"
