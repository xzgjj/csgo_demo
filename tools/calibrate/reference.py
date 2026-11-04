"""参考表加载、偏差计算与偏差门判定。

参考表是唯一事实源：`verification/reference/*.json` 每条给出
`id / value / unit / project / tolerance / gate / grade / source`。
`project` 指向 `project_defs.load_project()` 的扁平字段；为 null 表示项目尚未实现。
"""

from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REFERENCE_DIR = ROOT / "verification/reference"
SCHEMA = "recoil.calibration/1"
PASS, DEVIATION, SKIP = "PASS", "DEVIATION", "SKIP"


def load_entries(directory: Path = REFERENCE_DIR) -> list[dict]:
    entries: list[dict] = []
    for path in sorted(directory.glob("*.json")):
        data = json.loads(path.read_text(encoding="utf-8"))
        for item in data.get("entries", []):
            entries.append({**item, "domain": data.get("domain", path.stem)})
    return entries


def _within_tolerance(delta: float, pct: float, entry: dict) -> bool:
    """绝对容差或相对容差任一满足即视为在容差内；容差缺省时要求精确相等。"""
    tolerance = entry.get("tolerance", {})
    if "abs" in tolerance and abs(delta) <= float(tolerance["abs"]):
        return True
    return "rel" in tolerance and abs(pct) <= float(tolerance["rel"]) * 100.0


def _row(entry: dict, status: str) -> dict:
    return {
        "id": entry.get("id", "?"),
        "domain": entry.get("domain", ""),
        "project": entry.get("project"),
        "reference": entry.get("value"),
        "unit": entry.get("unit", ""),
        "actual": None,
        "delta": None,
        "delta_pct": None,
        "tolerance": entry.get("tolerance", {}),
        "gate": bool(entry.get("gate", False)),
        "grade": entry.get("grade", ""),
        "source": entry.get("source", ""),
        "note": entry.get("note", ""),
        "status": status,
    }


def compare(entry: dict, project: dict[str, float]) -> dict:
    """单条参考 vs 项目值：返回含差值与容差判定的报告行。"""
    key = entry.get("project")
    if not key:
        row = _row(entry, SKIP)
        row["note"] = row["note"] or "项目未实现（无映射）"
        return row
    if key not in project:
        row = _row(entry, SKIP)
        row["note"] = f"项目缺少字段 {key}"
        return row
    reference = entry.get("value")
    if not isinstance(reference, (int, float)) or isinstance(reference, bool):
        row = _row(entry, SKIP)
        row["note"] = "参考值非数值（需分档实现）"
        return row
    actual = float(project[key])
    delta = actual - float(reference)
    pct = (delta / float(reference) * 100.0) if reference else 0.0
    row = _row(entry, PASS if _within_tolerance(delta, pct, entry) else DEVIATION)
    row.update({
        "actual": round(actual, 4),
        "delta": round(delta, 4),
        "delta_pct": round(pct, 2),
    })
    return row


def _project_only_row(key: str, value: float) -> dict:
    prefix = key.split(".")[0]
    domain = {"rifle": "weapons", "round": "economy"}.get(prefix, prefix)
    return {
        "id": f"project:{key}",
        "domain": domain,
        "project": key,
        "reference": None,
        "unit": "",
        "actual": round(float(value), 4),
        "delta": None,
        "delta_pct": None,
        "tolerance": {},
        "gate": False,
        "grade": "",
        "source": "",
        "note": "项目字段无参考（待校准）",
        "status": SKIP,
    }


def build_report(project: dict[str, float], directory: Path = REFERENCE_DIR) -> dict:
    entries = load_entries(directory)
    rows = [compare(entry, project) for entry in entries]
    referenced = {entry["project"] for entry in entries if entry.get("project")}
    for key in sorted(project):
        if key not in referenced:
            rows.append(_project_only_row(key, project[key]))
    gate_rows = [row for row in rows if row["gate"]]
    statuses = [row["status"] for row in rows]
    return {
        "schema": SCHEMA,
        "summary": {
            "rows": len(rows),
            "reference_entries": len(entries),
            "pass": statuses.count(PASS),
            "deviation": statuses.count(DEVIATION),
            "skip": statuses.count(SKIP),
            "gate_rows": len(gate_rows),
            "gate_failures": sum(1 for row in gate_rows if row["status"] != PASS),
        },
        "rows": rows,
    }


def gate_failures(report: dict) -> list[dict]:
    """门的失败行：标了 gate 且不在容差内（SKIP 同样算门失败）。"""
    return [row for row in report["rows"] if row["gate"] and row["status"] != PASS]
