"""屏幕占比审计：把「画面里那块大长方形到底是什么」变成可复现的数据。

输入是地图盒子清单（`host/scenes/<map>_boxes.json`，Godot 坐标）与固定机位，
输出每个机位的遮挡贡献表（盒子 id / kind / material / 尺寸 / 屏幕占比）与问题清单：
  * too_tall      : 墙体顶面过高（默认 > 320u），抬头即一整面墙
  * aerial_slab   : 不参与碰撞的悬浮板（屋顶层）横向尺度过大，压住通道上方
  * blank_panel   : 单盒占屏 ≥ 阈值且是一个大平面（无细节可读性差）
  * eye_inside    : 机位眼点落在实体盒内（画面被糊成一片）
  * spawn_blocked : 出生点/守点落位卡在实体盒里（敌人“直架在道具里”的静态证据）

同时给出材质清单：贴图缺失的材质会让大面变成纯色块，这是「什么都没有的长方形」的另一半原因。

用法：
  python -X utf8 tools/verify/visual_audit.py --map de_dust2
  python -X utf8 tools/verify/visual_audit.py --map de_dust2 --json log/verify/visual_audit.json
退出码：0 无问题；1 有问题（供 CI / run_all 使用）。
"""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# 与 host/tests/capture_map.gd 的 VIEWS 同源：改机位两边一起改。
VIEWS = {
    "overview_top": ((0.0, 3400.0, 0.0), (0.0, 0.0, 0.0)),
    "A_site": ((-1575.0, 420.0, -780.0), (-1575.0, 128.0, -1500.0)),
    "B_site": ((-1790.0, 420.0, 950.0), (-1790.0, 64.0, 1550.0)),
    "mid": ((180.0, 110.0, -260.0), (-1200.0, 64.0, -320.0)),
    "T_spawn": ((1835.0, 210.0, 850.0), (1200.0, 60.0, 300.0)),
    "CT_spawn": ((-1420.0, 210.0, -800.0), (-500.0, 60.0, -200.0)),
    "long": ((1500.0, 210.0, -1160.0), (-1400.0, 60.0, -1160.0)),
}
FOV_DEG = 75.0
ASPECT = 16.0 / 9.0
SAMPLES_X, SAMPLES_Y = 96, 54

TALL_WALL = 320.0
AERIAL_SPAN = 700.0
BLANK_SHARE = 0.30
NEAR = 1.0

# 贴图清单（res://textures/dust2/*）；缺失即“纯色块”风险。
TEXTURE_FILES = {
    "concrete": "host/textures/dust2/plaster_sand_01.png",
    "stone": "host/textures/dust2/sandstone_block_01.png",
    "wood": "host/textures/dust2/wood_plank_01.png",
    "metal": "host/textures/dust2/metal_panel_01.png",
    "glass": "host/textures/dust2/glass_crack_01.png",
    "fabric": "host/textures/dust2/sandbag_01.png",
    "dirt": "host/textures/dust2/dirt_ground_01.png",
    "road": "host/textures/dust2/road_compacted_01.png",
    "site": "",
    "spawn": "",
}


def load_boxes(map_id: str) -> list[dict]:
    path = ROOT / f"host/scenes/{map_id}_boxes.json"
    data = json.loads(path.read_text(encoding="utf-8"))
    boxes = []
    for index, box in enumerate(data["boxes"]):
        cx, cy, cz = (float(v) for v in box["center"])
        sx, sy, sz = (float(v) for v in box["size"])
        boxes.append({
            "index": index,
            "id": f"box_{index:03d}",
            "kind": box["kind"],
            "material": box["material"],
            "collision": bool(box.get("collision", True)),
            "min": (cx - sx / 2.0, cy - sy / 2.0, cz - sz / 2.0),
            "max": (cx + sx / 2.0, cy + sy / 2.0, cz + sz / 2.0),
            "size": (sx, sy, sz),
            "center": (cx, cy, cz),
        })
    return boxes


def _norm(v):
    length = math.sqrt(sum(c * c for c in v))
    return tuple(c / length for c in v) if length > 1e-9 else (0.0, 0.0, 0.0)


