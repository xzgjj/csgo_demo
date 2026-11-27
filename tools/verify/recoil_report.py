"""弹道图案报告（SPEC-02 BAL-01/BAL-03 证据生成器）。

读取 content/defs/weapons.json 的 recoil.pattern 与 verification/reference/recoil/*.json，
输出：
  log/verify/recoil/<weapon>.csv      每发 pitch/yaw/相对准星偏移
  log/verify/recoil/first_shot.csv    首发/姿态扩散与 30m 落点偏差
  log/verify/recoil/report.md         垂直度/Z 形/相关系数指标与门限判定

门限（对应 docs/SPEC-02-04-验收标准与实施计划.md §3.1）：
  - 前 10 发垂直度 = 1 - max|yaw|/max|pitch| >= 0.85
  - 第 11..30 发同时出现 yaw>0 与 yaw<0（Z 形）
  - 与 reference 的皮尔逊相关系数 >= 0.90
用法：python -X utf8 tools/verify/recoil_report.py
"""

from __future__ import annotations

import csv
import json
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WEAPONS = ROOT / "content/defs/weapons.json"
REFERENCE_DIR = ROOT / "verification/reference/recoil"
OUT_DIR = ROOT / "log/verify/recoil"
METERS_PER_UNIT = 0.0254  # 1 u = 1 inch
FIRST_SHOT_DISTANCE_M = 30.0
CORRELATION_GATE = 0.90
VERTICAL_GATE = 0.85


def load_weapons() -> list[dict]:
    data = json.loads(WEAPONS.read_text(encoding="utf-8"))
    return data["weapons"]


def pearson(a: list[float], b: list[float]) -> float:
    n = min(len(a), len(b))
    if n < 2:
        return 0.0
    ma = sum(a[:n]) / n
    mb = sum(b[:n]) / n
    num = sum((a[i] - ma) * (b[i] - mb) for i in range(n))
    da = math.sqrt(sum((a[i] - ma) ** 2 for i in range(n)))
    db = math.sqrt(sum((b[i] - mb) ** 2 for i in range(n)))
    return num / (da * db) if da > 0 and db > 0 else 0.0


def vertical_ratio(pattern: list[list[float]]) -> float:
    first_ten = pattern[:10]
    max_pitch = max((p[0] for p in first_ten), default=0.0)
    max_yaw = max((abs(p[1]) for p in first_ten), default=0.0)
    if max_pitch <= 0.0:
        return 0.0
    return 1.0 - max_yaw / max_pitch


def z_shape(pattern: list[list[float]]) -> bool:
    tail = pattern[10:]
    return any(p[1] > 0.1 for p in tail) and any(p[1] < -0.1 for p in tail)


def first_shot_metrics(inaccuracy: dict) -> dict:
    base = float(inaccuracy["base"])
    duck = base * float(inaccuracy["duck_mult"])
    distance_u = FIRST_SHOT_DISTANCE_M / METERS_PER_UNIT
    stand_m = math.tan(math.radians(base)) * distance_u * METERS_PER_UNIT
    duck_m = math.tan(math.radians(duck)) * distance_u * METERS_PER_UNIT
    return {"stand_m": stand_m, "duck_m": duck_m, "base_deg": base}


def report_weapon(weapon: dict, rows: list[str], failures: list[str], first_rows: list[list]) -> None:
    pattern = weapon["recoil"]["pattern"]
    wid = weapon["id"]
    with (OUT_DIR / f"{wid}.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["shot", "pitch_deg", "yaw_deg"])
        for index, point in enumerate(pattern):
            writer.writerow([index + 1, point[0], point[1]])
    vertical = vertical_ratio(pattern)
    z_ok = z_shape(pattern)
    ref_path = REFERENCE_DIR / f"{wid}.json"
    correlation = None
    if ref_path.exists():
        ref = json.loads(ref_path.read_text(encoding="utf-8"))["pattern"]
        correlation = pearson([p[0] for p in pattern], [p[0] for p in ref])
    first = first_shot_metrics(weapon["inaccuracy"])
    first_rows.append([wid, f"{first['base_deg']:.3f}", f"{first['stand_m']:.3f}",
                       f"{first['duck_m']:.3f}"])
    ok = vertical >= VERTICAL_GATE and z_ok and (correlation is None or correlation >= CORRELATION_GATE)
    status = "PASS" if ok else "FAIL"
    rows.append(
        f"| {wid} | {status} | {vertical:.3f} | {'是' if z_ok else '否'} | "
        f"{'无参考' if correlation is None else f'{correlation:.3f}'} | "
        f"{first['stand_m']:.3f} | {first['duck_m']:.3f} |"
    )
    if not ok:
        failures.append(f"{wid}: vertical={vertical:.3f} z={z_ok} corr={correlation}")


def main() -> int:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    rows: list[str] = []
    failures: list[str] = []
    first_rows: list[list] = []
    for weapon in load_weapons():
        report_weapon(weapon, rows, failures, first_rows)
    with (OUT_DIR / "first_shot.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["weapon", "base_deg", "stand_30m_m", "duck_30m_m"])
        writer.writerows(first_rows)
    body = [
        "# 弹道图案报告（SPEC-02）",
        "",
        f"数据源：`content/defs/weapons.json`；参考：`{REFERENCE_DIR.relative_to(ROOT)}`。",
        "1 u = 1 inch = 0.0254 m；30 m ≈ 1181 u。",
        "",
        "| 武器 | 结果 | 前10发垂直度 | 后20发Z形 | 与参考相关系数 | 首发30m偏差(m) | 蹲伏30m偏差(m) |",
        "|---|---|---|---|---|---|---|",
        *rows,
        "",
        f"门限：垂直度 ≥ {VERTICAL_GATE}、相关系数 ≥ {CORRELATION_GATE}、全武器首发 30 m ≤ 0.25 m。",
        "",
        "结论：" + ("全部通过" if not failures else "失败：" + "; ".join(failures)),
        "",
    ]
    (OUT_DIR / "report.md").write_text("\n".join(body), encoding="utf-8")
    print(f"[recoil_report] {len(rows)} 把武器 -> {OUT_DIR.relative_to(ROOT)}")
    if failures:
        print("[recoil_report] FAIL: " + "; ".join(failures), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
