"""校验 assets/manifest.json：必填字段 / 许可白名单 / 文件存在 / 哈希 / 生成资产覆盖。

可被 CI 复用：`from check_manifest import validate_manifest`（返回问题列表，空列表即通过）。
命令行：python -X utf8 tools/assets/check_manifest.py [--json] [--root DIR]
退出码：0 通过；1 有问题；2 参数错误。
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MANIFEST_REL = "assets/manifest.json"
SPEC_REL = "ci/spec.json"
GENERATED_REL = "assets/generated"
REQUIRED_FIELDS = ("id", "path", "license", "author", "source")
FALLBACK_LICENSES = (
    "CC0-1.0",
    "PD",
    "PUBLIC-DOMAIN",
    "OFL-1.1",
    "MIT",
    "Apache-2.0",
    "Original",
    "Generated",
    "CC-BY-4.0",
)


def load_allowed_licenses(root: Path = ROOT) -> set[str]:
    try:
        spec = json.loads((root / SPEC_REL).read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return set(FALLBACK_LICENSES)
    allowed = spec.get("allowed_license_ids") or FALLBACK_LICENSES
    return set(allowed)


def sha256_of(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1 << 16), b""):
            digest.update(block)
    return "sha256:" + digest.hexdigest()


def load_manifest(root: Path = ROOT) -> tuple[dict | None, str]:
    path = root / MANIFEST_REL
    if not path.is_file():
        return None, f"缺少 {MANIFEST_REL}"
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except ValueError as exc:
        return None, f"{MANIFEST_REL} JSON 非法: {exc}"
    if not isinstance(data, dict):
        return None, f"{MANIFEST_REL} 顶层必须是对象"
    return data, ""


def _path_issues(root: Path, rel: str) -> list[str]:
    issues = []
    candidate = Path(rel)
    if candidate.is_absolute() or (len(rel) > 1 and rel[1] == ":"):
        return [f"path 必须相对项目根: {rel}"]
    if ".." in candidate.parts:
        issues.append(f"path 越界: {rel}")
    target = root / rel
    if not target.is_file():
        issues.append(f"文件不存在: {rel}")
    return issues


def _entry_issues(root: Path, index: int, item: dict, seen: dict[str, int],
                  allowed: set[str]) -> list[str]:
    label = str(item.get("id") or f"#{index}")
    issues = [f"{label} 缺字段 {field}" for field in REQUIRED_FIELDS if not item.get(field)]
    asset_id = str(item.get("id", ""))
    if asset_id and asset_id in seen:
        issues.append(f"id 重复: {asset_id}（#{seen[asset_id]} 与 #{index}）")
    seen.setdefault(asset_id, index)
    license_id = str(item.get("license", ""))
    if license_id and license_id not in allowed:
        issues.append(f"许可不在白名单: {license_id}（{label}）")
    rel = str(item.get("path", ""))
    if rel:
        issues += _path_issues(root, rel)
        expected = str(item.get("hash", ""))
        target = root / rel
        if expected and target.is_file() and sha256_of(target) != expected:
            issues.append(f"哈希不匹配: {rel}")
    return issues


def _duplicate_path_issues(root: Path, items: list, seen_paths: dict[str, int]) -> list[str]:
    issues = []
    for index, item in enumerate(items):
        rel = str(item.get("path", ""))
        if rel and rel in seen_paths:
            issues.append(f"path 重复: {rel}（#{seen_paths[rel]} 与 #{index}）")
        seen_paths.setdefault(rel, index)
    return issues


def _coverage_issues(root: Path, registered: set[str]) -> list[str]:
    generated = root / GENERATED_REL
    if not generated.is_dir():
        return []
    missing = []
    for path in sorted(generated.rglob("*")):
        if not path.is_file() or path.suffix.lower() not in (".png", ".jpg", ".exr"):
            continue
        rel = path.relative_to(root).as_posix()
        if rel not in registered:
            missing.append(f"未登记的生成资产: {rel}")
    return missing


def validate_manifest(root: Path = ROOT) -> list[str]:
    """返回问题列表；空列表表示 manifest 合规。"""
    data, error = load_manifest(root)
    if error:
        return [error]
    items = data.get("assets")
    if not isinstance(items, list) or not items:
        return [f"{MANIFEST_REL} 的 assets 为空或非数组"]
    allowed = load_allowed_licenses(root)
    issues: list[str] = []
    seen: dict[str, int] = {}
    for index, item in enumerate(items):
        if not isinstance(item, dict):
            issues.append(f"#{index} 不是对象")
            continue
        issues += _entry_issues(root, index, item, seen, allowed)
    issues += _duplicate_path_issues(root, items, {})
    registered = {str(item.get("path", "")) for item in items if isinstance(item, dict)}
    issues += _coverage_issues(root, registered)
    return issues


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="校验 assets/manifest.json")
    parser.add_argument("--root", default=str(ROOT), help="项目根目录")
    parser.add_argument("--json", action="store_true", help="输出 JSON 报告")
    args = parser.parse_args(argv)
    root = Path(args.root).resolve()
    data, error = load_manifest(root)
    if error:
        print(error)
        return 1
    issues = validate_manifest(root)
    count = len(data.get("assets") or [])
    payload = {
        "status": "PASS" if not issues else "FAIL",
        "manifest": MANIFEST_REL,
        "assets": count,
        "issues": issues,
    }
    if args.json:
        print(json.dumps(payload, ensure_ascii=False, indent=2))
    else:
        for issue in issues:
            print(" -", issue)
        print(f"manifestcheck {'PASS' if not issues else 'FAIL'} assets={count} issues={len(issues)}")
    return 0 if not issues else 1


if __name__ == "__main__":
    sys.exit(main())
