"""统一验证管线：运行 → 截图 → 比对 → 报告（制品 + 断言，非“看一眼”）。

用法：
  python -X utf8 tools/verify/run_all.py                # 全量：ctest/场景/运行时截图/6 机位/校准/reference/CI
  python -X utf8 tools/verify/run_all.py --build        # 先增量重建内核与 GDExtension
  python -X utf8 tools/verify/run_all.py --update       # 重建截图 golden（人工门，需记录 diff.md）
  python -X utf8 tools/verify/run_all.py --no-ci        # 跳过 CI（调试用）

Authoring/Runtime 分离：本脚本只启动**非编辑器**的 Godot CLI 进程做运行时截图与场景执行；
编辑器仅用于改场景/资源（MCP/编辑器自动化属 authoring 通道）。状态断言 headless，截图走窗口化
GPU CLI；所有比对结果、diff 图、指标、报告都落盘在 verification/actual 与 log/verify。
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/verify"))
import run_map_goldens  # noqa: E402
import run_scenarios  # noqa: E402

PY = sys.executable
MAP_VIEWS = ["overview_top", "A_site", "B_site", "mid", "T_spawn", "CT_spawn"]
RUNTIME_VIEWS = [
    "menu",
    "spawn",
    "gameplay",
    "fire",
    "weapon_0",
    "weapon_1",
    "weapon_2",
    "scoreboard",
    "pause",
    "settings",
]
MAP_GOLDENS = ROOT / "verification/goldens/map"
RUNTIME_GOLDENS = ROOT / "verification/goldens/runtime"
MAP_ACTUAL = ROOT / "verification/actual/map"
RUNTIME_ACTUAL = ROOT / "verification/actual/runtime"
DIFF_DIR = ROOT / "verification/actual/diff"
OUT = ROOT / "log/verify/run_all"
WEAPON_DISTINCT_MAE = 0.5


def run_cmd(cmd, timeout, label):
    start = time.time()
    proc = subprocess.run(
        [str(item) for item in cmd],
        cwd=str(ROOT),
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=timeout,
    )
    elapsed = round(time.time() - start, 2)
    tail = ((proc.stdout or "").strip().splitlines() or [""])[-3:]
    ok = proc.returncode == 0
    print(f"[{label}] {'PASS' if ok else 'FAIL'} ({elapsed}s) {' | '.join(tail)}")
    return {
        "name": label,
        "ok": ok,
        "rc": proc.returncode,
        "seconds": elapsed,
        "tail": tail,
        "cmd": [str(item) for item in cmd],
    }


def image_diff(actual: Path, golden: Path, out: Path) -> None:
    from PIL import Image, ImageChops

    diff = ImageChops.difference(
        Image.open(actual).convert("RGB"), Image.open(golden).convert("RGB")
    )
    out.parent.mkdir(parents=True, exist_ok=True)
    diff.save(out)


def compare_shot(kind, view, actual, golden_dir, thresholds, update):
    golden = golden_dir / f"{view}.png"
    entry = {
        "kind": kind,
        "view": view,
        "actual": str(actual.relative_to(ROOT)),
        "golden": str(golden.relative_to(ROOT)),
    }
    if not actual.exists():
        entry.update(ok=False, issues=["实际截图缺失"])
        print(f"[shot:{kind}:{view}] FAIL 缺少实际截图")
        return entry
    metrics = run_map_goldens.structure_metrics(actual)
    issues = run_map_goldens.structure_issues(metrics)
    entry["metrics"] = metrics
    if update:
        golden_dir.mkdir(parents=True, exist_ok=True)
        shutil.copy2(actual, golden)
        entry.update(ok=True, golden_updated=True)
        print(f"[shot:{kind}:{view}] PASS golden 已更新")
        return entry
    ok, detail = _compare_or_missing(entry, actual, golden, thresholds, issues)
    entry.update(ok=ok and not issues, compare=detail)
    if issues:
        entry["issues"] = issues
    print(
        f"[shot:{kind}:{view}] {'PASS' if entry['ok'] else 'FAIL'} {detail} metrics={metrics}"
    )
    return entry


def _compare_or_missing(entry, actual, golden, thresholds, issues):
    if not golden.exists():
        issues.append("缺少 golden（先跑 --update）")
        return False, "缺少 golden"
    ok, detail = run_scenarios.compare(actual, golden, thresholds)
    diff_path = DIFF_DIR / f"{entry['kind']}_{entry['view']}_diff.png"
    image_diff(actual, golden, diff_path)
    entry["diff"] = str(diff_path.relative_to(ROOT))
    if not ok:
        issues.append(f"golden 对比失败 {detail}")
    return ok, detail


def capture_map():
    godot = run_scenarios.find_godot()
    return run_cmd(
        [godot, "--path", "host", "--script", "res://tests/capture_de_ash.gd"],
        600,
        "capture_map",
    )


def capture_runtime():
    godot = run_scenarios.find_godot()
    RUNTIME_ACTUAL.mkdir(parents=True, exist_ok=True)
    start = time.time()
    proc = subprocess.run(
        [
            str(godot),
            "--path",
            "host",
            "--script",
            "res://tests/capture_suite.gd",
            "--",
            str(RUNTIME_ACTUAL),
        ],
        cwd=str(ROOT),
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=900,
    )
    elapsed = round(time.time() - start, 2)
    meta, shots = {}, 0
    for line in (proc.stdout or "").splitlines():
        if line.startswith("RUNTIME_META "):
            meta = json.loads(line[len("RUNTIME_META ") :])
        if line.startswith("RUNTIME_CAPTURE "):
            shots += 1
    ok = proc.returncode == 0
    print(
        f"[capture_runtime] {'PASS' if ok else 'FAIL'} ({elapsed}s) shots={shots} meta={meta}"
    )
    return (
        {
            "name": "capture_runtime",
            "ok": ok,
            "rc": proc.returncode,
            "seconds": elapsed,
            "detail": f"shots={shots} meta={meta}",
        },
        meta,
    )


def weapon_distinctness():
    from PIL import Image
    import numpy as np

    images = []
    for index in range(3):
        path = RUNTIME_ACTUAL / f"weapon_{index}.png"
        if not path.exists():
            return {"name": "weapon_distinct", "ok": False, "detail": "缺少武器截图"}
        images.append(np.asarray(Image.open(path).convert("RGB"), dtype=np.int16))
    pairs = []
    for a in range(3):
        for b in range(a + 1, 3):
            mae = float(np.abs(images[a] - images[b]).mean())
            pairs.append(
                {"pair": [a, b], "mae": round(mae, 3), "ok": mae >= WEAPON_DISTINCT_MAE}
            )
    ok = all(pair["ok"] for pair in pairs)
    print(f"[weapon_distinct] {'PASS' if ok else 'FAIL'} {pairs}")
    return {
        "name": "weapon_distinct",
        "ok": ok,
        "pairs": pairs,
        "detail": f"阈值 MAE>={WEAPON_DISTINCT_MAE}",
    }


def runtime_meta_assert(meta):
    issues = []
    world = str(meta.get("world", ""))
    if world != "de_ash_glb":
        issues.append(f"游戏内地图未使用 GLB：world={world}")
    glb = meta.get("glb", {})
    for weapon in ("ak", "m4", "awp"):
        if not glb.get(weapon):
            issues.append(f"武器 {weapon} 未使用 GLB（回退盒体）")
    if int(meta.get("player_count", 0)) != 10:
        issues.append(f"对局玩家数不是 10：{meta.get('player_count')}")
    ok = not issues
    print(f"[runtime_meta] {'PASS' if ok else 'FAIL'} {issues or meta}")
    return {"name": "runtime_meta", "ok": ok, "issues": issues, "meta": meta}


def phase_build_state(args):
    checks = []
    if args.build:
        checks.append(run_cmd(["cmake", "--build", "build"], 900, "build_core"))
        checks.append(run_cmd(["cmake", "--build", "build-host"], 900, "build_host"))
    checks.append(
        run_cmd(["ctest", "--test-dir", "build", "--output-on-failure"], 900, "ctest")
    )
    checks.append(
        run_cmd(
            [PY, "-X", "utf8", "tools/verify/run_scenarios.py", "--headless"],
            900,
            "state_scenarios",
        )
    )
    return checks


def phase_map(args, thresholds):
    checks, shots = [], []
    capture = capture_map()
    checks.append(capture)
    if not (capture["ok"] or args.update):
        return checks, shots
    MAP_ACTUAL.mkdir(parents=True, exist_ok=True)
    for view in MAP_VIEWS:
        src = ROOT / "host/captures" / f"de_ash_{view}.png"
        dst = MAP_ACTUAL / f"{view}.png"
        if src.exists():
            shutil.copy2(src, dst)
        shots.append(
            compare_shot("map", view, dst, MAP_GOLDENS, thresholds, args.update)
        )
    return checks, shots


def phase_runtime(args, thresholds):
    checks, shots = [], []
    runtime_check, meta = capture_runtime()
    checks.append(runtime_check)
    if not (runtime_check["ok"] or args.update):
        return checks, shots
    checks.append(runtime_meta_assert(meta))
    for view in RUNTIME_VIEWS:
        shots.append(
            compare_shot(
                "runtime",
                view,
                RUNTIME_ACTUAL / f"{view}.png",
                RUNTIME_GOLDENS,
                thresholds,
                args.update,
            )
        )
    if not args.update:
        checks.append(weapon_distinctness())
    return checks, shots


def phase_gates(args):
    checks = [
        run_cmd(
            [PY, "-X", "utf8", "tools/calibrate/run_calibration.py"], 600, "calibration"
        ),
        run_cmd(
            [PY, "-X", "utf8", "tools/verify/run_reference_tests.py"], 600, "reference"
        ),
    ]
    if not args.no_ci:
        checks.append(run_cmd([PY, "-X", "utf8", "tools/ci/run_local.py"], 1200, "ci"))
    return checks


def _collect_failures(checks, shots):
    shot_failures = [shot for shot in shots if not shot.get("ok", False)]
    failures = [check["name"] for check in checks if not check["ok"]]
    failures += [f"shot:{shot['kind']}:{shot['view']}" for shot in shot_failures]
    return failures


def _check_rows(checks):
    rows = []
    for check in checks:
        detail = " ".join(check.get("tail", []))[:120] or check.get("detail", "")
        rows.append(
            f"| {check['name']} | {'PASS' if check['ok'] else 'FAIL'} | {detail} |"
        )
    return rows


def _shot_rows(shots):
    rows = []
    for shot in shots:
        detail = shot.get(
            "compare", "golden 更新" if shot.get("golden_updated") else ""
        )
        rows.append(
            f"| {shot['kind']} | {shot['view']} | "
            f"{'PASS' if shot['ok'] else 'FAIL'} | {detail} |"
        )
    return rows


def write_report(args, checks, shots, started):
    failures = _collect_failures(checks, shots)
    ok = not failures
    report = {
        "when": datetime.now().isoformat(timespec="seconds"),
        "ok": ok,
        "seconds": round(time.time() - started, 2),
        "update": args.update,
        "checks": checks,
        "shots": shots,
        "failures": failures,
    }
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "report.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    lines = [
        "# 统一验证报告",
        "",
        f"- 时间：{report['when']}（{report['seconds']}s）",
        f"- 结论：**{'PASS' if ok else 'FAIL'}**",
        f"- 制品：actual=verification/actual，golden=verification/goldens，"
        f"diff={DIFF_DIR.relative_to(ROOT)}",
        "",
        "## 检查项",
        "",
        "| 检查 | 结果 | 说明 |",
        "|---|---|---|",
    ]
    lines += _check_rows(checks)
    lines += [
        "",
        "## 截图断言",
        "",
        "| 类型 | 机位/页面 | 结果 | 比对 |",
        "|---|---|---|---|",
    ]
    lines += _shot_rows(shots)
    lines += [""] + ([f"- {name}" for name in failures] if failures else [])
    (OUT / "report.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"\nrun_all: {'PASS' if ok else 'FAIL'} -> {OUT / 'report.md'}")
    return 0 if ok else 1


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", action="store_true", help="先增量重建内核与 GDExtension")
    ap.add_argument("--update", action="store_true", help="重建截图 golden（人工门）")
    ap.add_argument("--no-ci", action="store_true", help="跳过 CI 项")
    args = ap.parse_args()
    started = time.time()
    thresholds = run_scenarios.load_thresholds()
    checks = phase_build_state(args)
    map_checks, map_shots = phase_map(args, thresholds)
    runtime_checks, runtime_shots = phase_runtime(args, thresholds)
    checks += map_checks + runtime_checks + phase_gates(args)
    return write_report(args, checks, map_shots + runtime_shots, started)


if __name__ == "__main__":
    sys.exit(main())
