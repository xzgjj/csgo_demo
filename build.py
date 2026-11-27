"""统一构建入口（csgo_demo）。

用法：
  python -X utf8 build.py --env      # 环境基线
  python -X utf8 build.py --core     # 配置并构建 C++ 内核（core/ 存在时）
  python -X utf8 build.py --map      # 重建 de_dust2 资产并跑地图体检（check/audit/reach）
  python -X utf8 build.py --bless    # 人工确认画面后刷新 golden（trace + 运行时截图）
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
BUILD_TYPE = "Release"
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
    rc = run(["cmake", "-S", "core", "-B", "build", "-G", "Ninja", "-DCMAKE_BUILD_TYPE=" + BUILD_TYPE])
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


def do_verify():
    script = ROOT / "tools/verify/run_scenarios.py"
    if not script.exists():
        print("[verify] 无场景运行器，跳过")
        return 0
    return run([PY, "-X", "utf8", str(script), "--headless"])


def do_ci():
    return run([PY, "-X", "utf8", "tools/ci/run_local.py"])


def do_host():
    ext_cmake = ROOT / "host/gdextension/CMakeLists.txt"
    if not ext_cmake.exists():
        print("[host] host/gdextension/CMakeLists.txt 不存在，跳过")
        return 0
    if not (ROOT / "third_party/godot-cpp/build/bin").exists():
        print("[host] godot-cpp 未构建；先运行 tools/fetch_godot_cpp.ps1")
        return 1
    # 内核静态库与扩展
    rc = run(["cmake", "-S", "host/gdextension", "-B", "build-host", "-G", "Ninja",
              "-DCMAKE_BUILD_TYPE=" + BUILD_TYPE])
    if rc:
        return rc
    rc = run(["cmake", "--build", "build-host"])
    if rc:
        return rc
    # MinGW 运行时（GDExtension DLL 依赖）
    for dll in ("libstdc++-6.dll", "libgcc_s_seh-1.dll", "libwinpthread-1.dll"):
        src = Path("C:/msys64/ucrt64/bin") / dll
        if src.exists():
            shutil.copy2(src, ROOT / "host/bin" / dll)
    g = godot_bin()
    if not g:
        print("[host] 未找到 Godot；设置 GODOT_BIN 后重试")
        return 1
    rc = run([g, "--headless", "--path", "host", "--import"])
    if rc:
        return rc
    return run([g, "--headless", "--path", "host", "--script", "res://tests/smoke.gd"])



def do_map():
    """重建 de_dust2 三件套（layout/boxes/GLB）并跑地图体检。

    顺序固定：生成 → Blender 导出 → 几何一致性 → GLB 尺寸 → 遮挡审计 → 跳高可达性。
    任何一步红灯即停，避免“只重建了一半”的漂移资产进仓库。
    """
    checks = [
        ([PY, "-X", "utf8", "tools/mapgen/import_scene_v3.py"], "mapgen"),
        ([blender_bin(), "--background", "--python", "tools/blender/build_de_dust2.py"], "blender"),
        ([PY, "-X", "utf8", "tools/mapgen/check_dust2.py"], "check_dust2"),
        ([PY, "-X", "utf8", "tools/mapgen/check_glb_boxes.py", "--map", "de_dust2"], "glb_boxes"),
        ([PY, "-X", "utf8", "tools/verify/visual_audit.py", "--map", "de_dust2"], "visual_audit"),
        ([PY, "-X", "utf8", "tools/mapgen/check_reach.py", "--map", "de_dust2"], "check_reach"),
    ]
    for cmd, label in checks:
        rc = run(cmd)
        if rc:
            print(f"[map] {label} 失败，停止后续重建")
            return rc
    print("[map] de_dust2 重建与体检全部通过")
    return 0


def do_bless():
    """把当前画面固化为 golden：dust2 七机位 trace + 运行时页面截图。

    只在“人工确认画面无误”后运行；CI 不自动 bless，否则回归永远绿灯。
    """
    g = godot_bin()
    if not g:
        print("[bless] 未找到 Godot")
        return 1
    out = ROOT / "verification/actual/de_dust2"
    rc = run([g, "--path", "host", "--script", "res://tests/capture_map.gd", "--",
              "--map=de_dust2", "--out=" + str(out)])
    if rc:
        return rc
    rc = run([PY, "-X", "utf8", "tools/verify/trace_contract.py", "--map", "de_dust2", "--update"])
    if rc:
        return rc
    return run([PY, "-X", "utf8", "tools/verify/run_all.py", "--update", "--no-ci",
                "--map", "de_dust2"])


def blender_bin():
    import os
    env = os.environ.get("BLENDER_BIN")
    if env:
        return env
    for cand in (Path(r"D:\app_finish\blender\blender.exe"),):
        if cand.exists():
            return str(cand)
    return "blender"


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
    for flag in ("--env", "--core", "--host", "--map", "--bless", "--test", "--verify", "--ci",
                 "--clean", "--all"):
        ap.add_argument(flag, action="store_true")
    args = ap.parse_args()
    if not any(vars(args).values()):
        ap.print_help()
        return 0
    steps = [("env", do_env), ("core", do_core), ("map", do_map), ("host", do_host),
             ("test", do_test), ("verify", do_verify), ("bless", do_bless), ("ci", do_ci)]
    rc = 0
    for name, step in steps:
        if args.all or getattr(args, name):
            # do_env/do_clean 无返回值；其余步骤返回 0/非 0 累加，保证一次跑完所有阶段。
            rc |= step() or 0
    if args.clean:
        do_clean()
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
