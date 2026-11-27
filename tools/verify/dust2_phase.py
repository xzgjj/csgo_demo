"""de_dust2 阶段检查（截图 → trace 双检测 → 探针 → 雷达 → 计时/对齐）。

从 run_all.py 拆出以控制单文件行数；依赖注入 run_cmd 保持 stdout 记录格式一致。
"""

from __future__ import annotations

from pathlib import Path

import run_scenarios


def capture(root: Path, run_cmd, timeout: int = 600) -> dict:
    godot = run_scenarios.find_godot()
    return run_cmd(
        [godot, "--path", "host", "--script", "res://tests/capture_map.gd", "--",
         "--map=de_dust2", "--out=" + str(root / "verification/actual/de_dust2")],
        timeout,
        "capture_dust2",
    )


def checks(root: Path, run_cmd, py: str) -> list:
    result = [capture(root, run_cmd)]
    result.append(run_cmd([py, "-X", "utf8", "tools/verify/trace_contract.py",
                           "--map", "de_dust2"], 300, "trace_contract"))
    # 地图资产自体检：遮挡/体量、跳高可达、盒↔GLB 尺寸。跟着截图一起跑，换图后不会静默漂移。
    result.append(run_cmd([py, "-X", "utf8", "tools/verify/visual_audit.py",
                           "--map", "de_dust2"], 240, "visual_audit"))
    result.append(run_cmd([py, "-X", "utf8", "tools/mapgen/check_reach.py",
                           "--map", "de_dust2"], 120, "check_reach"))
    result.append(run_cmd([py, "-X", "utf8", "tools/mapgen/check_glb_boxes.py",
                           "--map", "de_dust2"], 120, "glb_boxes"))
    godot = run_scenarios.find_godot()
    for script, label in (("dust2_probe.gd", "dust2_probe"),
                          ("radar_dust2_smoke.gd", "radar_dust2_smoke")):
        result.append(run_cmd([godot, "--headless", "--path", "host",
                               "--script", f"res://tests/{script}"], 300, label))
    for tool, label in (("lane_timing.py", "lane_timing"),
                        ("scene_alignment.py", "scene_alignment")):
        result.append(run_cmd([py, "-X", "utf8", f"tools/mapgen/{tool}"], 300, label))
    return result
