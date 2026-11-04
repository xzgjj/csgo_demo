"""最终验收（薄包装）：统一验证管线 run_all.py（运行→截图→比对→报告），

覆盖 ctest / 状态场景 / 6 机位地图 golden / 运行时页面与武器截图 / 校准门 / reference / CI。

用法：python -X utf8 tools/verify/acceptance.py [--update]
"""

from __future__ import annotations

import json
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PY = sys.executable


def main() -> int:
    args = [PY, "-X", "utf8", "tools/verify/run_all.py"] + sys.argv[1:]
    proc = subprocess.run(
        args, cwd=str(ROOT), text=True, encoding="utf-8", errors="replace"
    )
    src = ROOT / "log/verify/run_all/report.json"
    out = ROOT / "log/acceptance"
    out.mkdir(parents=True, exist_ok=True)
    if src.exists():
        report = json.loads(src.read_text(encoding="utf-8"))
        shutil.copy2(src, out / "report.json")
        (out / "report.md").write_text(
            (ROOT / "log/verify/run_all/report.md").read_text(encoding="utf-8"),
            encoding="utf-8",
        )
        print(
            f"acceptance: {'PASS' if report.get('ok') else 'FAIL'} -> {out / 'report.md'}"
        )
    else:
        print("acceptance: FAIL（run_all 未生成报告）")
    return proc.returncode


if __name__ == "__main__":
    sys.exit(main())
