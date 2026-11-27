"""Trace 契约 + 视觉双检测（MAP-12）。

同一机位截图同时验证两层：
  1) Trace：盒子哈希 / 三角面 / 材质数 / 相机位姿（确定性契约）
  2) 视觉：PSNR / SSIM（与人工审核过的 golden 比）
任一不过即失败，并输出并排对比图。

用法：
  python -X utf8 tools/verify/trace_contract.py --map de_dust2 --update   # 生成候选 golden（人工门）
  python -X utf8 tools/verify/trace_contract.py --map de_dust2            # 断言
"""

from __future__ import annotations

import argparse
import json
import math
import shutil
import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
TRACE_NAME = "trace.json"
PSNR_MIN = 28.0
SSIM_MIN = 0.90
POSE_POS_MAX = 2.0
POSE_ANGLE_MAX = 2.0
TRI_MAX = 67000
MATERIAL_MAX = 12


def load_trace(path: Path) -> dict:
    data = json.loads(path.read_text(encoding="utf-8"))
    for key in ("map", "boxes_sha256", "tri_count", "materials", "views"):
        if key not in data:
            raise ValueError(f"{path.name} 缺少字段 {key}")
    if len(str(data["boxes_sha256"])) != 64:
        raise ValueError("boxes_sha256 必须是 64 位十六进制")
    return data


def psnr(actual: np.ndarray, golden: np.ndarray) -> float:
    mse = float(np.mean((actual.astype(np.float64) - golden.astype(np.float64)) ** 2))
    if mse <= 1e-9:
        return 99.0
    return 10.0 * math.log10(255.0 ** 2 / mse)


def ssim(actual: np.ndarray, golden: np.ndarray) -> float:
    a = actual.astype(np.float64).mean(axis=2)
    b = golden.astype(np.float64).mean(axis=2)
    c1, c2 = (0.01 * 255) ** 2, (0.03 * 255) ** 2
    mu_a, mu_b = a.mean(), b.mean()
    var_a, var_b = a.var(), b.var()
    cov = ((a - mu_a) * (b - mu_b)).mean()
    return float(((2 * mu_a * mu_b + c1) * (2 * cov + c2)) /
                 ((mu_a ** 2 + mu_b ** 2 + c1) * (var_a + var_b + c2)))


def angle_delta(a: float, b: float) -> float:
    return abs((a - b + 180.0) % 360.0 - 180.0)


def compare_view(view: dict, base_view: dict, actual_dir: Path, golden_dir: Path,
                 out_dir: Path) -> dict:
    name = str(view["camera"])
    actual_path = actual_dir / str(view["file"])
    golden_path = golden_dir / str(view["file"])
    issues: list[str] = []
    if not actual_path.exists():
        return {"camera": name, "ok": False, "issues": ["实际截图缺失"]}
    if not golden_path.exists():
        return {"camera": name, "ok": False, "issues": ["缺少 golden"]}
    actual = np.asarray(Image.open(actual_path).convert("RGB"))
    golden = np.asarray(Image.open(golden_path).convert("RGB"))
    p = psnr(actual, golden)
    s = ssim(actual, golden)
    pos_delta = math.dist(view["pos"], base_view["pos"])
    yaw_delta = angle_delta(float(view["yaw"]), float(base_view["yaw"]))
    pitch_delta = angle_delta(float(view["pitch"]), float(base_view["pitch"]))
    if p < PSNR_MIN:
        issues.append(f"PSNR {p:.2f} < {PSNR_MIN}")
    if s < SSIM_MIN:
        issues.append(f"SSIM {s:.4f} < {SSIM_MIN}")
    if pos_delta > POSE_POS_MAX:
        issues.append(f"pose 位移 {pos_delta:.2f}u > {POSE_POS_MAX}u")
    if max(yaw_delta, pitch_delta) > POSE_ANGLE_MAX:
        issues.append(f"pose 角度 {max(yaw_delta, pitch_delta):.2f}° > {POSE_ANGLE_MAX}°")
    if issues:
        _write_side_by_side(actual, golden, out_dir / f"trace_diff_{name}.png")
    return {
        "camera": name,
        "ok": not issues,
        "psnr": round(p, 2),
        "ssim": round(s, 4),
        "pose_delta": round(pos_delta, 3),
        "yaw_delta": round(yaw_delta, 3),
        "pitch_delta": round(pitch_delta, 3),
        "issues": issues,
    }


