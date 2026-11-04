"""时间线报告落盘（从 run_timeline.py 拆出，保持单文件 ≤400 行）。"""

from __future__ import annotations

import json
import time
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "log/verify/timeline"


def _round_row(row: dict) -> str:
    score = row.get("score_after") or [0, 0]
    winner = row["winner"] or "—"
    return f"| {row['number']} | {row['freeze']} | {row['live']} | {row['end']} | {winner} | {score[0]}:{score[1]} |"


def _scenario_lines(item: dict) -> list[str]:
    meta = item.get("meta", {})
    lines = [
        f"## {item['id']} — {'PASS' if item['ok'] else 'FAIL'}",
        "",
        f"- ticks={meta.get('ticks')} samples={meta.get('samples')} wall={meta.get('wall_ms')}ms "
        f"ms/tick={meta.get('ms_per_tick')} level={meta.get('level')} 制品={item['artifacts']}",
    ]
    if item.get("golden"):
        lines.append(f"- golden：{item['golden']}")
    if item.get("rounds"):
        lines += ["", "| 回合 | freeze | live | round_end | 胜方 | 比分 |", "|---|---|---|---|---|---|"]
        lines += [_round_row(row) for row in item["rounds"]]
    lines += ["", "| 检查 | 结果 | 说明 |", "|---|---|---|"]
    lines += [f"| {c['name']} | {'PASS' if c['ok'] else 'FAIL'} | {c['detail']} |" for c in item["checks"]]
    if item["failures"]:
        lines += [""] + [f"- {reason}" for reason in item["failures"]]
    return lines + [""]


def write_report(results: list[dict], started: float) -> int:
    failures = [item for item in results if not item["ok"]]
    ok = not failures
    report = {
        "when": datetime.now().isoformat(timespec="seconds"),
        "ok": ok,
        "seconds": round(time.time() - started, 1),
        "scenarios": results,
        "failures": [f"{item['id']}: {reason}" for item in failures for reason in item["failures"]],
    }
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    lines = [
        "# 时间线验证报告",
        "",
        f"- 时间：{report['when']}（{report['seconds']}s）",
        f"- 结论：**{'PASS' if ok else 'FAIL'}**",
        f"- 场景数：{len(results)}",
        "",
    ]
    for item in results:
        lines += _scenario_lines(item)
    (OUT / "report.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"\nrun_timeline: {'PASS' if ok else 'FAIL'} -> {OUT / 'report.md'}")
    return 0 if ok else 1
