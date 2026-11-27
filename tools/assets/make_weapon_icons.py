"""程序化手枪图标：补齐 USP-S / Glock-18（与既有 AK/M4/AWP 的平涂剪影同风格）。

既有图标是 512×256 RGBA、两色平涂（棕枪身 + 近黑枪管/握把）。这里用多边形画两把
手枪：USP 长滑套 + 消音器短管，Glock 短滑套 + 更斜的握把，颜色同一套 palette。

用法：python -X utf8 tools/assets/make_weapon_icons.py
输出：assets/generated/procedural/icons/weapon_usp.png、weapon_glock.png
"""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "assets" / "generated" / "procedural" / "icons"

BODY = (74, 78, 86, 255)      # 枪身钢灰
DARK = (30, 30, 34, 255)      # 滑套/枪管/握把近黑
GRIP = (46, 44, 46, 255)      # 握把防滑纹底板
ACCENT = (150, 120, 74, 255)  # 木质/贴片小面积点缀


def canvas() -> tuple[Image.Image, ImageDraw.ImageDraw]:
    image = Image.new("RGBA", (512, 256), (0, 0, 0, 0))
    return image, ImageDraw.Draw(image)


def usp() -> Image.Image:
    image, draw = canvas()
    # 消音器 + 枪管（细长，右侧略高）
    draw.rectangle([318, 84, 486, 110], fill=DARK)
    # 滑套（长条，前端切角）
    draw.polygon([(112, 92), (330, 92), (330, 132), (112, 132)], fill=BODY)
    draw.polygon([(112, 92), (92, 100), (92, 132), (112, 132)], fill=DARK)
    # 准星/照门
    draw.rectangle([300, 80, 310, 92], fill=DARK)
    draw.rectangle([126, 80, 136, 92], fill=DARK)
    # 握把（后倾 18°）
    draw.polygon([(168, 128), (226, 128), (196, 232), (136, 232)], fill=GRIP)
    draw.polygon([(168, 128), (192, 128), (162, 232), (136, 232)], fill=DARK)
    # 扳机护圈
    draw.arc([196, 118, 262, 176], start=200, end=340, fill=DARK, width=6)
    # 弹匣底板
    draw.rectangle([132, 224, 200, 238], fill=DARK)
    return image


def glock() -> Image.Image:
    image, draw = canvas()
    # 短滑套（Glock 更方、更短）
    draw.polygon([(150, 86), (352, 86), (352, 128), (150, 128)], fill=BODY)
    draw.polygon([(150, 86), (134, 96), (134, 128), (150, 128)], fill=DARK)
    # 枪管口
    draw.rectangle([352, 96, 392, 118], fill=DARK)
    # 准星
    draw.rectangle([330, 74, 342, 86], fill=DARK)
    draw.rectangle([162, 74, 172, 86], fill=DARK)
    # 握把（更陡 26°）+ 贴片
    draw.polygon([(206, 124), (262, 124), (226, 236), (168, 236)], fill=GRIP)
    draw.polygon([(206, 124), (228, 124), (194, 236), (168, 236)], fill=ACCENT)
    draw.arc([236, 116, 300, 172], start=200, end=340, fill=DARK, width=6)
    draw.rectangle([164, 228, 232, 242], fill=DARK)
    return image


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    usp().save(OUT / "weapon_usp.png")
    glock().save(OUT / "weapon_glock.png")
    for name in ("weapon_usp", "weapon_glock"):
        path = OUT / f"{name}.png"
        print(f"{path.relative_to(ROOT)} {path.stat().st_size}B")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
