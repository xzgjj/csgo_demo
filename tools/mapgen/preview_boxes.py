"""把派生盒子画成俯视雷达预览（换图后肉眼核对轮廓用）。

用法：python -X utf8 tools/mapgen/preview_boxes.py
输出：docs/spec01-de_dust2-radar-preview.png（docs 被 .gitignore 忽略）
"""

from __future__ import annotations

import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
BOXES = ROOT / "host/scenes/de_dust2_boxes.json"
LAYOUT = ROOT / "content/maps/de_dust2.layout.json"
OUT = ROOT / "docs/spec01-de_dust2-radar-preview.png"

W, H = 1100, 1100
MX, MY, MW = 60, 70, 980
S = MW / 4600.0
COLORS = {
    "wall": (70, 74, 82, 235),
    "platform": (196, 204, 216, 235),
    "stairs": (168, 178, 192, 235),
    "prop": (214, 148, 60, 220),
    "floor": (238, 240, 242, 255),
    "site": (196, 92, 78, 140),
    "spawn": (86, 128, 200, 140),
}


def px(x): return MX + (x + 2300) * S
def py(y): return MY + (2300 - y) * S


def main() -> int:
    data = json.loads(BOXES.read_text(encoding="utf-8"))
    layout = json.loads(LAYOUT.read_text(encoding="utf-8"))
    img = Image.new("RGBA", (W, H), (255, 255, 255, 255))
    draw = ImageDraw.Draw(img, "RGBA")
    font = ImageFont.truetype("C:/Windows/Fonts/msyh.ttc", 15)
    bold = ImageFont.truetype("C:/Windows/Fonts/msyhbd.ttc", 18)

    draw.rectangle([MX, MY, MX + MW, MY + MW], outline=(150, 150, 150, 255), width=1)
    for box in sorted(data["boxes"], key=lambda b: b["kind"] in {"site", "spawn"}):
        gx, gy, gz = box["center"]          # Godot: x=-sim_y, y=z, z=-sim_x
        sx, sy, sz = box["size"]            # size: (sim_y, z, sim_x)
        x0, x1 = -(gz + sz / 2), -(gz - sz / 2)
        y0, y1 = -(gx + sx / 2), -(gx - sx / 2)
        color = COLORS.get(box["kind"], (120, 120, 120, 200))
        draw.rectangle([px(x0), py(y1), px(x1), py(y0)], fill=color,
                       outline=(60, 62, 66, 255), width=1)

    for lane in layout["lanes"]:
        pts = [(px(x), py(y)) for x, y in lane["points"]]
        draw.line(pts, fill=(40, 150, 110, 150), width=3)
    for choke in layout["choke_points"]:
        x, y = choke["center"]
        r = 6
        draw.polygon([(px(x), py(y) - r), (px(x) + r, py(y)), (px(x), py(y) + r), (px(x) - r, py(y))],
                     fill=(200, 50, 42, 255))
    for site in layout["bombsites"]:
        x, y = site["center"][0], site["center"][1]
        draw.text((px(x), py(y) + 14), site["id"], font=bold, fill=(160, 40, 34, 255), anchor="mm")
    for team, spawn in layout["spawns"].items():
        x, y = spawn["center"][0], spawn["center"][1]
        draw.text((px(x), py(y)), team, font=bold,
                  fill=(200, 90, 30, 255) if team == "T" else (40, 90, 200, 255), anchor="mm")

    draw.text((MX, 22), "de_dust2 灰盒派生预览（53 盒；绿线=8 条对线，红点=choke）",
              font=ImageFont.truetype("C:/Windows/Fonts/msyhbd.ttc", 24), fill=(24, 24, 26, 255))
    draw.text((MX, H - 26), "由 tools/mapgen/preview_boxes.py 从 de_dust2_boxes.json 直出，非手绘",
              font=font, fill=(110, 114, 120, 255))
    OUT.parent.mkdir(parents=True, exist_ok=True)
    img.convert("RGB").save(OUT)
    print("saved", OUT.relative_to(ROOT), img.size)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

