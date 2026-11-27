"""de_dust2 常驻看护：数据变化 → 重建 → 截图/trace 双检测 → 重载预览。

Blender 不关闭：重建与取帧都经 tools/blender/blender.ps1（MCP 桥，GUI 常驻）。
预览只管理本脚本启动的 Godot 子进程，不触碰用户已开的编辑器。

用法：
  python -X utf8 tools/verify/watch_map.py --once --no-preview   # 单轮验证（CI/收口）
  python -X utf8 tools/verify/watch_map.py --interval 8          # 常驻：每 8s 检查一次 mtime
"""

from __future__ import annotations

import argparse
import json as js
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MAP_ID = "de_dust2"
WATCHED = [
    ROOT / "content/maps/de_dust2.layout.json",
    ROOT / "host/scenes/de_dust2_boxes.json",
    ROOT / "host/models/map/de_dust2.glb",
]
PREVIEW_PROC: subprocess.Popen | None = None


def run(cmd: list[str], label: str, timeout: int = 300) -> bool:
    print(f"[watch] {label} ...", flush=True)
    proc = subprocess.run(cmd, cwd=str(ROOT), capture_output=True, text=True,
                          encoding="utf-8", errors="replace", timeout=timeout)
    tail = (proc.stdout or "").strip().splitlines()[-4:]
    ok = proc.returncode == 0
    print(f"[watch] {label} {'PASS' if ok else 'FAIL'} {' | '.join(tail)}", flush=True)
    if not ok:
        print((proc.stderr or "")[-1000:], flush=True)
    return ok


def rebuild_geometry() -> bool:
    ok = run([sys.executable, "-X", "utf8", "tools/mapgen/import_scene_v3.py"], "layout")
    ok = run([sys.executable, "-X", "utf8", "tools/mapgen/check_dust2.py"], "check_dust2") and ok
    code = (
        "g={'__file__':r'" + str(ROOT / "tools/blender/build_de_dust2.py").replace("\\", "/") +
        "','__name__':'__main__'}; "
        "exec(compile(open(g['__file__'],encoding='utf-8').read(),'build','exec'),g)"
    )
    payload = js.dumps({"code": code})
    ok = run(["pwsh", "-NoProfile", "-File", str(ROOT / "tools/blender/blender.ps1"),
              "blender_py", payload], "blender_glb", 600) and ok
    ok = run([sys.executable, "-X", "utf8", "tools/mapgen/check_glb_boxes.py",
              "--map", MAP_ID], "check_glb") and ok
    return ok


def capture_and_check() -> bool:
    godot = find_godot()
    if godot is None:
        print("[watch] 未找到 Godot", flush=True)
        return False
    ok = run([godot, "--path", "host", "--script", "res://tests/capture_map.gd",
              "--", f"--map={MAP_ID}",
              "--out=" + str(ROOT / f"verification/actual/{MAP_ID}")], "capture", 600)
    ok = run([sys.executable, "-X", "utf8", "tools/verify/trace_contract.py",
              "--map", MAP_ID], "trace_contract") and ok
    return ok


def blender_frame() -> bool:
    script = ROOT / "tools/blender/dust2_frame.py"
    if not script.exists():
        return True
    code = (
        "g={'__file__':r'" + str(script).replace("\\", "/") +
        "','__name__':'__main__'}; "
        "exec(compile(open(g['__file__'],encoding='utf-8').read(),'frame','exec'),g)"
    )
    return run(["pwsh", "-NoProfile", "-File", str(ROOT / "tools/blender/blender.ps1"),
                "blender_py", js.dumps({"code": code})], "blender_frame", 300)


def find_godot() -> str | None:
    for path in (Path(r"D:\app_finish\Godot\Godot_v4.7.2-stable_win64.exe"),
                 Path("godot")):
        if path.exists() or path.name == "godot":
            return str(path)
    return None


def reload_preview(no_preview: bool) -> None:
    global PREVIEW_PROC
    if no_preview:
        return
    if PREVIEW_PROC is not None and PREVIEW_PROC.poll() is None:
        PREVIEW_PROC.terminate()
        try:
            PREVIEW_PROC.wait(timeout=5)
        except subprocess.TimeoutExpired:
            PREVIEW_PROC.kill()
    godot = find_godot()
    if godot is None:
        return
    PREVIEW_PROC = subprocess.Popen([godot, "--path", "host", "--", f"--map={MAP_ID}"],
                                    cwd=str(ROOT))


def mtimes() -> dict[str, float]:
    return {str(path): path.stat().st_mtime if path.exists() else 0.0 for path in WATCHED}


def one_round(no_blender: bool, no_preview: bool) -> bool:
    ok = rebuild_geometry()
    ok = capture_and_check() and ok
    if not no_blender:
        ok = blender_frame() and ok
    if ok:
        reload_preview(no_preview)
    print(f"[watch] round {'PASS' if ok else 'FAIL'}", flush=True)
    return ok


def main() -> int:
    parser = argparse.ArgumentParser(description="de_dust2 常驻重建/截图/重载")
    parser.add_argument("--once", action="store_true")
    parser.add_argument("--interval", type=float, default=8.0)
    parser.add_argument("--no-preview", action="store_true")
    parser.add_argument("--no-blender", action="store_true")
    args = parser.parse_args()
    if args.once:
        return 0 if one_round(args.no_blender, args.no_preview) else 1
    last = mtimes()
    one_round(args.no_blender, args.no_preview)
    while True:
        time.sleep(args.interval)
        current = mtimes()
        if current != last:
            last = current
            one_round(args.no_blender, args.no_preview)


if __name__ == "__main__":
    sys.exit(main())
