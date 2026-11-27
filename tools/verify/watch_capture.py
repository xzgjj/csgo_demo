"""持续截图/断言看护（SPEC-02/03/04 的"自动持续截图更新与重新加载"）。

监视 core/、host/、content/defs/ 的源码 mtime；有变化就：
  1) 增量构建内核与 GDExtension（build.py --core/--host）
  2) 运行状态场景（tools/verify/run_scenarios.py）
  3) 渲染截图并比对 golden（tools/verify/run_all.py）
  4) 写 log/verify/watch/latest.md（最近一轮结果 + 变更文件）
Godot 只在每轮 run_scenarios/run_all 内部按脚本启动一次，不反复开关；
`--once` 用于 CI/人工单轮校验（不常驻）。

用法：
  python -X utf8 tools/verify/watch_capture.py            # 常驻看护（Ctrl+C 退出）
  python -X utf8 tools/verify/watch_capture.py --once     # 单轮
  python -X utf8 tools/verify/watch_capture.py --update-golden  # 单轮并更新 golden（人工门）
"""

from __future__ import annotations

import argparse
import hashlib
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WATCH_DIRS = ("core/include", "core/src", "core/tests", "host/scripts", "host/gdextension", "content/defs")
WATCH_SUFFIXES = {".cpp", ".h", ".gd", ".json", ".py", ".tscn"}
OUT_DIR = ROOT / "log/verify/watch"
POLL_SECONDS = 2.0


def snapshot() -> dict[str, tuple[int, int]]:
    result: dict[str, tuple[int, int]] = {}
    for folder in WATCH_DIRS:
        base = ROOT / folder
        if not base.exists():
            continue
        for path in base.rglob("*"):
            if path.is_file() and path.suffix in WATCH_SUFFIXES:
                stat = path.stat()
                result[str(path.relative_to(ROOT))] = (stat.st_mtime_ns, stat.st_size)
    return result


def digest(state: dict[str, tuple[int, int]]) -> str:
    payload = "\n".join(f"{key}:{value[0]}:{value[1]}" for key, value in sorted(state.items()))
    return hashlib.sha1(payload.encode("utf-8")).hexdigest()[:12]


def changed_files(before: dict[str, tuple[int, int]], after: dict[str, tuple[int, int]]) -> list[str]:
    changed = [key for key in after if key not in before or before[key] != after[key]]
    removed = [key for key in before if key not in after]
    return sorted(changed + [f"{key} (删除)" for key in removed])


def run(command: list[str], timeout: int = 600) -> tuple[int, str]:
    print(f"[watch] RUN {' '.join(command)}")
    try:
        proc = subprocess.run(command, cwd=ROOT, capture_output=True, timeout=timeout,
                              encoding="utf-8", errors="replace")
    except subprocess.TimeoutExpired:
        return 124, "超时"
    text = (proc.stdout or "") + (proc.stderr or "")
    return proc.returncode, text[-2500:]


def write_report(rounds: int, files: list[str], steps: list[tuple[str, int, str]], ok: bool) -> Path:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    lines = [
        f"# 截图看护最新一轮（#{rounds}）",
        "",
        f"- 时间：{datetime.now().isoformat(timespec='seconds')}",
        f"- 结论：{'PASS' if ok else 'FAIL'}",
        f"- 变更文件（{len(files)}）：",
    ]
    lines += [f"  - `{name}`" for name in files[:20]]
    lines += ["", "## 步骤", "", "| 步骤 | 退出码 | 摘要 |", "|---|---|---|"]
    for name, code, tail in steps:
        summary = tail.strip().splitlines()[-1] if tail.strip() else ""
        lines.append(f"| {name} | {code} | {summary[:160]} |")
    path = OUT_DIR / "latest.md"
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return path


def one_round(update_golden: bool, rounds: int, files: list[str] | None = None) -> bool:
    steps: list[tuple[str, int, str]] = []
    for name, command in (
        ("core", [sys.executable, "-X", "utf8", "build.py", "--core"]),
        ("host", [sys.executable, "-X", "utf8", "build.py", "--host"]),
        ("scenarios", [sys.executable, "-X", "utf8", "tools/verify/run_scenarios.py"]),
    ):
        code, tail = run(command)
        steps.append((name, code, tail))
        if code != 0:
            break
    else:
        shots = [sys.executable, "-X", "utf8", "tools/verify/run_all.py"]
        if update_golden:
            shots.append("--update")
        code, tail = run(shots, timeout=900)
        steps.append(("shots", code, tail))
    ok = all(step_code == 0 for _, step_code, _ in steps)
    write_report(rounds, files or [], steps, ok)
    return ok


def main() -> int:
    parser = argparse.ArgumentParser(description="持续看护：构建 + 场景 + 截图 golden")
    parser.add_argument("--once", action="store_true", help="只跑一轮（CI 用）")
    parser.add_argument("--update-golden", action="store_true", help="本轮更新截图 golden（人工门）")
    parser.add_argument("--interval", type=float, default=POLL_SECONDS, help="轮询间隔（秒）")
    args = parser.parse_args()
    rounds = 0
    if args.once:
        rounds += 1
        ok = one_round(args.update_golden, rounds)
        print(f"[watch] 一轮结束：{'PASS' if ok else 'FAIL'} -> log/verify/watch/latest.md")
        return 0 if ok else 1

    before = snapshot()
    print(f"[watch] 起始快照 {len(before)} 个文件，轮询 {args.interval}s；Ctrl+C 退出")
    while True:
        time.sleep(args.interval)
        after = snapshot()
        if digest(after) == digest(before):
            continue
        files = changed_files(before, after)
        before = after
        rounds += 1
        print(f"[watch] 检测到 {len(files)} 个变更，开始第 {rounds} 轮")
        one_round(args.update_golden, rounds, files)
        # 重建后文件会再次变化，重新取快照避免自触发。
        before = snapshot()


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        raise SystemExit(0)
