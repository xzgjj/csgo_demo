"""csgo_demo 本地 CI 检查器（核心：环境/结构/文档/重复/复杂度/依赖）。

每个 check_* 返回 {"id","status","detail"}；PASS/FAIL/SKIP/WARN。
规则与阈值来自 ci/spec.json，本文件不写死策略。
"""

from __future__ import annotations

import ast
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PASS, FAIL, SKIP, WARN = "PASS", "FAIL", "SKIP", "WARN"
GODOT_CANDIDATES = [
    Path(r"D:\app_finish\Godot\Godot_v4.7.2-stable_win64.exe"),
    Path(r"D:\app_finish\Godot\Godot_v4.7.2-stable_win64_console.exe"),
]


def run(cmd, cwd=ROOT, timeout=180):
    try:
        p = subprocess.run(
            cmd, cwd=str(cwd), capture_output=True, text=True,
            encoding="utf-8", errors="replace", timeout=timeout,
        )
        return p.returncode, (p.stdout or "") + (p.stderr or "")
    except FileNotFoundError:
        return 127, "not found"
    except subprocess.TimeoutExpired:
        return 124, "timeout"


def git(*args, timeout=60):
    return run(["git", *args], timeout=timeout)


def result(cid, status, detail=""):
    return {"id": cid, "status": status, "detail": detail}


def load_json(path: Path, default=None):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return default


def excluded(spec, rel: str) -> bool:
    for item in spec["scan_exclude"]:
        e = item.strip("/")
        if rel == e or rel.startswith(e + "/"):
            return True
    return False


def iter_files(spec, subdirs=None):
    bases = [ROOT / d for d in subdirs] if subdirs else [ROOT]
    for base in bases:
        if not base.exists():
            continue
        for p in base.rglob("*"):
            if not p.is_file():
                continue
            rel = p.relative_to(ROOT).as_posix()
            if excluded(spec, rel):
                continue
            yield p, rel


def source_files(spec):
    exts = tuple(spec["source_exts"])
    return [(p, rel) for p, rel in iter_files(spec) if rel.endswith(exts)]


# ---------------- 环境 ----------------


def _godot_bin():
    for cand in GODOT_CANDIDATES:
        if cand.exists():
            return cand
    return None


def _gcc_issue(gxx, pins):
    rc, out = run([gxx, "--version"], timeout=30)
    m = re.search(r"(\d+)\.(\d+)", out)
    if not m or int(m.group(1)) < int(pins["gcc_min"]):
        return f"g++ 版本不足 {pins['gcc_min']}"
    return f"g++ {m.group(0)}"


def check_env(spec):
    pins = spec["pins"]
    issues, notes = [], []
    py = sys.version.split()[0]
    (issues if not py.startswith(pins["python"]) else notes).append(
        f"Python {py} != {pins['python']}" if not py.startswith(pins["python"]) else f"python {py}"
    )
    g = _godot_bin()
    if g:
        rc, out = run([str(g), "--version"], timeout=30)
        line = (out.strip().splitlines() or [""])[0]
        (issues if pins["godot"] not in line else notes).append(
            f"Godot {line} != {pins['godot']}" if pins["godot"] not in line else f"godot {line}"
        )
    else:
        notes.append("godot 未找到")
    gxx = shutil.which("g++") or shutil.which("g++.exe")
    if gxx:
        msg = _gcc_issue(gxx, pins)
        (issues if "不足" in msg else notes).append(msg)
    else:
        notes.append("g++ 未找到")
    cmake = shutil.which("cmake")
    notes.append(f"cmake {cmake}" if cmake else "cmake 未找到")
    return result("env", FAIL if issues else PASS, "; ".join(issues + notes))


# ---------------- 目录结构 ----------------


def _top_entries(spec):
    planning = set(spec["planning_docs"])
    ignore = set(spec["budgets"].get("top_level_ignore", [])) | planning
    ignore |= {".gitignore", ".git", "LICENSE", "README.md", "CI.md"}
    out = []
    for p in ROOT.iterdir():
        if p.name.startswith(".") or p.name in ignore or excluded(spec, p.name):
            continue
        out.append(p.name)
    return out


def _dir_problems(spec):
    issues, warns = [], []
    budgets = spec["budgets"]
    for p in ROOT.rglob("*"):
        if not p.is_dir():
            continue
        rel = p.relative_to(ROOT).as_posix()
        if excluded(spec, rel):
            continue
        if p.name in spec["forbidden_dirs"]:
            issues.append(f"禁止目录名: {rel}")
        depth = len(rel.split("/"))
        if depth > budgets["max_depth_special"]:
            issues.append(f"目录过深({depth}): {rel}")
        elif depth > budgets["max_depth"]:
            warns.append(f"目录较深({depth}): {rel}")
        files = [f for f in p.rglob("*") if f.is_file()]
        if len(files) < budgets["min_dir_files"]:
            warns.append(f"目录文件过少({len(files)}): {rel}")
    return issues, warns


