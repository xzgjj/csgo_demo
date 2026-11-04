"""csgo_demo 本地 CI 检查器（git/预算/策略/素材/场景/日志/lint/测试）。"""

from __future__ import annotations

import hashlib
import os
import shutil
import sys

from checks import (
    FAIL, PASS, SKIP, ROOT, git, load_json, result, run, source_files,
)

POLICY = "ci/spec.json"


def _head_exists():
    return git("rev-parse", "--verify", "HEAD")[0] == 0


def _numstat():
    files, lines = set(), 0
    rc, out = git("diff", "--numstat", "HEAD")
    for row in out.splitlines():
        parts = row.split("\t")
        if len(parts) != 3:
            continue
        files.add(parts[2])
        lines += sum(int(x) for x in parts[:2] if x.isdigit())
    return files, lines


def _untracked():
    files, lines = set(), 0
    rc, out = git("status", "--porcelain")
    for row in out.splitlines():
        if not row.startswith("?? "):
            continue
        rel = row[3:].strip().strip('"')
        files.add(rel)
        p = ROOT / rel
        if p.is_file():
            lines += p.read_text(encoding="utf-8", errors="replace").count("\n") + 1
    return files, lines


def check_diff_budget(spec):
    if not _head_exists():
        return result("diff_budget", SKIP, "尚无提交")
    budgets = spec["budgets"]
    files, lines = _numstat()
    extra_files, extra_lines = _untracked()
    files |= extra_files
    lines += extra_lines
    approved = bool((os.environ.get("CI_APPROVED_BY") or "").strip())
    over = len(files) > budgets["changed_files"] or lines > budgets["diff_lines"]
    if over and not approved:
        detail = (f"文件 {len(files)}/{budgets['changed_files']}，行 {lines}/{budgets['diff_lines']}；"
                  "超限需 CI_APPROVED_BY")
        return result("diff_budget", FAIL, detail)
    return result("diff_budget", PASS, f"文件 {len(files)}，行 {lines}" + ("（批准例外）" if over else ""))


def _author_issues(spec):
    allowed_authors = set(spec["author"]["emails"]) | {spec["author"]["login"]}
    allowed_committers = set(spec["author"].get("allowed_committers", []))
    rc, out = git("log", "--format=%an%x00%ae%x00%cn%x00%ce%x00%B%x00END")
    issues = []
    for chunk in out.split("\x00END"):
        parts = chunk.strip("\n\x00").split("\x00")
        if len(parts) < 5:
            continue
        for who in parts[:2]:
            if who and who not in allowed_authors:
                issues.append(f"非授权作者: {who}")
        for who in parts[2:4]:
            if who and who not in allowed_committers:
                issues.append(f"非授权提交者: {who}")
        if "co-authored-by" in parts[4].lower():
            issues.append("提交包含 Co-authored-by")
    return issues


def check_author(spec):
    if not _head_exists():
        return result("author", SKIP, "尚无提交")
    issues = _author_issues(spec)
    return result("author", FAIL if issues else PASS,
                  "; ".join(issues[:5]) if issues else "作者/共同作者仅 xzgjj")


def check_protected_tests(spec):
    manifest = load_json(ROOT / "ci/protected_test_cases.json", {})
    required = set(manifest.get("required_assertions", []))
    files = set(manifest.get("protected_files", []))
    tests_dir = ROOT / "tests"
    if not tests_dir.exists() or not any(tests_dir.rglob("*")):
        return result("protected_tests", SKIP, "tests/ 为空（测试本地维护，按约定不入库）")
    issues = [f"缺失文件 {f}" for f in files if not (ROOT / f).exists()]
    texts = "\n".join(p.read_text(encoding="utf-8", errors="replace")
                      for p in tests_dir.rglob("*") if p.is_file())
    issues += [f"缺失用例 {i}" for i in required if i not in texts]
    return result("protected_tests", FAIL if issues else PASS,
                  "; ".join(issues[:5]) if issues else "受保护用例完整")


def _policy_hash(spec):
    h = hashlib.sha256()
    for base in spec["policy_paths"]:
        p = ROOT / base.rstrip("/")
        paths = [p] if p.is_file() else sorted(x for x in p.rglob("*") if x.is_file())
        for x in paths:
            rel = x.relative_to(ROOT).as_posix()
            if any(part in rel for part in ("__pycache__", ".pytest_cache", ".ruff_cache")):
                continue
            if rel in ("ci/baseline_snapshot.json", "ci/spec.json.bak"):
                continue
            h.update(x.read_text(encoding="utf-8", errors="replace").encode("utf-8"))
    return h.hexdigest()


def check_policy(spec):
    baseline = load_json(ROOT / "ci/baseline_snapshot.json", None)
    if baseline is None:
        return result("policy", SKIP, "尚无基线（下次运行生成）")
    issues = []
    for key, val in spec["budgets"].items():
        old = baseline.get("budgets", {}).get(key)
        if isinstance(old, (int, float)) and isinstance(val, (int, float)) and val > old:
            issues.append(f"预算放宽 {key}: {old} -> {val}")
    if _policy_hash(spec) != baseline.get("policy_hash"):
        approver = (os.environ.get("CI_POLICY_APPROVER") or "").strip()
        issues += ["规则文件已变更但未设置 CI_POLICY_APPROVER"] if not approver else []
        issues += ["审批人不能是开发者（单人仓库需人工记录例外）"] if approver == spec["author"]["login"] else []
    return result("policy", FAIL if issues else PASS,
                  "; ".join(issues) if issues else "规则未放宽、策略变更已审批")


