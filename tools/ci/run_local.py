"""本地 CI 运行器：读取 ci/spec.json，执行全部检查，维护基线。

用法：
  python -X utf8 tools/ci/run_local.py
  python -X utf8 tools/ci/run_local.py --strict --retry 3
  python -X utf8 tools/ci/run_local.py --update-baseline
退出码：0 全过（或仅 SKIP/WARN）；1 有 FAIL（或 --strict 下存在 SKIP）。
"""

from __future__ import annotations

import argparse
import json
import sys
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "ci"))

import checks  # noqa: E402
import checks_git  # noqa: E402

CORE_CHECKS = [
    checks.check_env,
    checks.check_structure,
    checks.check_docs,
    checks.check_duplicates,
    checks.check_complexity,
    checks.check_dependency,
]
ALL_CHECKS = CORE_CHECKS + checks_git.ALL_CHECKS


def load_spec():
    return json.loads((ROOT / "ci/spec.json").read_text(encoding="utf-8"))


def baseline_path():
    return ROOT / "ci/baseline_snapshot.json"


def write_baseline(spec):
    data = {
        "version": 1,
        "created_at": datetime.now().isoformat(timespec="seconds"),
        "commit": checks.git("rev-parse", "HEAD")[1].strip(),
        "budgets": spec["budgets"],
        "policy_hash": checks_git._policy_hash(spec),
        "protected": json.loads((ROOT / "ci/protected_test_cases.json").read_text(encoding="utf-8")),
    }
    baseline_path().write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"[baseline] 已生成 {baseline_path().relative_to(ROOT)}")


def run_one(fn, spec, retry):
    cid = fn.__name__.replace("check_", "")
    tries = 1 + (min(retry, spec.get("retries", 3)) if cid in checks_git.RETRYABLE else 0)
    last = None
    for _ in range(tries):
        last = fn(spec)
        if last["status"] != checks.FAIL:
            break
    return last


def print_results(results, spec, strict):
    width = max(len(r["id"]) for r in results) + 2
    print("\n== 本地 CI ==")
    for r in results:
        print(f"{r['id']:<{width}} {r['status']:<5} {r['detail']}")
    manual = [f"{r['id']}: {r['name']}" for r in spec.get("rules", []) if r["owner"] == "human"]
    print("\n== 需人工判断 ==")
    for item in manual:
        print(f"- {item}")
    fails = [r for r in results if r["status"] == checks.FAIL]
    skips = [r for r in results if r["status"] == checks.SKIP]
    print(f"\nsummary: {len(results)} checks, PASS {len(results) - len(fails) - len(skips)}, "
          f"FAIL {len(fails)}, SKIP {len(skips)}")
    if fails:
        print("failed: " + ", ".join(r["id"] for r in fails))
    if strict and skips:
        print("strict: SKIP 视为失败 -> " + ", ".join(r["id"] for r in skips))
    return fails, skips


def write_report(results, spec, out_path):
    out = ROOT / out_path
    out.parent.mkdir(parents=True, exist_ok=True)
    report = {
        "when": datetime.now().isoformat(timespec="seconds"),
        "commit": checks.git("rev-parse", "HEAD")[1].strip(),
        "checks": results,
        "manual": [r["id"] for r in spec.get("rules", []) if r["owner"] == "human"],
    }
    out.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--strict", action="store_true", help="SKIP 视为失败")
    ap.add_argument("--retry", type=int, default=0, help="仅对测试类检查重试，上限由 spec.retries 控制")
    ap.add_argument("--update-baseline", action="store_true", help="重新生成基线（需人工批准）")
    ap.add_argument("--json", default="log/ci/report.json", help="报告输出路径")
    args = ap.parse_args()

    spec = load_spec()
    first = not baseline_path().exists()
    if args.update_baseline and not first:
        print("[baseline] 警告：覆盖已有基线需人工批准并记录 diff.md")
    if args.update_baseline:
        write_baseline(spec)

    results = [run_one(fn, spec, args.retry) for fn in ALL_CHECKS]
    if first and not args.update_baseline:
        write_baseline(spec)

    fails, skips = print_results(results, spec, args.strict)
    write_report(results, spec, args.json)
    return 1 if (fails or (args.strict and skips)) else 0


if __name__ == "__main__":
    sys.exit(main())
