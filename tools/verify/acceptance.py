"""最终验收：一次跑完构建/测试/场景/校准/地图 6 机位 golden/CI 并生成报告。

用法：python -X utf8 tools/verify/acceptance.py
说明：地图 6 机位 golden 需要窗口化 GPU（headless 不能抓图），因此验收机必须有显示环境。
"""
from __future__ import annotations

import json
import subprocess
import sys
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PY = sys.executable


def run(name, cmd):
    proc = subprocess.run(cmd, cwd=str(ROOT), capture_output=True, text=True,
                          encoding="utf-8", errors="replace", timeout=1800)
    tail = (proc.stdout or "").strip().splitlines()[-1:] or [""]
    ok = proc.returncode == 0
    print(f"[{name}] {'PASS' if ok else 'FAIL'}  {tail[0]}")
    return {"name": name, "ok": ok, "cmd": cmd, "detail": tail[0]}


def main():
    checks = []
    checks.append(run("ctest", ["ctest", "--test-dir", "build", "--output-on-failure"]))
    checks.append(run("scenarios", [PY, "-X", "utf8", "tools/verify/run_scenarios.py", "--headless"]))
    checks.append(run("map_goldens", [PY, "-X", "utf8", "tools/verify/run_map_goldens.py"]))
    checks.append(run("calibration", [PY, "-X", "utf8", "tools/calibrate/run_calibration.py"]))
    checks.append(run("reference", [PY, "-X", "utf8", "tools/verify/run_reference_tests.py"]))
    checks.append(run("ci", [PY, "-X", "utf8", "tools/ci/run_local.py"]))
    ok = all(c["ok"] for c in checks)
    out = ROOT / "log/acceptance"
    out.mkdir(parents=True, exist_ok=True)
    report = {"when": datetime.now().isoformat(timespec="seconds"), "ok": ok, "checks": checks}
    (out / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2),
                                     encoding="utf-8")
    (out / "report.md").write_text("# 验收报告\n\n" + "\n".join(
        f"- {'PASS' if c['ok'] else 'FAIL'} {c['name']}: {c['detail']}" for c in checks) + "\n",
        encoding="utf-8")
    print("acceptance:", "PASS" if ok else "FAIL", "->", out / "report.md")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