def _cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def _dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def ray_box(origin, direction, box, t_max=1.0e9):
    """返回最近命中距离；未命中返回 None。"""
    t_near, t_far = 0.0, t_max
    for axis in range(3):
        o = origin[axis]
        d = direction[axis]
        lo, hi = box["min"][axis], box["max"][axis]
        if abs(d) < 1e-9:
            if o < lo or o > hi:
                return None
            continue
        t1 = (lo - o) / d
        t2 = (hi - o) / d
        if t1 > t2:
            t1, t2 = t2, t1
        t_near = max(t_near, t1)
        t_far = min(t_far, t2)
        if t_near > t_far:
            return None
    return t_near if t_near > NEAR else t_far


def inside_box(point, box) -> bool:
    return all(box["min"][i] <= point[i] <= box["max"][i] for i in range(3))


def _sample_direction(forward, right, up, ndc_x, ndc_y):
    return _norm(tuple(forward[k] + right[k] * ndc_x + up[k] * ndc_y for k in range(3)))


def _nearest_box(boxes, eye, direction, skip):
    best, best_t = None, 1.0e9
    for box in boxes:
        if box["index"] in skip:
            continue
        t = ray_box(eye, direction, box)
        if t is not None and t < best_t:
            best, best_t = box["index"], t
    return best


def view_share(boxes, eye, target):
    forward = _norm(tuple(target[i] - eye[i] for i in range(3)))
    # 朝天机位（overview）forward 与 +Y 平行，换 (0,0,-1) 作 up，避免基向量退化。
    world_up = (0.0, 0.0, -1.0) if abs(forward[1]) > 0.95 else (0.0, 1.0, 0.0)
    right = _norm(_cross(forward, world_up))
    up = _cross(right, forward)
    # 眼点已在实体盒内时，最近命中没有意义（背面剔除会“看穿”），单独报 eye_inside。
    skip = {box["index"] for box in boxes if inside_box(eye, box) and box["collision"]}
    tan_v = math.tan(math.radians(FOV_DEG) * 0.5)
    total = SAMPLES_X * SAMPLES_Y
    hits: dict[int, int] = {}
    for j in range(SAMPLES_Y):
        ndc_y = (1.0 - 2.0 * (j + 0.5) / SAMPLES_Y) * tan_v
        for i in range(SAMPLES_X):
            ndc_x = (2.0 * (i + 0.5) / SAMPLES_X - 1.0) * tan_v * ASPECT
            direction = _sample_direction(forward, right, up, ndc_x, ndc_y)
            best = _nearest_box(boxes, eye, direction, skip)
            if best is not None:
                hits[best] = hits.get(best, 0) + 1
    return {index: count / total for index, count in hits.items()}


def _near(a: float, b: float, tol: float) -> bool:
    return abs(a - b) <= tol


def placement_points(layout: dict) -> list[dict]:
    """出生中心 + 守点环：内核实际使用的位置（不是台账 slots）。"""
    points = []
    for team, spawn in layout.get("spawns", {}).items():
        cx, cy = float(spawn["center"][0]), float(spawn["center"][1])
        for slot in range(5):
            points.append({
                "id": f"spawn_{team}_{slot}",
                "pos": (cx + (slot - 2) * 64.0, cy, 36.0),
            })
    for site_id, site in layout.get("sites", {}).items():
        cx, cy, cz = (float(v) for v in site["center"])
        for angle in (0.0, 90.0, 180.0, 270.0):
            dx = math.cos(math.radians(angle)) * 96.0
            dy = math.sin(math.radians(angle)) * 96.0
            points.append({
                "id": f"hold_{site_id}_{int(angle)}",
                "pos": (cx + dx, cy + dy, cz + 36.0),
            })
    return points


def hull(point, half=16.0, height=72.0) -> dict:
    x, y, z = point
    return {
        "min": (x - half, y - half, z - height / 2.0),
        "max": (x + half, y + half, z + height / 2.0),
    }


def placement_issues(boxes, layout):
    issues = []
    for point in placement_points(layout):
        probe = hull(point["pos"])
        for box in boxes:
            if not box["collision"] or box["max"][1] <= 1.0:
                continue
            overlap = all(probe["min"][i] < box["max"][i] and probe["max"][i] > box["min"][i]
                          for i in range(3))
            if overlap:
                issues.append({
                    "rule": "spawn_blocked",
                    "point": point["id"],
                    "pos": list(point["pos"]),
                    "box": box["id"],
                    "kind": box["kind"],
                })
                break
    return issues