def check_structure(spec):
    top = _top_entries(spec)
    issues, warns = _dir_problems(spec)
    if len(top) > spec["budgets"]["max_top_level"]:
        issues.append(f"顶层条目 {len(top)} > {spec['budgets']['max_top_level']}: {','.join(sorted(top))}")
    detail = "; ".join(issues + warns) if (issues or warns) else f"顶层 {len(top)} 条，结构正常"
    return result("structure", FAIL if issues else PASS, detail)


# ---------------- 文档 ----------------


def _planning_texts(spec):
    for rel in spec["planning_docs"]:
        p = ROOT / rel
        if p.exists():
            yield rel, p.read_text(encoding="utf-8")


def _doc_size_issues(spec):
    issues = []
    for rel, text in _planning_texts(spec):
        if len(text) > spec["budgets"]["planning_doc_chars"]:
            issues.append(f"{rel} {len(text)} > {spec['budgets']['planning_doc_chars']} 字符")
    for p, rel in iter_files(spec):
        if not rel.endswith(".md") or rel.startswith(tuple(spec["unbounded_doc_prefixes"])):
            continue
        if p.stat().st_size > spec["budgets"]["doc_bytes"]:
            issues.append(f"{rel} 超过 {spec['budgets']['doc_bytes']} 字节")
    return issues


def _duplicate_paragraphs(spec):
    issues = []
    for rel, text in _planning_texts(spec):
        paras = [re.sub(r"\s+", " ", x).strip().casefold() for x in re.split(r"\n\s*\n", text)]
        seen = set()
        for para in paras:
            if len(para) >= spec["budgets"]["duplicate_para_chars"] and para in seen:
                issues.append(f"{rel}: 重复段落 {len(para)} 字符")
            seen.add(para)
    return issues


def check_docs(spec):
    issues = _doc_size_issues(spec) + _duplicate_paragraphs(spec)
    notes = "; ".join(f"{rel}:{len(t)}" for rel, t in _planning_texts(spec))
    return result("docs", FAIL if issues else PASS, "; ".join(issues) if issues else notes)


# ---------------- 重复代码块 ----------------


def _norm_lines(text):
    out = []
    for ln in text.splitlines():
        s = re.sub(r"\s+", " ", ln.strip())
        if not s or s.startswith(("//", "#", "/*", "*", "--")):
            continue
        out.append(s)
    return out


def _scan_duplicate_blocks(spec):
    size = spec["budgets"]["duplicate_block_lines"]
    skip = tuple(spec.get("duplicates_exclude_prefixes", []))
    seen, hits = {}, []
    for p, rel in source_files(spec):
        if p.suffix.lower() in (".json", ".txt", ".cmake") or rel.startswith(skip):
            continue
        lines = _norm_lines(p.read_text(encoding="utf-8", errors="replace"))
        for i in range(max(0, len(lines) - size + 1)):
            key = "\n".join(lines[i:i + size])
            if len(key) < 40:
                continue
            prev = seen.get(key)
            if prev and prev[0] != rel:
                hits.append(f"{prev[0]}:{prev[1]} == {rel}:{i + 1}")
                return hits
            seen.setdefault(key, (rel, i + 1))
    return hits


def check_duplicates(spec):
    hits = _scan_duplicate_blocks(spec)
    size = spec["budgets"]["duplicate_block_lines"]
    return result("duplicates", FAIL if hits else PASS,
                  "; ".join(hits[:5]) if hits else f"重复块阈值 {size} 行：未发现")


# ---------------- 复杂度 ----------------


def _py_complexity(tree, limit):
    bad = []
    for node in ast.walk(tree):
        if not isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            continue
        count = 1 + sum(isinstance(c, (ast.If, ast.For, ast.While, ast.Try, ast.BoolOp,
                                       ast.comprehension, ast.IfExp))
                        for c in ast.walk(node))
        if count > limit:
            bad.append(f"{node.name}:{count}")
    return bad


def _file_issues(p, rel, spec):
    budgets = spec["budgets"]
    text = p.read_text(encoding="utf-8", errors="replace")
    issues = []
    if text.count("\n") + 1 > budgets["max_source_lines"]:
        issues.append(f"{rel} 超过 {budgets['max_source_lines']} 行")
    if p.suffix != ".py":
        return issues
    try:
        bad = _py_complexity(ast.parse(text), budgets["max_function_complexity"])
        issues.extend(f"{rel}:{x}" for x in bad)
    except SyntaxError as exc:
        issues.append(f"{rel} 语法错误: {exc}")
    return issues


def check_complexity(spec):
    issues = []
    for p, rel in source_files(spec):
        issues.extend(_file_issues(p, rel, spec))
    return result("complexity", FAIL if issues else PASS,
                  "; ".join(issues) if issues else "函数复杂度/文件行数在预算内")


# ---------------- 依赖方向 ----------------


def check_dependency(spec):
    bad = []
    for p, rel in iter_files(spec, ["core"]):
        if p.suffix.lower() not in (".h", ".hpp", ".cpp", ".cc", ".c"):
            continue
        for ln, line in enumerate(p.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            if re.match(r"\s*#\s*include\s*[<\"](Godot|godot|SDL|windows\.h|Windows\.h)", line):
                bad.append(f"{rel}:{ln}")
    return result("dependency", FAIL if bad else PASS,
                  "; ".join(bad[:5]) if bad else "core 无引擎/平台头依赖")
