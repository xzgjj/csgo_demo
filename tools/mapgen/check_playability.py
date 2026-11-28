"""de_dust2 可玩性审计入口：铺地、台阶、车道遮挡、掩体尺度、出生槽、坡道。

用法：
  python -X utf8 tools/mapgen/check_playability.py --map de_dust2
  python -X utf8 tools/mapgen/check_playability.py --map de_dust2 --json
只读 host/scenes/<map>_boxes.json 与 content/maps/<map>.layout.json，
报告写到 log/verify/playability.json；有问题退出码 1，--no-fail 只记基线。
"""

import argparse
import json
from pathlib import Path

import playability_rules as rules

ROOT = Path(__file__).resolve().parents[2]


def to_sim(box, index):
    """boxes.json 是 Godot 坐标（x=-sim_z, z=-sim_x），这里换回 layout/sim 平面。

    报告要能定位到具体盒：boxes.json 无 id 字段，用「种类#序号」合成稳定标签。
    坡道的 slope.axis 也要跟着换轴：Godot z 对应 sim x，Godot x 对应 sim z。
    """
    bx, by, bz = (float(v) for v in box["center"])
    sx, sy, sz = (abs(float(v)) for v in box["size"])
    slope = box.get("slope")
    if isinstance(slope, dict):
        axis = "x" if slope.get("axis") == "z" else "z"
        slope = {"axis": axis, "ascending": bool(slope.get("ascending"))}
    return {"id": f"{box['kind']}#{index}", "center": [-bz, by, -bx],
            "size": [sz, sy, sx], "kind": box["kind"],
            "material": box.get("material", ""), "slope": slope,
            "collision": bool(box.get("collision")), "hp": box.get("hp", 0)}


def load_map(map_id):
    boxes_path = ROOT / "host" / "scenes" / f"{map_id}_boxes.json"
    layout_path = ROOT / "content" / "maps" / f"{map_id}.layout.json"
    raw = json.loads(boxes_path.read_text(encoding="utf-8"))["boxes"]
    boxes = [to_sim(box, index) for index, box in enumerate(raw)]
    layout = json.loads(layout_path.read_text(encoding="utf-8"))
    return boxes, layout


def _paving_failures(paving):
    return [f"{item['site']} 点地面缺口 {item['holes']} 格" for item in paving if item["holes"]]


def _stair_failures(stairs):
    return [f"{item['chain']} 长台阶 {item['steps']} 级/{item['run']}u @{item['site']}"
            for item in stairs if item["verdict"] == "long_stairs" and item["site"]]


def _lane_failures(lanes):
    out = []
    for item in lanes:
        slabs = [b for b in item["blockers"] if b["severity"] == "slab"]
        if slabs:
            out.append(f"车道 {item['lane']} 细长板拦路 {len(slabs)} 件")
    return out


def collect_failures(report):
    """把各段结果折成硬失败清单：只有这里为空才算通过。"""
    hard = []
    hard += _paving_failures(report["paving"])
    hard += _stair_failures(report["stairs"])
    hard += _lane_failures(report["lanes"])
    hard += [f"{item['site']} 点细长板 {item['slab_count']} 件"
             for item in report["cover"] if item["slab_count"]]
    hard += [f"{item['team']} 出生点异常"
             for item in report["spawns"] if item["problems"] or item["slots"] != 5]
    hard += [f"坡道 {item['id']} {'；'.join(item['problems'])}"
             for item in report["ramps"] if item["problems"]]
    return hard


def audit(map_id):
    boxes, layout = load_map(map_id)
    report = {
        "map": map_id,
        "paving": rules.check_paving(layout, boxes),
        "stairs": rules.check_stairs(layout, boxes),
        "lanes": rules.check_lane_blockers(layout, boxes),
        "cover": rules.check_cover(layout, boxes),
        "spawns": rules.check_spawns(layout, boxes),
        "ramps": rules.check_ramps(boxes),
    }
    report["hard_failures"] = collect_failures(report)
    return report


def _print_sites(report):
    for item in report["paving"]:
        print(f"{item['site']} 点铺地 {item['coverage'] * 100:.1f}% 缺口 {item['holes']} 格 "
              f"@标高 {item['level']:.0f}")
    for item in report["stairs"]:
        site = f"@{item['site']}" if item["site"] else ""
        print(f"台阶 {item['chain']}: {item['steps']} 级 跨度 {item['run']}u → {item['verdict']} {site}")
    for item in report["cover"]:
        print(f"{item['site']} 点掩体 {item['cover_count']} 件，细长板 {item['slab_count']} 件")


def _print_traffic(report):
    for item in report["lanes"]:
        for blocker in item["blockers"]:
            print(f"车道 {item['lane']} 拦路 {blocker['id']} 横宽 {blocker['cross']}u "
                  f"/ 车道 {blocker['lane_width']}u")
    for item in report["ramps"]:
        print(f"坡道 {item['id']} 升 {item['rise']}u / 跑 {item['run']} → {item['slope']}° "
              f"接 {item['landing'] or '无'} {item['problems'] or ''}")
    for item in report["spawns"]:
        print(f"{item['team']} 出生槽 {item['slots']} 个，问题 {item['problems'] or '无'}")


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description="地图可玩性审计")
    parser.add_argument("--map", default="de_dust2")
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--no-fail", action="store_true", help="只记录基线，不返回失败码")
    args = parser.parse_args(argv)

    report = audit(args.map)
    out_dir = ROOT / "log" / "verify"
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "playability.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")

    if args.json:
        print(json.dumps(report, ensure_ascii=False, indent=2))
        return 0 if (args.no_fail or not report["hard_failures"]) else 1

    _print_sites(report)
    _print_traffic(report)
    print(f"hard_failures={len(report['hard_failures'])}")
    for line in report["hard_failures"]:
        print(f"  - {line}")
    print("PASS" if not report["hard_failures"] else "FAIL")
    return 0 if (args.no_fail or not report["hard_failures"]) else 1


if __name__ == "__main__":
    raise SystemExit(main())
