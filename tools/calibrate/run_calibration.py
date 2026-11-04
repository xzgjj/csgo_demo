"""数值校准偏差报告：reference 表 vs 项目 Def，输出偏差/百分比/容差判定。

用法：
  python -X utf8 tools/calibrate/run_calibration.py            # 人类可读报告
  python -X utf8 tools/calibrate/run_calibration.py --json     # JSON 到 stdout
  python -X utf8 tools/calibrate/run_calibration.py --gate     # 门失败退出 1
报告同时写入 log/calibration/latest.json 与 log/calibration/<日期>.json。
"""

from __future__ import annotations

import argparse
import json
import sys
from datetime import date, datetime
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import project_defs  # noqa: E402
import reference  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
OUT_DIR = ROOT / "log/calibration"


def _fmt(value: object) -> str:
    if value is None:
        return "-"
    if isinstance(value, float):
        return f"{value:.2f}"
    return str(value)


def _tol_text(row: dict) -> str:
    tolerance = row["tolerance"]
    parts = []
    if "abs" in tolerance:
        parts.append(f"abs {tolerance['abs']}")
    if "rel" in tolerance:
        parts.append(f"rel {tolerance['rel'] * 100:.1f}%")
    return " / ".join(parts) or "-"


def print_rows(report: dict) -> None:
    groups: dict[str, list[dict]] = {}
    for row in report["rows"]:
        groups.setdefault(row["domain"], []).append(row)
    for domain, rows in groups.items():
        print(f"\n== {domain} ==")
        for row in rows:
            flag = " [gate]" if row["gate"] else ""
            print(f"{row['id']:<34} ref={_fmt(row['reference']):>9} "
                  f"actual={_fmt(row['actual']):>9} d={_fmt(row['delta']):>8} "
                  f"pct={_fmt(row['delta_pct']):>7}% tol={_tol_text(row):<18} "
                  f"{row['status']}{flag}")


def write_reports(report: dict) -> list[Path]:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    payload = {**report, "when": datetime.now().isoformat(timespec="seconds")}
    paths = [OUT_DIR / "latest.json", OUT_DIR / f"{date.today().isoformat()}.json"]
    for path in paths:
        path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
    return paths


def main() -> int:
    parser = argparse.ArgumentParser(description="reference 表 vs 项目 Def 偏差报告")
    parser.add_argument("--json", action="store_true", help="输出 JSON 报告")
    parser.add_argument("--gate", action="store_true", help="偏差门失败时退出 1")
    args = parser.parse_args()

    project = project_defs.load_project()
    report = reference.build_report(project)
    paths = write_reports(report)
    failures = reference.gate_failures(report)
    summary = report["summary"]

    if args.json:
        print(json.dumps(report, ensure_ascii=False, indent=2))
    else:
        print_rows(report)
        print(f"\nsummary: 参考 {summary['reference_entries']} 条 / 报告 {summary['rows']} 行；"
              f"PASS {summary['pass']}, 偏差 {summary['deviation']}, SKIP {summary['skip']}；"
              f"门 {summary['gate_rows'] - len(failures)}/{summary['gate_rows']} PASS")
        print("报告: " + ", ".join(str(path.relative_to(ROOT)) for path in paths))
        for row in failures:
            print(f"门失败: {row['id']} ref={_fmt(row['reference'])} "
                  f"actual={_fmt(row['actual'])} delta={_fmt(row['delta'])}")
    return 1 if (args.gate and failures) else 0


if __name__ == "__main__":
    sys.exit(main())
