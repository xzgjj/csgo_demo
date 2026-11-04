"""统一构建入口（csgo_demo）。

用法：
  python -X utf8 build.py --env      # 环境基线
  python -X utf8 build.py --core     # 配置并构建 C++ 内核（core/ 存在时）
  python -X utf8 build.py --test     # 运行测试（ctest 或 pytest）
  python -X utf8 build.py --ci       # 本地 CI
  python -X utf8 build.py --clean    # 清理构建产物
  python -X utf8 build.py --all      # env + core + test + ci
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
BUILD = ROOT / "build"
PY = sys.executable
GODOT_CANDIDATES = [
    Path(r"D:\app_finish\Godot\Godot_v4.7.2-stable_win64.exe"),
    Path(r"D:\app_finish\Godot\Godot_v4.7.2-stable_win64_console.exe"),
]


def run(cmd, check=False):
    print("RUN:", " ".join(str(c) for c in cmd), flush=True)
    rc = subprocess.call([str(c) for c in cmd], cwd=str(ROOT))
    if check and rc != 0:
        raise SystemExit(rc)
    return rc


def godot_bin():
    import os
    env = os.environ.get("GODOT_BIN")
    if env:
        return Path(env) if Path(env).exists() else None
    for cand in GODOT_CANDIDATES:
        if cand.exists():
            return cand
    return None


def do_env():
    print(f"python : {sys.version.split()[0]} ({sys.executable})")
    for name in ("g++", "cmake", "ninja"):
        exe = shutil.which(name)
        if exe:
            out = subprocess.run([exe, "--version"], capture_output=True, text=True,
                                 encoding="utf-8", errors="replace").stdout
            print(f"{name:<7}: {exe} ({out.splitlines()[0] if out else '?'})")
        else:
            print(f"{name:<7}: 未找到")
    g = godot_bin()
    print(f"godot  : {g if g else '未找到（设置 GODOT_BIN）'}")
    if g:
        out = subprocess.run([str(g), "--version"], capture_output=True, text=True,
                             encoding="utf-8", errors="replace").stdout.strip()
        print(f"         {out.splitlines()[0] if out else '?'}")


def do_core():
    cmake_file = ROOT / "core" / "CMakeLists.txt"
    if not cmake_file.exists():
        print("[core] core/CMakeLists.txt 尚未建立（Phase 1 任务），跳过")
        return 0
    rc = run(["cmake", "-S", "core", "-B", "build", "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Debug"])
    if rc:
        return rc
    return run(["cmake", "--build", "build"])


def do_test():
    if (BUILD / "CTestTestfile.cmake").exists():
        return run(["ctest", "--test-dir", "build", "--output-on-failure"])
    if (ROOT / "tests").exists() and any((ROOT / "tests").rglob("test_*.py")):
        return run([PY, "-X", "utf8", "-m", "pytest", "-q", "tests"])
    print("[test] 暂无测试（tests/ 或 build/ 为空），跳过")
    return 0


def do_ci():
    return run([PY, "-X", "utf8", "tools/ci/run_local.py"])


def do_clean():
    targets = [BUILD, ROOT / "out"]
    for t in targets:
        if t.exists() and ROOT in t.parents:
            print(f"[clean] 删除 {t.relative_to(ROOT)}")
            shutil.rmtree(t)
    for pyc in ROOT.rglob("__pycache__"):
        if pyc.is_dir() and ROOT in pyc.parents:
            shutil.rmtree(pyc)
    print("[clean] 完成")
    return 0


def main():
    ap = argparse.ArgumentParser()
    for flag in ("--env", "--core", "--test", "--ci", "--clean", "--all"):
        ap.add_argument(flag, action="store_true")
    args = ap.parse_args()
    if not any(vars(args).values()):
        ap.print_help()
        return 0
    rc = 0
    if args.all:
        do_env()
        rc |= do_core()
        rc |= do_test()
        rc |= do_ci()
    else:
        if args.env:
            do_env()
        if args.core:
            rc |= do_core()
        if args.test:
            rc |= do_test()
        if args.ci:
            rc |= do_ci()
        if args.clean:
            do_clean()
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
