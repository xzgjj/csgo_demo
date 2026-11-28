"""跳高可达性审计：找出「设计上永远上不去」的台面/台阶。

背景：内核跳高 72u（v=339.411 / g=800）、可直接迈步 32u。地图里如果有人工抬高的
平台（platform/stairs）没有 ≤72u 的落脚路径，bot 追点/玩家抢位就会被卡住，且
check_dust2 的 nav 采样分辨率看不见这种“孤岛台面”。

判定模型（与 core 的 placement/movement 口径一致）：
  * 可走面：地面 0、collision 盒顶 >0 的 platform / stairs（site/spawn 视为地面装饰）；
  * A→B 可走：B 比 A 高 0..72u（同高邻接可平移过去），水平投影间隙 <= 24u；
  * 从 0u 做闭包，闭包外的台面即不可达。

用法：python -X utf8 tools/mapgen/check_reach.py [--map de_dust2] [--json]
退出码：0 全可达；1 存在不可达台面。
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "mapgen"))

import paths  # noqa: E402

JUMP_REACH = 72.0
STEP_REACH = 32.0  # 信息用：<=32 属于走上去，不算跳
EDGE_GAP = 24.0
WALKABLE_KINDS = {"platform", "stairs"}


def footprint(box):
    cx, _, cz = box["center"]
    sx, _, sz = box["size"]
    return (cx - sx / 2, cx + sx / 2, cz - sz / 2, cz + sz / 2)


def top_of(box):
    return box["center"][1] + box["size"][1] / 2


def bottom_of(box):
    return box["center"][1] - box["size"][1] / 2


def horizontal_gap(a, b):
    ax0, ax1, az0, az1 = a
    bx0, bx1, bz0, bz1 = b
    gx = max(0.0, max(ax0, bx0) - min(ax1, bx1))
    gz = max(0.0, max(az0, bz0) - min(az1, bz1))
    return (gx * gx + gz * gz) ** 0.5


def load_boxes(map_id):
    path = ROOT / "host" / "scenes" / f"{map_id}_boxes.json"
    return json.loads(path.read_text(encoding="utf-8"))["boxes"]


def ramp_entries(boxes):
    """坡道是连续的斜面：底边贴地（<=32u）就把顶边记成可达，再闭包接到相邻台面。"""
    return [(top_of(b), footprint(b)) for b in boxes
            if b.get("collision") and b["kind"] == "ramp" and bottom_of(b) <= STEP_REACH]


def reachable_by(box, reachable):
    """这个盒顶能否从已可达集合里上去（跳高内且边缘贴合）。"""
    top = top_of(box)
    fp = footprint(box)
    for level, other_fp in reachable:
        rise = top - level
        if -0.5 <= rise <= JUMP_REACH and horizontal_gap(fp, other_fp) <= EDGE_GAP:
            return True
    return False


def audit(boxes):
    walkables = [b for b in boxes if b.get("collision") and b["kind"] in WALKABLE_KINDS]
    reachable = [(0.0, (-100000.0, 100000.0, -100000.0, 100000.0))] + ramp_entries(boxes)
    remaining = list(walkables)
    while remaining:
        progressed = False
        for box in list(remaining):
            if not reachable_by(box, reachable):
                continue
            reachable.append((top_of(box), footprint(box)))
            remaining.remove(box)
            progressed = True
        if not progressed:
            break
    return remaining, walkables


def main(argv=None):
    parser = argparse.ArgumentParser(description="跳高可达性审计")
    parser.add_argument("--map", default="de_dust2")
    parser.add_argument("--json", action="store_true", help="只输出 JSON 结果")
    args = parser.parse_args(argv)

    boxes = load_boxes(args.map)
    unreachable, walkables = audit(boxes)
    issues = []
    for box in unreachable:
        fp = footprint(box)
        issues.append(
            {
                "kind": box["kind"],
                "top": top_of(box),
                "center": [box["center"][0], box["center"][2]],
                "size": [box["size"][0], box["size"][2]],
                "reason": f"顶面 {top_of(box):.0f}u 无 <= {JUMP_REACH:.0f}u 落脚路径",
                "footprint": [round(v, 1) for v in fp],
            }
        )

    report = {
        "map": args.map,
        "walkable_surfaces": len(walkables),
        "unreachable": len(issues),
        "issues": issues,
    }
    out_dir = ROOT / "log" / "verify"
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "check_reach.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")

    if args.json:
        print(json.dumps(report, ensure_ascii=False, indent=2))
    else:
        print(f"{args.map}: 台面 {len(walkables)} 个，不可达 {len(issues)} 个")
        for issue in issues:
            print(f"  - {issue['kind']} top={issue['top']:.0f} at {issue['center']} size={issue['size']}")
        print("PASS" if not issues else "FAIL")
    return 0 if not issues else 1


if __name__ == "__main__":
    raise SystemExit(main())