def _write_side_by_side(actual: np.ndarray, golden: np.ndarray, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    height = min(actual.shape[0], golden.shape[0])
    width = min(actual.shape[1], golden.shape[1])
    gap = np.full((height, 8, 3), 255, dtype=np.uint8)
    combined = np.hstack([actual[:height, :width], gap, golden[:height, :width]])
    Image.fromarray(combined).save(path)


def _contract_issues(trace: dict) -> list[str]:
    issues: list[str] = []
    if int(trace["tri_count"]) > TRI_MAX:
        issues.append(f"三角面 {trace['tri_count']} > {TRI_MAX}")
    if int(trace["materials"]) > MATERIAL_MAX:
        issues.append(f"材质 {trace['materials']} > {MATERIAL_MAX}")
    return issues


def _update_golden(actual_dir: Path, golden_dir: Path, trace: dict) -> None:
    golden_dir.mkdir(parents=True, exist_ok=True)
    for view in trace["views"]:
        shutil.copy2(actual_dir / view["file"], golden_dir / view["file"])
    shutil.copy2(actual_dir / TRACE_NAME, golden_dir / TRACE_NAME)


def _compare_views(actual: dict, golden: dict, dirs: tuple[Path, Path, Path]) -> tuple[list, list]:
    actual_dir, golden_dir, out_dir = dirs
    issues: list[str] = []
    if actual["boxes_sha256"] != golden["boxes_sha256"]:
        issues.append("boxes_sha256 漂移（几何数据变了，需重建 golden）")
    base_views = {str(item["camera"]): item for item in golden["views"]}
    views = []
    for view in actual["views"]:
        name = str(view["camera"])
        if name not in base_views:
            issues.append(f"golden 缺少机位 {name}")
            continue
        views.append(compare_view(view, base_views[name], actual_dir, golden_dir, out_dir))
    return views, issues


def _report(map_id: str, trace: dict, views: list, issues: list, out_dir: Path) -> bool:
    failures = list(issues)
    failures += [f"{item['camera']}: {'; '.join(item['issues'])}"
                 for item in views if not item["ok"]]
    payload = {
        "map": map_id,
        "ok": not failures,
        "tri_count": trace["tri_count"],
        "materials": trace["materials"],
        "boxes_sha256": trace["boxes_sha256"],
        "views": views,
        "failures": failures,
    }
    (out_dir / f"{map_id}_report.json").write_text(
        json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
    for item in views:
        print(f"[trace] {item['camera']} {'PASS' if item['ok'] else 'FAIL'} "
              f"psnr={item.get('psnr')} ssim={item.get('ssim')} pose={item.get('pose_delta')}")
    for item in failures:
        print("FAIL", item)
    if failures:
        print(f"TRACE_CONTRACT FAIL map={map_id}")
        return False
    print(f"TRACE_CONTRACT PASS map={map_id} views={len(views)} "
          f"tris={trace['tri_count']} materials={trace['materials']}")
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description="Trace 契约与视觉双检测")
    parser.add_argument("--map", default="de_dust2")
    parser.add_argument("--actual", default="")
    parser.add_argument("--golden", default="")
    parser.add_argument("--update", action="store_true", help="生成候选 golden（人工门）")
    parser.add_argument("--out", default="log/verify/trace_contract")
    args = parser.parse_args()
    actual_dir = ROOT / (args.actual or f"verification/actual/{args.map}")
    golden_dir = ROOT / (args.golden or f"verification/goldens/{args.map}")
    out_dir = ROOT / args.out
    out_dir.mkdir(parents=True, exist_ok=True)
    actual = load_trace(actual_dir / TRACE_NAME)
    if args.update:
        _update_golden(actual_dir, golden_dir, actual)
        print(f"TRACE_UPDATE map={args.map} views={len(actual['views'])} -> {golden_dir}")
    golden_path = golden_dir / TRACE_NAME
    if not golden_path.exists():
        print(f"FAIL 缺少 {golden_path}（先人工审核后 --update）")
        return 1
    golden = load_trace(golden_path)
    views, issues = _compare_views(actual, golden, (actual_dir, golden_dir, out_dir))
    issues = _contract_issues(actual) + issues
    return 0 if _report(args.map, actual, views, issues, out_dir) else 1


if __name__ == "__main__":
    sys.exit(main())
