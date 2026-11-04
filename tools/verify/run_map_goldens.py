"""de_ash 6 机位截图 golden 比对（B 轨：感知投影检查）。

用法：
  python -X utf8 tools/verify/run_map_goldens.py            # 渲染 + 比对
  python -X utf8 tools/verify/run_map_goldens.py --update   # 重建 golden（人工门，需记录 diff.md）

流程：窗口化 Godot 跑 host/tests/capture_de_ash.gd（headless 不能抓图）→ 6 张图落到
verification/actual/map/ → 与 verification/goldens/map/<view>.png 比对像素/MAE/SSIM，
并做与 golden 无关的结构指标（亮度分位/方差/暗部占比/颜色数），两套结论都记入报告。
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/verify"))
import run_scenarios  # noqa: E402

VIEWS = ["overview_top", "A_site", "B_site", "mid", "T_spawn", "CT_spawn"]
CAPTURES = ROOT / "host/captures"
ACTUAL = ROOT / "verification/actual/map"
GOLDENS = ROOT / "verification/goldens/map"
REPORT = ROOT / "log/verify/map_goldens.json"
STRUCTURE_LIMITS = {"min_std": 8.0, "min_colors": 128, "max_dark_fraction": 0.75,
                    "min_mean": 20.0, "max_mean": 235.0}


def capture() -> bool:
    godot = run_scenarios.find_godot()
    proc = subprocess.run(
        [str(godot), "--path", "host", "--script", "res://tests/capture_de_ash.gd"],
        cwd=str(ROOT), capture_output=True, text=True, encoding="utf-8", errors="replace",
        timeout=300)
    missing = [view for view in VIEWS if not (CAPTURES / f"de_ash_{view}.png").exists()]
    if proc.returncode != 0 or missing:
        print(f"[map_goldens] 渲染失败 rc={proc.returncode} missing={missing}")
        return False
    return True


def structure_metrics(path: Path) -> dict:
    import numpy as np
    from PIL import Image

    image = np.asarray(Image.open(path).convert("RGB"), dtype=np.uint8)
    flat = image.reshape(-1, 3)
    gray = flat.mean(axis=1)
    colors = len(np.unique(flat.reshape(-1, 3), axis=0))
    dark = float((gray < 16.0).mean())
    return {
        "mean": float(gray.mean()),
        "std": float(gray.std()),
        "p5": float(np.percentile(gray, 5)),
        "p50": float(np.percentile(gray, 50)),
        "p95": float(np.percentile(gray, 95)),
        "dark_fraction": dark,
        "colors": int(colors),
    }


def structure_issues(metrics: dict) -> list[str]:
    issues = []
    if metrics["std"] < STRUCTURE_LIMITS["min_std"]:
        issues.append(f"画面过平 std={metrics['std']:.1f}")
    if metrics["colors"] < STRUCTURE_LIMITS["min_colors"]:
        issues.append(f"颜色过少 colors={metrics['colors']}")
    if metrics["dark_fraction"] > STRUCTURE_LIMITS["max_dark_fraction"]:
        issues.append(f"过暗像素过多 dark={metrics['dark_fraction']:.2f}")
    if not STRUCTURE_LIMITS["min_mean"] <= metrics["mean"] <= STRUCTURE_LIMITS["max_mean"]:
        issues.append(f"亮度均值越界 mean={metrics['mean']:.1f}")
    return issues


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--update", action="store_true", help="重建 golden（人工门）")
    args = ap.parse_args()
    thresholds = run_scenarios.load_thresholds()
    if not capture():
        return 1
    ACTUAL.mkdir(parents=True, exist_ok=True)
    GOLDENS.mkdir(parents=True, exist_ok=True)
    report = {"when": datetime.now().isoformat(timespec="seconds"), "views": [], "ok": True}
    for view in VIEWS:
        actual = ACTUAL / f"{view}.png"
        shutil.copy2(CAPTURES / f"de_ash_{view}.png", actual)
        golden = GOLDENS / f"{view}.png"
        metrics = structure_metrics(actual)
        entry = {"view": view, "metrics": metrics, "issues": structure_issues(metrics)}
        if args.update:
            shutil.copy2(actual, golden)
            entry["golden"] = "updated"
            print(f"[map_goldens] {view} golden 已更新 metrics={metrics}")
        elif not golden.exists():
            entry["issues"].append("缺少 golden（先跑 --update）")
        else:
            ok, detail = run_scenarios.compare(actual, golden, thresholds)
            entry["compare"] = detail
            entry["ok"] = ok
            if not ok:
                entry["issues"].append(f"golden 对比失败 {detail}")
            print(f"[map_goldens] {view} {'PASS' if ok else 'FAIL'} {detail} metrics={metrics}")
        if entry["issues"]:
            report["ok"] = False
            print(f"[map_goldens] {view} 结构问题: {entry['issues']}")
        report["views"].append(entry)
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print("map_goldens:", "PASS" if report["ok"] else "FAIL", "->", REPORT)
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
