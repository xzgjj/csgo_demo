"""时间线验证管线：把“游玩时长”变成 tick 步进 + 时间线断言（不依赖真实时钟）。

用法：python -X utf8 tools/verify/run_timeline.py [--scenario ID] [--visual] [--update] [--determinism]
制品：verification/actual/timelines/<id>/（timeline/transitions/marks/meta + 关键帧）；报告：log/verify/timeline/
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/verify"))
import run_scenarios  # noqa: E402
from timeline_report import write_report  # noqa: E402

SPEC_DIR = ROOT / "verification/timelines"
ACTUAL = ROOT / "verification/actual/timelines"
GOLD = ROOT / "verification/goldens/timeline"
SCRIPT = "res://tests/timeline_suite.gd"
ALLOWED_PAIRS = {(0, 1), (1, 2), (2, 0)}


def read_jsonl(path: Path) -> list[dict]:
    if not path.exists():
        return []
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]


def run_suite(godot: Path, spec: Path, out_dir: Path, visual: bool) -> tuple[int, dict, str]:
    out_dir.mkdir(parents=True, exist_ok=True)
    cmd = [
        str(godot),
        "--path",
        "host",
        "--script",
        SCRIPT,
        "--",
        str(spec),
        str(out_dir),
    ]
    if visual:
        cmd.append("--visual")
    else:
        cmd.insert(1, "--headless")
    proc = subprocess.run(
        cmd,
        cwd=str(ROOT),
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=1800,
    )
    output = (proc.stdout or "") + (proc.stderr or "")
    meta: dict = {}
    for line in output.splitlines():
        if line.startswith("TIMELINE_META "):
            meta = json.loads(line[len("TIMELINE_META ") :])
    return proc.returncode, meta, output


def compute_rounds(transitions: list[dict]) -> list[dict]:
    """由阶段转换还原每回合：freeze / live / end 时长 + 得分 + 胜方。"""
    rounds: list[dict] = []
    freeze_start = 0
    live_start = 0
    end_start = 0
    score = (0, 0)
    for tr in transitions:
        tick, src, dst = int(tr["tick"]), int(tr["from"]), int(tr["to"])
        current = (int(tr["score_t"]), int(tr["score_ct"]))
        if (src, dst) == (0, 1):
            rounds.append(
                {
                    "number": int(tr["round_from"]),
                    "freeze": tick - freeze_start,
                    "live": None,
                    "end": None,
                    "winner": None,
                    "score_before": score,
                    "score_after": None,
                }
            )
            live_start = tick
        elif (src, dst) == (1, 2) and rounds:
            rounds[-1]["live"] = tick - live_start
            end_start = tick
            rounds[-1]["score_at_end"] = current
        elif (src, dst) == (2, 0) and rounds:
            rounds[-1]["end"] = tick - end_start
            rounds[-1]["score_after"] = current
            if current[0] > score[0]:
                rounds[-1]["winner"] = "T"
            elif current[1] > score[1]:
                rounds[-1]["winner"] = "CT"
            score = current
            freeze_start = tick
    return rounds


def check_structure(meta: dict, samples: list[dict], transitions: list[dict], spec: dict) -> list[tuple]:
    expect = spec.get("expect", {})
    rounds = compute_rounds(transitions)
    checks: list[tuple] = []
    checks.append(("玩家数=10", int(meta.get("player_count", 0)) == 10, str(meta.get("player_count"))))
    bad_pairs = [(t["from"], t["to"]) for t in transitions if (t["from"], t["to"]) not in ALLOWED_PAIRS]
    checks.append(("阶段转换序列", not bad_pairs, f"异常 {bad_pairs[:3]}" if bad_pairs else "仅 0→1/1→2/2→0"))
    want_rounds = int(expect.get("rounds", 1))
    checks.append(("回合数", len(rounds) == want_rounds, f"{len(rounds)}/{want_rounds}"))
    checks.append(("转换条数", len(transitions) == want_rounds * 3, f"{len(transitions)}/{want_rounds * 3}"))
    checks.append(("采样条数", len(samples) > 0, str(len(samples))))
    if not rounds:
        return checks
    freeze_expect = int(expect.get("freeze_ticks", 192))
    end_expect = int(expect.get("end_ticks", 320))
    live_min = int(expect.get("min_live_ticks", 1))
    live_max = int(expect.get("max_live_ticks", 7360))
    for item in rounds:
        tag = f"r{item['number']}"
        checks.append((f"{tag} freeze", item["freeze"] == freeze_expect, f"{item['freeze']}/{freeze_expect} tick"))
        checks.append((f"{tag} live", live_min <= int(item["live"] or -1) <= live_max,
                       f"{item['live']} in [{live_min},{live_max}] tick"))
        checks.append((f"{tag} round_end", item["end"] == end_expect, f"{item['end']}/{end_expect} tick"))
        before = item["score_before"]
        after = item["score_after"] or (0, 0)
        delta = (after[0] - before[0], after[1] - before[1])
        want_delta = (1, 0) if item["winner"] == "T" else (0, 1)
        checks.append((f"{tag} 胜方得分", item["winner"] in ("T", "CT") and delta == want_delta,
                       f"{item['winner']} 变化{before}→{after}"))
    final_score = (int(meta["score_t"]), int(meta["score_ct"]))
    checks.append(("总比分=回合数", final_score[0] + final_score[1] == want_rounds,
                   f"{final_score[0]}:{final_score[1]}（共 {want_rounds} 分）"))
    return checks


def _player_issues(tick: int, index: int, player: list) -> list[str]:
    x, y, z, hp, mag, alive = [int(value) for value in player]
    issues = []
    if not 0 <= hp <= 100:
        issues.append(f"t={tick} p{index} hp={hp}")
    if not 0 <= mag <= 30:
        issues.append(f"t={tick} p{index} mag={mag}")
    if z < -1:
        issues.append(f"t={tick} p{index} z={z}")
    if abs(x) > 2300 or abs(y) > 2300:
        issues.append(f"t={tick} p{index} pos=({x},{y})")
    if alive not in (0, 1):
        issues.append(f"t={tick} p{index} alive={alive}")
    return issues


def check_invariants(samples: list[dict]) -> list[tuple]:
    problems: list[str] = []
    prev_tick = -1
    for row in samples:
        tick = int(row["tick"])
        if tick <= prev_tick:
            problems.append(f"tick 非递增 {prev_tick}→{tick}")
        prev_tick = tick
        if int(row["phase"]) not in (0, 1, 2, 3):
            problems.append(f"t={tick} phase={row['phase']}")
        if not 0 <= int(row["alive_t"]) <= 5 or not 0 <= int(row["alive_ct"]) <= 5:
            problems.append(f"t={tick} alive={row['alive_t']}/{row['alive_ct']}")
        if not 0 <= int(row["bomb"][1]) <= 2560:
            problems.append(f"t={tick} bomb_timer={row['bomb'][1]}")
        problems += [item for index, player in enumerate(row["p"]) for item in _player_issues(tick, index, player)]
        problems += [f"t={tick} money={m}" for m in row["money"] if not 0 <= int(m) <= 16000]
    return [("状态不变量", not problems, "; ".join(problems[:4]) if problems else "ok")]


def check_motion(samples: list[dict], spec: dict, marks: list[dict]) -> list[tuple]:
    limit = float(spec.get("expect", {}).get("max_step_per_tick", 12.0))
    teleports = [
        (int(mark["tick"]), int(mark.get("player", 0)))
        for mark in marks
        if mark.get("name") == "teleport"
    ]
    worst = 0.0
    where = ""
    for before, after in zip(samples, samples[1:]):
        dt = int(after["tick"]) - int(before["tick"])
        if dt <= 0 or before["round"] != after["round"] or before["phase"] == 0 or after["phase"] == 0:
            continue
        for index in range(min(len(before["p"]), len(after["p"]))):
            pa, pb = before["p"][index], after["p"][index]
            if pa[5] == 0 or pb[5] == 0:
                continue
            if any(pl == index and int(before["tick"]) <= at < int(after["tick"]) for at, pl in teleports):
                continue
            dist = ((pb[0] - pa[0]) ** 2 + (pb[1] - pa[1]) ** 2 + (pb[2] - pa[2]) ** 2) ** 0.5
            speed = dist / dt
            if speed > worst:
                worst = speed
                where = f"t={after['tick']} p{index}"
    return [("单 tick 位移", worst <= limit, f"max={worst:.2f} u/tick（限 {limit}）@{where}")]


def check_bomb(samples: list[dict], transitions: list[dict], marks: list[dict], spec: dict) -> list[tuple]:
    expect = spec.get("expect", {})
    planted = next((int(m["tick"]) for m in marks if m.get("name") == "planted"), None)
    ended = next((t for t in transitions if t["from"] == 1 and t["to"] == 2), None)
    if planted is None or ended is None:
        return [("C4 时间线", False, "缺少 planted 标记或 Live→RoundEnd 转换")]
    delay = int(ended["tick"]) - planted
    want = int(expect.get("bomb_explode_delay", 2559))
    planted_samples = [s for s in samples if int(s["bomb"][0]) == 1]
    bad_rate = [
        (int(a["tick"]), int(b["tick"]))
        for a, b in zip(planted_samples, planted_samples[1:])
        if int(b["bomb"][1]) != int(a["bomb"][1]) - (int(b["tick"]) - int(a["tick"]))
    ]
    return [
        ("C4 起爆延时", delay == want, f"{delay}/{want} tick（安放后 {want / 64:.2f}s）"),
        ("C4 计时器速率", not bad_rate, f"异常 {bad_rate[:3]}" if bad_rate else "每 tick -1"),
        ("C4 胜方", int(ended["score_t"]) == 1 and int(ended["score_ct"]) == 0,
         f"score {ended['score_t']}:{ended['score_ct']}"),
    ]


def check_perf(meta: dict, spec: dict) -> list[tuple]:
    expect = spec.get("expect", {})
    budget = float(expect.get("max_ms_per_tick", 5.0))
    max_wall = float(expect.get("max_wall_ms", 300000))
    ms_per_tick = float(meta.get("ms_per_tick", 1e9))
    wall = float(meta.get("wall_ms", 1e9))
    return [
        ("性能 ms/tick", ms_per_tick <= budget, f"{ms_per_tick:.4f} <= {budget}"),
        ("性能 墙钟", wall <= max_wall, f"{wall:.0f}ms <= {max_wall:.0f}ms"),
    ]


def golden_payload(meta: dict, rounds: list[dict]) -> dict:
    return {
        "ticks": int(meta["ticks"]),
        "final": {
            "round": int(meta["round"]),
            "phase": int(meta["phase"]),
            "score_t": int(meta["score_t"]),
            "score_ct": int(meta["score_ct"]),
        },
        "rounds": [
            {key: item[key] for key in ("number", "freeze", "live", "end", "winner")}
            for item in rounds
        ],
    }


def check_golden(sid: str, payload: dict, update: bool) -> tuple[list[tuple], str | None]:
    path = GOLD / f"{sid}.json"
    gold = json.loads(path.read_text(encoding="utf-8")) if path.exists() else None
    if update:
        GOLD.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        return [("时间线 golden", True, "已更新")], str(path.relative_to(ROOT))
    if gold is None:
        return [("时间线 golden", True, "WARN 无 golden（首次请 --update）")], None
    diffs = []
    for key in ("ticks", "final"):
        if gold.get(key) != payload[key]:
            diffs.append(f"{key}: {gold.get(key)} → {payload[key]}")
    if gold.get("rounds") != payload.get("rounds"):
        diffs.append("rounds 序列不一致")
    return [("时间线 golden", not diffs, "; ".join(diffs[:3]) if diffs else "一致")], str(path.relative_to(ROOT))


def run_one(godot: Path, spec_path: Path, visual: bool, update: bool, determinism: bool) -> dict:
    spec = json.loads(spec_path.read_text(encoding="utf-8"))
    sid = str(spec.get("id", spec_path.stem))
    out_dir = ACTUAL / sid
    started = time.time()
    rc, meta, output = run_suite(godot, spec_path, out_dir, visual)
    elapsed = round(time.time() - started, 1)
    checks: list[tuple] = []
    artifacts = out_dir.relative_to(ROOT)
    if rc != 0 or not meta:
        tail = "\n".join(output.strip().splitlines()[-6:])
        return {
            "id": sid,
            "ok": False,
            "elapsed": elapsed,
            "checks": [("驱动进程", False, f"rc={rc}")],
            "failures": [f"驱动失败 rc={rc}", tail],
            "artifacts": str(artifacts),
        }
    samples = read_jsonl(out_dir / "timeline.jsonl")
    transitions = read_jsonl(out_dir / "transitions.jsonl")
    marks = read_jsonl(out_dir / "marks.jsonl")
    rounds = compute_rounds(transitions)
    checks += check_structure(meta, samples, transitions, spec)
    checks += check_invariants(samples)
    checks += check_motion(samples, spec, marks)
    if "bomb_ticks" in spec.get("expect", {}):
        checks += check_bomb(samples, transitions, marks, spec)
    checks += check_perf(meta, spec)
    golden_checks, golden_path = check_golden(sid, golden_payload(meta, rounds), update)
    checks += golden_checks
    if determinism:
        checks.append(_check_determinism(godot, spec_path, sid, visual))
    failures = [f"{name}: {detail}" for name, ok, detail in checks if not ok]
    if meta.get("failures"):
        failures += [f"驱动器: {item}" for item in meta["failures"]]
    for name, ok, detail in checks:
        print(f"[{sid}] {'PASS' if ok else 'FAIL'} {name} — {detail}")
    return {
        "id": sid,
        "ok": not failures,
        "elapsed": elapsed,
        "meta": meta,
        "rounds": rounds,
        "checks": [{"name": n, "ok": o, "detail": d} for n, o, d in checks],
        "failures": failures,
        "artifacts": str(artifacts),
        "golden": golden_path,
    }


def _check_determinism(godot: Path, spec_path: Path, sid: str, visual: bool) -> tuple:
    first = ACTUAL / sid
    second = ACTUAL / f"{sid}__repeat"
    rc, meta, output = run_suite(godot, spec_path, second, visual)
    if rc != 0 or not meta:
        return ("确定性（同场景跑两遍）", False, "第二遍驱动失败")
    same = True
    for name in ("transitions.jsonl", "timeline.jsonl"):
        a = (first / name).read_bytes() if (first / name).exists() else b""
        b = (second / name).read_bytes() if (second / name).exists() else b""
        if a != b:
            same = False
    return ("确定性（同场景跑两遍）", same, "逐字节一致" if same else "时间线不一致")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--scenario", default="", help="只跑指定 id")
    ap.add_argument("--visual", action="store_true", help="同时抓关键帧截图（窗口化，需 GPU）")
    ap.add_argument("--update", action="store_true", help="重建时间线 golden（人工门）")
    ap.add_argument("--determinism", action="store_true", help="同场景跑两遍比对时间线")
    args = ap.parse_args()
    started = time.time()
    run_scenarios.ensure_host()
    godot = run_scenarios.find_godot()
    specs = sorted(SPEC_DIR.glob("*.json"))
    if args.scenario:
        specs = [item for item in specs if item.stem == args.scenario]
    if not specs:
        print("没有时间线场景可运行")
        return 1
    results = [run_one(godot, path, args.visual, args.update, args.determinism) for path in specs]
    return write_report(results, started)


if __name__ == "__main__":
    sys.exit(main())
