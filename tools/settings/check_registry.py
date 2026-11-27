"""设置注册表校验器（SPEC-05 §5.4.1）。

单一事实源：`content/defs/settings_registry.json`。本脚本只读，不改数据。
口径（SET-01）：
  - 8 页；每页有 id/title/groups；组有 id/title/items；
  - 全表 >= 60 项；id 全局唯一且前缀等于 page id；
  - type ∈ {bool, enum, slider, keybind, color, text}；
  - apply ∈ {immediate, restart, next_round}；target 属于热应用分发表；
  - 默认值必须落在自身类型/枚举/范围内；slider 必须 min<=default<=max 且 step>0。

用法：
  python -X utf8 tools/settings/check_registry.py
  python -X utf8 tools/settings/check_registry.py --csv log/verify/settings/registry.csv
退出码：0 通过；1 有违规（逐行打印 `文件:项:原因`）。
"""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REGISTRY = ROOT / "content" / "defs" / "settings_registry.json"

TYPE_SET = {"bool", "enum", "slider", "keybind", "color", "text"}
APPLY_SET = {"immediate", "restart", "next_round"}
TARGET_SET = {
    "window",
    "renderer",
    "env_adjustment",
    "camera",
    "audio",
    "input",
    "hud",
    "gameplay",
    "account",
}
MIN_PAGES = 8
MIN_ITEMS = 60
COLOR_PREFIX = "#"
MAX_KEYBIND_LEN = 12
CSV_FIELDS = ["page", "group", "id", "type", "apply", "target", "default"]


def load_registry(path: Path) -> tuple[dict, list[str]]:
    if not path.exists():
        return {}, [f"{path}:文件不存在"]
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        return {}, [f"{path}:JSON 解析失败（{exc}）"]
    if not isinstance(data, dict):
        return {}, [f"{path}:顶层必须是对象"]
    return data, []


def _check_bool_default(where: str, item: dict) -> list[str]:
    return [] if isinstance(item.get("default"), bool) else [f"{where}:bool 默认必须是 true/false"]


def _check_enum_default(where: str, item: dict) -> list[str]:
    values = item.get("values")
    if not isinstance(values, list) or not values:
        return [f"{where}:enum 缺少 values"]
    if item.get("default") not in values:
        return [f"{where}:默认 {item.get('default')!r} 不在枚举 {values}"]
    return []


def _check_slider_default(where: str, item: dict) -> list[str]:
    bounds = [item.get("min"), item.get("max"), item.get("step"), item.get("default")]
    if not all(isinstance(v, (int, float)) for v in bounds):
        return [f"{where}:slider 需要数值 min/max/step/default"]
    low, high, step, default = bounds
    if step <= 0 or low > high or not low <= default <= high:
        return [f"{where}:slider 范围非法 min={low} max={high} step={step}"]
    return []


def _check_keybind_default(where: str, item: dict) -> list[str]:
    default = item.get("default")
    if not isinstance(default, str) or not default.strip():
        return [f"{where}:keybind 默认必须是非空字符串"]
    if len(default.strip()) > MAX_KEYBIND_LEN:
        return [f"{where}:keybind 名称过长 {default!r}"]
    return []


def _check_color_default(where: str, item: dict) -> list[str]:
    default = item.get("default")
    if not isinstance(default, str) or not default.startswith(COLOR_PREFIX):
        return [f"{where}:color 默认必须是 #rrggbb"]
    return []


DEFAULT_CHECKS = {
    "bool": _check_bool_default,
    "enum": _check_enum_default,
    "slider": _check_slider_default,
    "keybind": _check_keybind_default,
    "color": _check_color_default,
}


def check_default(where: str, item: dict) -> list[str]:
    """按 type 分派默认值校验；text 无额外约束。"""
    checker = DEFAULT_CHECKS.get(str(item.get("type")))
    return [] if checker is None else checker(where, item)


def check_item(where: str, item: dict) -> list[str]:
    errors: list[str] = []
    for field in ("id", "label", "type", "default"):
        if field not in item:
            errors.append(f"{where}:缺少字段 {field}")
    if item.get("type") not in TYPE_SET:
        errors.append(f"{where}:未知 type {item.get('type')!r}")
    if item.get("apply") not in APPLY_SET:
        errors.append(f"{where}:apply 非法 {item.get('apply')!r}")
    if item.get("target") not in TARGET_SET:
        errors.append(f"{where}:target 非法 {item.get('target')!r}")
    if item.get("label") and not isinstance(item["label"], str):
        errors.append(f"{where}:label 必须是字符串")
    return errors + check_default(where, item)