def check_assets(spec):
    manifest = ROOT / "assets/manifest.json"
    if not manifest.exists():
        return result("assets", SKIP, "assets/manifest.json 尚未建立（Phase 0）")
    data = load_json(manifest, {})
    allow = set(spec["allowed_license_ids"])
    issues = []
    for item in data.get("assets", []):
        fields = ("id", "path", "license", "author", "source")
        issues += [f"缺少 {k}: {item.get('id', '?')}" for k in fields if not item.get(k)]
        issues += [f"许可不在白名单: {item.get('license')}"] if item.get("license") not in allow else []
        issues += [f"文件不存在: {item.get('path')}"] if not (ROOT / item.get("path", "")).exists() else []
    return result("assets", FAIL if issues else PASS,
                  "; ".join(issues[:5]) if issues else f"{len(data.get('assets', []))} 项素材合规")


def check_scenarios(spec):
    base = ROOT / "verification/scenarios"
    if not base.exists():
        return result("scenarios", SKIP, "verification/scenarios 尚未建立（Phase 1）")
    issues = []
    for p in base.rglob("*.json"):
        data = load_json(p, None)
        if not isinstance(data, dict):
            issues.append(f"{p.name}: 非法 JSON")
            continue
        issues += [f"{p.name}: 缺少 {k}" for k in ("id", "map", "script", "asserts") if k not in data]
    return result("scenarios", FAIL if issues else PASS,
                  "; ".join(issues[:5]) if issues else "场景 schema/引用正常")


def check_logs(spec):
    budgets = spec["budgets"]
    logdir = ROOT / "log"
    if not logdir.exists():
        return result("logs", SKIP, "log/ 不存在")
    files = [p for p in logdir.rglob("*") if p.is_file()]
    size = sum(p.stat().st_size for p in files)
    issues = []
    issues += [f"日志 {size} > {budgets['log_bytes']} 字节"] if size > budgets["log_bytes"] else []
    issues += [f"日志文件 {len(files)} > {budgets['log_files']}"] if len(files) > budgets["log_files"] else []
    return result("logs", FAIL if issues else PASS,
                  "; ".join(issues) if issues else f"{len(files)} 文件 / {size} 字节")


def _detect_lint_tools():
    gdformat = shutil.which("gdformat")
    venv = ROOT.parent / "web——game/.ci-venv/Scripts/gdformat.exe"
    if not gdformat and venv.exists():
        gdformat = str(venv)
    ruff = [sys.executable, "-m", "ruff"] if run([sys.executable, "-m", "ruff", "--version"], timeout=20)[0] == 0 else None
    return shutil.which("clang-format"), gdformat, ruff


def _lint_issues(spec, tools):
    clang, gdformat, ruff = tools
    issues, checked = [], 0
    for p, rel in source_files(spec):
        checks = []
        if p.suffix in (".cpp", ".hpp", ".h", ".cc") and clang:
            checks.append(([clang, "--dry-run", "--Werror", str(p)], f"{rel}: clang-format"))
        if p.suffix == ".gd" and gdformat:
            checks.append(([gdformat, "--check", str(p)], f"{rel}: gdformat"))
        if p.suffix == ".py" and ruff:
            checks.append((ruff + ["check", str(p)], f"{rel}: ruff"))
        for cmd, label in checks:
            checked += 1
            issues += [label] if run(cmd, timeout=60)[0] != 0 else []
    return issues, checked


def check_lint(spec):
    tools = _detect_lint_tools()
    issues, checked = _lint_issues(spec, tools)
    rc, out = run([sys.executable, "-m", "compileall", "-q", "tools/ci", "build.py"], timeout=120)
    issues += ["Python 编译检查失败: " + out.strip()[:120]] if rc != 0 else []
    installed = [n for n, t in zip(("clang-format", "gdformat", "ruff"), tools) if t]
    if not installed:
        status = FAIL if issues else SKIP
        return result("lint", status, "; ".join(issues) if issues else "无可用的 clang-format/gdformat/ruff")
    return result("lint", FAIL if issues else PASS,
                  "; ".join(issues[:5]) if issues else f"用 {','.join(installed)} 检查 {checked} 个文件")


def check_tests(spec):
    tests_dir = ROOT / "tests"
    if tests_dir.exists() and any(tests_dir.rglob("test_*.py")):
        rc, out = run([sys.executable, "-m", "pytest", "-q", "tests"], timeout=600)
        tail = (out.strip().splitlines() or [""])[-1]
        return result("tests", PASS if rc == 0 else FAIL, f"pytest: {tail}")
    build = ROOT / "build"
    if build.exists() and any(build.rglob("CTestTestfile.cmake")):
        rc, out = run(["ctest", "--test-dir", "build", "--output-on-failure"], timeout=600)
        tail = (out.strip().splitlines() or [""])[-1]
        return result("tests", PASS if rc == 0 else FAIL, f"ctest: {tail}")
    return result("tests", SKIP, "暂无测试（tests/ 或 build/ 为空）")


ALL_CHECKS = [
    check_diff_budget, check_author, check_protected_tests, check_policy,
    check_assets, check_scenarios, check_logs, check_lint, check_tests,
]
RETRYABLE = {"tests"}
