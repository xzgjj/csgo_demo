"""de_ash blockout 生成（Blender background，独立进程，不打扰已打开的 Blender）。

用法：
  blender --background --python tools/blender/build_de_ash.py -- <layout.json> <versions_dir> <glb_out>
每次运行保存一个新版本 .blend；只保留最近 3 个版本（多删）。
"""

import json
import math
import sys
from pathlib import Path

import bpy


def args() -> tuple[Path, Path, Path]:
    raw = sys.argv[sys.argv.index("--") + 1:]
    return Path(raw[0]), Path(raw[1]), Path(raw[2])


def material(name: str, rgb: tuple[float, float, float]):
    mat = bpy.data.materials.get(name)
    if mat is None:
        mat = bpy.data.materials.new(name)
        mat.diffuse_color = (*rgb, 1.0)
    return mat


def add_box(name: str, center, size, color, rot_y=0.0):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(center[0], center[1], center[2]))
    obj = bpy.context.active_object
    obj.name = name
    obj.scale = (size[0], size[1], size[2])
    obj.rotation_euler[2] = rot_y
    obj.data.materials.append(material(f"m_{name.split('_')[0]}", color))
    return obj


def main() -> None:
    layout_path, versions, glb_out = args()
    data = json.loads(layout_path.read_text(encoding="utf-8"))
    bpy.ops.wm.read_factory_settings(use_empty=True)

    lo, hi = data["bounds"]["min"], data["bounds"]["max"]
    w, d, h = hi[0] - lo[0], hi[1] - lo[1], data["wall_height"]
    add_box("floor", (0, 0, -8), (w, d, 16), (0.30, 0.31, 0.33))
    t = 32.0
    add_box("wall_w", (lo[0] - t / 2, 0, h / 2), (t, d, h), (0.42, 0.40, 0.36))
    add_box("wall_e", (hi[0] + t / 2, 0, h / 2), (t, d, h), (0.42, 0.40, 0.36))
    add_box("wall_s", (0, lo[1] - t / 2, h / 2), (w, t, h), (0.42, 0.40, 0.36))
    add_box("wall_n", (0, hi[1] + t / 2, h / 2), (w, t, h), (0.42, 0.40, 0.36))

    for li, lane in enumerate(data["lanes"]):
        pts = lane["points"]
        for si, (a, b) in enumerate(zip(pts, pts[1:])):
            length = math.hypot(b[0] - a[0], b[1] - a[1])
            theta = math.atan2(b[1] - a[1], b[0] - a[0])
            add_box(f"lane_{li}_{si}", ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, 4),
                    (length, lane["width"], 8), (0.36, 0.37, 0.39), -theta)
    for site in data["bombsites"]:
        add_box(f"site_{site['id']}", (site["center"][0], site["center"][1], 10),
                (site["size"][0], site["size"][1], 4), (0.75, 0.25, 0.20))
    for team, spawn in data["spawns"].items():
        add_box(f"spawn_{team}", (spawn["center"][0], spawn["center"][1], 12),
                (spawn["size"][0], spawn["size"][1], 4), (0.20, 0.45, 0.75))
    for pi, prop in enumerate(data["props"]):
        sx, sy, sz = prop["size"]
        add_box(f"prop_{pi}_{prop['type']}", (prop["center"][0], prop["center"][1], prop["center"][2] + sz / 2),
                (sx, sy, sz), (0.50, 0.36, 0.22))

    versions.mkdir(parents=True, exist_ok=True)
    existing = sorted(versions.glob("de_ash_v*.blend"))
    index = 1 + max([int(p.stem.split("_v")[-1]) for p in existing], default=0)
    blend_path = versions / f"de_ash_v{index:03d}.blend"
    bpy.ops.wm.save_as_mainfile(filepath=str(blend_path))

    glb_out.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=str(glb_out), export_format="GLB")

    for old in sorted(versions.glob("de_ash_v*.blend"))[:-3]:
        old.unlink()
    print(f"BLENDER_OK version={blend_path.name} glb={glb_out} objects={len(bpy.data.objects)}")


if __name__ == "__main__":
    main()
