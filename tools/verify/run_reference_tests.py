"""靶场数值断言：reference 偏差门 + demo_armor / demo_economy 场景实测。

断言分三层：
1. reference 门：`verification/reference/*.json` 的 gate 条目必须在容差内；
2. 期望值推导：由项目 Def + reference 推导命中伤害/护甲消耗/经济余额，与场景 JSON 的断言值比对；
3. 场景实测：用 `tools/verify/run_scenarios.py --headless --scenario <name>` 真跑内核。

用法：
  python -X utf8 tools/verify/run_reference_tests.py
  python -X utf8 tools/verify/run_reference_tests.py --no-scenario   # 只跑静态断言
退出码：0 全过（无宿主/Godot 时场景部分标 SKIP）；1 有失败。
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/calibrate"))

import project_defs  # noqa: E402
import reference  # noqa: E402

SCENARIO_DIR = ROOT / "verification/scenarios"
GODOT_CANDIDATES = (
    Path(r"D:\app_finish\Godot\Godot_v4.7.2-stable_win64_console.exe"),
    Path(r"D:\app_finish\Godot\Godot_v4.7.2-stable_win64.exe"),
)
MAX_HEALTH = 100
FULL_ARMOR = 100
ARMOR_LOSS_DIVISOR = 2  # core/src/step.cpp：护甲消耗 = 实际伤害 / 2
FREEZE_PHASE, LIVE_PHASE = 0, 1
PLAYER_SEPARATION = 400.0  # host/gdextension/recoil_host.cpp 靶子初始 x


def record(results: list[dict], cid: str, status: str, detail: str) -> None:
    results.append({"id": cid, "status": status, "detail": detail})
    print(f"[{status}] {cid}: {detail}")


def _load_scenario(name: str) -> dict:
    return json.loads((SCENARIO_DIR / f"{name}.json").read_text(encoding="utf-8"))


def _check_step(data: dict, name: str) -> list[dict]:
    for step in data.get("script", []):
        if step.get("action") == "check" and step.get("name") == name:
            return step.get("asserts", [])
    return []


def _expect(asserts: list[dict], player: int, field: str):
    for check in asserts:
        if int(check.get("player", 0)) == player and check.get("field") == field:
            return check.get("equals")
    return None


def _ref_value(entries: list[dict], entry_id: str) -> float:
    for entry in entries:
        if entry.get("id") == entry_id:
            return float(entry["value"])
    raise KeyError(f"reference 缺少条目 {entry_id}")


def _body_damage(project: dict[str, float], distance: float, armored: bool) -> int:
    """按 content 口径复算 0u 距离修正后的躯干单发伤害（四舍五入）。"""
    falloff = project["rifle.range_modifier"] ** (
        distance / project["rifle.range_modifier_unit"])
    ratio = project["rifle.armor_ratio"] if armored else 1.0
    return round(project["rifle.damage"] * falloff * ratio)


def _expectations(results: list[dict], cid: str, items: list[tuple[str, object, object]]) -> None:
    failures = []
    for label, actual, expected in items:
        if actual is None:
            failures.append(f"{label}: 场景缺少断言")
        elif actual != expected:
            failures.append(f"{label}: 期望 {expected} 实际 {actual}")
    detail = f"{len(items)} 条期望值一致" if not failures else "；".join(failures)
    record(results, cid, "PASS" if not failures else "FAIL", detail)


def check_reference_gate(project: dict[str, float], entries: list[dict],
                         results: list[dict]) -> None:
    report = reference.build_report(project)
    failures = reference.gate_failures(report)
    summary = report["summary"]
    domains = sorted({entry.get("domain", "?") for entry in entries})
    ok = (summary["reference_entries"] >= 60 and summary["gate_rows"] >= 30
          and len(domains) >= 4 and not failures)
    detail = (f"{summary['reference_entries']} 条参考 / {summary['rows']} 行；域 {len(domains)}；"
              f"门 {summary['gate_rows'] - len(failures)}/{summary['gate_rows']} PASS；"
              f"非门偏差 {summary['deviation']} 条")
    if failures:
        detail += "；门失败 " + ", ".join(row["id"] for row in failures[:5])
    record(results, "reference.gate", "PASS" if ok else "FAIL", detail)

    missing = [entry.get("id", "?") for entry in entries
               if not entry.get("tolerance") or not entry.get("source")]
    record(results, "reference.fields", "PASS" if not missing else "FAIL",
           f"{len(entries)} 条均含 tolerance/source" if not missing
           else f"缺 tolerance/source: {missing[:5]}")


def check_armor(project: dict[str, float], entries: list[dict], results: list[dict]) -> None:
    data = _load_scenario("demo_armor")
    distance = PLAYER_SEPARATION - project["movement.half_width"]
    armored = _body_damage(project, distance, True)
    unarmored = _body_damage(project, distance, False)
    armored_ref = round(_ref_value(entries, "ak47.armored_body_damage"))
    expectations = [
        ("有甲伤害=参考值", armored, armored_ref),
        ("有甲血量", _expect(_check_step(data, "armored_hit"), 1, "health"),
         MAX_HEALTH - armored),
        ("有甲护甲", _expect(_check_step(data, "armored_hit"), 1, "armor"),
         FULL_ARMOR - armored // ARMOR_LOSS_DIVISOR),
        ("无甲血量", _expect(_check_step(data, "unarmored_hit"), 0, "health"),
         MAX_HEALTH - unarmored),
        ("无甲护甲不变", _expect(_check_step(data, "unarmored_hit"), 0, "armor"), 0),
    ]
    _expectations(results, "range.armor", expectations)


def check_economy(project: dict[str, float], entries: list[dict], results: list[dict]) -> None:
    data = _load_scenario("demo_economy")
    start = _ref_value(entries, "start_money")
    price = _ref_value(entries, "armor_price")
    initial = _check_step(data, "initial_state")
    after_buy = _check_step(data, "after_buy")
    after_freeze = _check_step(data, "after_freeze")
    expectations = [
        ("初始金钱", _expect(initial, 0, "money"), start),
        ("初始阶段(Freeze)", _expect(initial, 0, "round_phase"), FREEZE_PHASE),
        ("初始冻结 tick", _expect(initial, 0, "round_ticks"), project["round.freeze_ticks"]),
        ("购买后金钱", _expect(after_buy, 0, "money"), start - price),
        ("购买后护甲", _expect(after_buy, 0, "armor"), FULL_ARMOR),
        ("冻结结束阶段(Live)", _expect(after_freeze, 0, "round_phase"), LIVE_PHASE),
        ("Live 剩余 tick", _expect(after_freeze, 0, "round_ticks"), project["round.live_ticks"]),
    ]
    _expectations(results, "range.economy", expectations)


def _find_godot() -> Path | None:
    env = os.environ.get("GODOT_BIN")
    if env and Path(env).exists():
        return Path(env)
    for candidate in GODOT_CANDIDATES:
        if candidate.exists():
            return candidate
    return None


def check_scenario_run(name: str, results: list[dict]) -> None:
    runner = ROOT / "tools/verify/run_scenarios.py"
    dlls = list((ROOT / "host/bin").glob("*.dll"))
    if not runner.exists() or _find_godot() is None or not dlls:
        record(results, f"scenario.{name}", "SKIP", "宿主/Godot 未就绪，跳过场景实测")
        return
    proc = subprocess.run(
        [sys.executable, "-X", "utf8", str(runner), "--headless", "--scenario", name],
        cwd=str(ROOT), capture_output=True, text=True, encoding="utf-8",
        errors="replace", timeout=300,
    )
    lines = (proc.stdout or "").strip().splitlines()
    tail = " ".join(line for line in lines[-2:] if line.strip())
    record(results, f"scenario.{name}", "PASS" if proc.returncode == 0 else "FAIL",
           (tail or (proc.stderr or "").strip())[-200:])


def main() -> int:
    parser = argparse.ArgumentParser(description="靶场数值断言")
    parser.add_argument("--no-scenario", action="store_true", help="只跑静态断言")
    args = parser.parse_args()

    project = project_defs.load_project()
    entries = reference.load_entries()
    results: list[dict] = []
    check_reference_gate(project, entries, results)
    check_armor(project, entries, results)
    check_economy(project, entries, results)
    if args.no_scenario:
        record(results, "scenario.runtime", "SKIP", "--no-scenario")
    else:
        for name in ("demo_armor", "demo_economy"):
            check_scenario_run(name, results)

    fails = [row for row in results if row["status"] == "FAIL"]
    skips = [row for row in results if row["status"] == "SKIP"]
    print(f"summary: {len(results)} checks, PASS {len(results) - len(fails) - len(skips)}, "
          f"FAIL {len(fails)}, SKIP {len(skips)}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
