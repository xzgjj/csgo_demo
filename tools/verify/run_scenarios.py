"""场景双检查执行器（代码断言 + 截图 golden）。

用法：
  python -X utf8 tools/verify/run_scenarios.py --visual            # 断言 + 截图对比
  python -X utf8 tools/verify/run_scenarios.py --headless          # 仅代码断言（CI）
  python -X utf8 tools/verify/run_scenarios.py --visual --update-golden
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GODOT_CANDIDATES = [
    Path(r"D:\app_finish\Godot\Godot_v4.7.2-stable_win64.exe"),
    Path(r"D:\app_finish\Godot\Godot_v4.7.2-stable_win64_console.exe"),
]
DEFAULT_THRESHOLDS = {"pixel_ratio": 0.002, "mae": 2.0, "ssim": 0.99}


def find_godot() -> Path:
    env = os.environ.get("GODOT_BIN")
    if env and Path(env).exists():
        return Path(env)
    for cand in GODOT_CANDIDATES:
        if cand.exists():
            return cand
    raise SystemExit("未找到 Godot；设置 GODOT_BIN 或安装到默认路径")


def load_thresholds() -> dict:
    path = ROOT / "verification/goldens/thresholds.json"
    if path.exists():
        data = json.loads(path.read_text(encoding="utf-8"))
        return {**DEFAULT_THRESHOLDS, **data}
    return DEFAULT_THRESHOLDS


def compare(actual_path: Path, golden_path: Path, thresholds: dict) -> tuple[bool, str]:
    try:
        import numpy as np
        from PIL import Image
        from skimage.metrics import structural_similarity
    except Exception as exc:  # pragma: no cover
        return False, f"缺少图像依赖: {exc}"

    actual = np.asarray(Image.open(actual_path).convert("RGB"), dtype=np.uint8)
    golden = np.asarray(Image.open(golden_path).convert("RGB"), dtype=np.uint8)
    if actual.shape != golden.shape:
        return False, f"尺寸不一致 {actual.shape} vs {golden.shape}"
    delta = np.abs(actual.astype(np.int16) - golden.astype(np.int16))
    pixel_ratio = float((delta > 8).any(axis=2).mean())
    mae = float(delta.mean())
    ssim = float(structural_similarity(actual, golden, channel_axis=2, data_range=255))
    ok = (pixel_ratio <= thresholds["pixel_ratio"] and mae <= thresholds["mae"]
          and ssim >= thresholds["ssim"])
    detail = f"pixel_ratio={pixel_ratio:.5f} mae={mae:.3f} ssim={ssim:.5f}"
    return ok, detail


def ensure_host() -> None:
    """内核静态库比宿主 DLL 新时自动重建，避免场景跑到旧逻辑。"""
    dll = ROOT / "host/bin/librecoil_host.windows.template_debug.x86_64.dll"
    core = ROOT / "build/librecoil_core.a"
    if not core.exists():
        return
    if dll.exists() and dll.stat().st_mtime >= core.stat().st_mtime:
        return
    subprocess.run(["cmake", "--build", "build-host"], cwd=str(ROOT), check=False)


def run_scenario(godot: Path, scenario: Path, out_dir: Path, visual: bool) -> tuple[bool, str]:
    args = [str(godot), "--path", "host", "--script", "res://tests/run_scenario.gd", "--",
            str(scenario.resolve()), str(out_dir.resolve())]
    if visual:
        args.append("--visual")
    else:
        args.insert(1, "--headless")
    env = dict(os.environ)
    env.setdefault("GODOT_SILENCE_ROOT_WARNING", "1")
    proc = subprocess.run(args, cwd=str(ROOT), capture_output=True, text=True,
                          encoding="utf-8", errors="replace", timeout=180, env=env)
    ok = proc.returncode == 0
    tail = "\n".join((proc.stdout or "").strip().splitlines()[-3:])
    return ok, tail or (proc.stderr or "").strip()[-300:]


def check_shot(sid: str, shot: dict, out_dir: Path, goldens: Path, thresholds: dict,
               update: bool) -> bool:
    name = str(shot.get("name", "shot")) + ".png"
    actual_path = out_dir / name
    golden_path = goldens / (name if name.startswith(sid) else f"{sid}_{name}")
    if not actual_path.exists():
        print(f"[{sid}] 缺少实际截图 {actual_path}")
        return False
    if update:
        goldens.mkdir(parents=True, exist_ok=True)
        shutil.copy2(actual_path, golden_path)
        print(f"[{sid}] golden 已更新 {golden_path.name}")
        return True
    if not golden_path.exists():
        print(f"[{sid}] 缺少 golden：{golden_path}（先跑 --update-golden）")
        return False
    ok, detail = compare(actual_path, golden_path, thresholds)
    print(f"[{sid}] 截图 {name} {'PASS' if ok else 'FAIL'}  {detail}")
    return ok


def run_one(godot: Path, scenario: Path, args) -> int:
    sid = scenario.stem
    out_dir = ROOT / "verification/actual" / sid
    ok, detail = run_scenario(godot, scenario, out_dir, args.visual)
    print(f"[{sid}] 代码断言 {'PASS' if ok else 'FAIL'}  {detail}")
    if not (args.visual and ok):
        return 0 if ok else 1
    data = json.loads(scenario.read_text(encoding="utf-8"))
    goldens = ROOT / "verification/goldens"
    failures = 0
    for shot in data.get("screenshots", []):
        if not check_shot(sid, shot, out_dir, goldens, load_thresholds(), args.update_golden):
            failures += 1
    return failures


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--headless", action="store_true", help="仅代码断言（CI）")
    ap.add_argument("--visual", action="store_true", help="同时做截图 golden 对比")
    ap.add_argument("--update-golden", action="store_true")
    ap.add_argument("--scenario", default="", help="只跑一个场景")
    args = ap.parse_args()
    if not args.headless and not args.visual:
        args.visual = True
    ensure_host()
    godot = find_godot()
    scenarios = sorted((ROOT / "verification/scenarios").glob("*.json"))
    if args.scenario:
        scenarios = [p for p in scenarios if p.stem == args.scenario]
    if not scenarios:
        print("没有场景可运行")
        return 0
    failures = sum(run_one(godot, scenario, args) for scenario in scenarios)
    print(f"summary: {len(scenarios)} scenarios, failures={failures}")
    return 0 if failures == 0 else 1



if __name__ == "__main__":
    sys.exit(main())