def _item_row(pid: str, group_id: str, item: dict) -> dict:
    return {
        "page": pid,
        "group": group_id,
        "id": str(item.get("id", "")),
        "type": str(item.get("type", "")),
        "apply": str(item.get("apply", "")),
        "target": str(item.get("target", "")),
        "default": json.dumps(item.get("default"), ensure_ascii=False),
    }


def _check_item_id(pid: str, group_id: str, item: dict, seen_ids: set[str]) -> tuple[str, list[str]]:
    iid = str(item.get("id", ""))
    where = f"{pid}/{group_id}/{iid or '?'}"
    errors: list[str] = []
    if iid in seen_ids:
        errors.append(f"{where}:id 重复")
    seen_ids.add(iid)
    if not iid.startswith(pid + "."):
        errors.append(f"{where}:id 前缀必须等于页面 id")
    return where, errors


def _check_group(pid: str, group: dict, seen_ids: set[str], rows: list[dict]) -> list[str]:
    gid = str(group.get("id", ""))
    errors: list[str] = []
    if not gid:
        errors.append(f"page:{pid}/group:?:组 id 缺失")
    if not isinstance(group.get("title"), str):
        errors.append(f"page:{pid}/group:{gid}:组 title 缺失")
    items = group.get("items")
    if not isinstance(items, list) or not items:
        return errors + [f"page:{pid}/group:{gid}:items 为空"]
    for item in items:
        where, id_errors = _check_item_id(pid, gid, item, seen_ids)
        errors.extend(id_errors)
        errors.extend(check_item(where, item))
        rows.append(_item_row(pid, gid, item))
    return errors


def _check_page(page: dict, seen_pages: set[str], seen_ids: set[str], rows: list[dict]) -> list[str]:
    pid = str(page.get("id", ""))
    if not pid:
        return ["page:?:页面 id 缺失"]
    if pid in seen_pages:
        return [f"page:{pid}:页面 id 重复"]
    seen_pages.add(pid)
    errors: list[str] = []
    if not isinstance(page.get("title"), str):
        errors.append(f"page:{pid}:title 缺失")
    groups = page.get("groups")
    if not isinstance(groups, list) or not groups:
        return errors + [f"page:{pid}:groups 为空"]
    for group in groups:
        errors.extend(_check_group(pid, group, seen_ids, rows))
    return errors


def validate(data: dict) -> tuple[list[str], list[dict]]:
    errors: list[str] = []
    rows: list[dict] = []
    if data.get("schema") != "settings-registry/1":
        errors.append("registry:schema 必须是 settings-registry/1")
    pages = data.get("pages")
    if not isinstance(pages, list):
        return errors + ["registry:pages 必须是数组"], rows
    if len(pages) < MIN_PAGES:
        errors.append(f"registry:页面 {len(pages)} < {MIN_PAGES}")
    seen_pages: set[str] = set()
    seen_ids: set[str] = set()
    for page in pages:
        if not isinstance(page, dict):
            errors.append("registry:page 必须是对象")
            continue
        errors.extend(_check_page(page, seen_pages, seen_ids, rows))
    if len(rows) < MIN_ITEMS:
        errors.append(f"registry:设置项 {len(rows)} < {MIN_ITEMS}")
    return errors, rows


def write_csv(rows: list[dict], out: Path) -> None:
    out.parent.mkdir(parents=True, exist_ok=True)
    with out.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=CSV_FIELDS)
        writer.writeheader()
        writer.writerows(rows)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="设置注册表校验（SET-01）")
    parser.add_argument("--csv", default="log/verify/settings/registry.csv")
    parser.add_argument("--path", default=str(REGISTRY))
    args = parser.parse_args(argv)
    path = Path(args.path)
    data, errors = load_registry(path)
    rows: list[dict] = []
    if data:
        more_errors, rows = validate(data)
        errors.extend(more_errors)
    if not errors:
        write_csv(rows, ROOT / args.csv)
    page_count = len(data.get("pages", [])) if data else 0
    print(f"[settings] pages={page_count} items={len(rows)}")
    for error in errors:
        print(f"{path.name}:{error}")
    print("[settings] " + ("FAIL" if errors else "PASS"))
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