def _view_row(box, share):
    return {
        "box": box["id"],
        "kind": box["kind"],
        "material": box["material"],
        "share": round(share, 4),
        "size": [round(v) for v in box["size"]],
        "center": [round(v) for v in box["center"]],
        "collision": box["collision"],
    }


def _ranked_rows(boxes, eye, target, limit=6):
    ranked = sorted(view_share(boxes, eye, target).items(), key=lambda item: -item[1])
    return [_view_row(boxes[index], share) for index, share in ranked[:limit]]


def _view_issues(name, rows, boxes):
    issues = []
    for row in rows[:4]:
        box = boxes[int(row["box"][4:])]
        width, height, depth = box["size"]
        span = max(width, depth)
        if row["share"] >= BLANK_SHARE and span >= 512.0 and height >= 128.0:
            issues.append({"rule": "blank_panel", "view": name, "box": box["id"],
                           "share": row["share"], "size": row["size"]})
        if not box["collision"] and box["min"][1] > 1.0 and span >= AERIAL_SPAN:
            issues.append({"rule": "aerial_slab", "view": name, "box": box["id"],
                           "size": row["size"], "center": row["center"]})
        if box["kind"] == "wall" and box["collision"] and box["max"][1] >= TALL_WALL:
            issues.append({"rule": "too_tall", "view": name, "box": box["id"],
                           "top": round(box["max"][1]), "size": row["size"]})
    return issues


def audit(map_id: str) -> dict:
    boxes = load_boxes(map_id)
    layout = json.loads((ROOT / f"content/maps/{map_id}.layout.json")
                        .read_text(encoding="utf-8"))
    issues: list[dict] = []
    views = {}
    missing_textures = [key for key, rel in TEXTURE_FILES.items()
                        if rel and not (ROOT / rel).exists()]
    for name, (eye, target) in VIEWS.items():
        rows = _ranked_rows(boxes, eye, target)
        views[name] = {"eye": list(eye), "rows": rows}
        if any(inside_box(eye, box) for box in boxes):
            issues.append({"rule": "eye_inside", "view": name, "eye": list(eye)})
        issues.extend(_view_issues(name, rows, boxes))
    issues.extend(placement_issues(boxes, layout))
    return {
        "map": map_id,
        "boxes": len(boxes),
        "missing_textures": missing_textures,
        "views": views,
        "issues": issues,
    }


def _dedupe(items):
    seen, out = set(), []
    for item in items:
        key = (item["rule"], item.get("box", ""), item.get("view", ""), item.get("point", ""))
        if key in seen:
            continue
        seen.add(key)
        out.append(item)
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description="地图遮挡/占位审计")
    parser.add_argument("--map", default="de_dust2")
    parser.add_argument("--json", default="log/verify/visual_audit.json")
    parser.add_argument("--top", type=int, default=3)
    parser.add_argument("--no-fail", action="store_true")
    args = parser.parse_args()
    report = audit(args.map)
    report["issues"] = _dedupe(report["issues"])
    out = ROOT / args.json
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")

    print(f"[visual] map={args.map} boxes={report['boxes']} "
          f"issues={len(report['issues'])} -> {out.relative_to(ROOT)}")
    if report["missing_textures"]:
        print("  缺贴图材质：" + ",".join(report["missing_textures"]))
    for name, view in report["views"].items():
        rows = view["rows"][:args.top]
        text = "; ".join(f"{r['box']} {r['kind']}/{r['material']} {r['share'] * 100:.0f}% "
                         f"{r['size']}" for r in rows)
        print(f"  {name:<12} {text}")
    groups: dict[str, int] = {}
    for issue in report["issues"]:
        groups[issue["rule"]] = groups.get(issue["rule"], 0) + 1
    if groups:
        print("  问题：" + ", ".join(f"{k}={v}" for k, v in sorted(groups.items())))
        for issue in report["issues"][:6]:
            print("   -", json.dumps(issue, ensure_ascii=False))
    return 0 if (args.no_fail or not report["issues"]) else 1


if __name__ == "__main__":
    raise SystemExit(main())
