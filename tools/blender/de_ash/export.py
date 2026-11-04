"""de_ash 导出与复检：blend 版本保留、GLB 导出、导入回查（包围盒/对象数/材质）。"""

from __future__ import annotations

import bpy
from mathutils import Vector

from common import export_glb as shared_export_glb
from common import next_version_path as shared_next_version_path
from common import parse_glb
from common import prune_versions as shared_prune_versions

from .data import GLB_PATH, MATERIAL_SPECS, VERSION_DIR, box_bounds, load_boxes

VERSION_PREFIX = "de_ash"


def next_version_path():
    return shared_next_version_path(VERSION_DIR, VERSION_PREFIX)


def prune_versions(keep=3):
    return shared_prune_versions(VERSION_DIR, VERSION_PREFIX, keep)


def export_glb(blockout, detail):
    """把 Blockout + Detail 两个集合的网格导出成 content/maps/de_ash.glb。"""
    objects = [obj for collection in (blockout, detail)
               for obj in collection.objects if obj.type == "MESH"]
    return shared_export_glb(GLB_PATH, objects)


def object_bounds(obj):
    """对象世界空间包围盒 (x0, y0, z0, x1, y1, z1)。"""
    corners = [obj.matrix_world @ Vector(corner) for corner in obj.bound_box]
    return (min(point.x for point in corners), min(point.y for point in corners),
            min(point.z for point in corners), max(point.x for point in corners),
            max(point.y for point in corners), max(point.z for point in corners))


def bbox_deviation(boxes, by_name):
    """逐盒比对导入对象包围盒与 JSON 期望值，返回 (最大偏差, 最差盒, 缺失列表)。"""
    worst_delta = 0.0
    worst_box = ""
    missing = []
    for box in boxes:
        obj = by_name.get(box["name"])
        if obj is None:
            missing.append(box["name"])
            continue
        actual = object_bounds(obj)
        delta = max(abs(a - b) for a, b in zip(actual, box_bounds(box)))
        if delta > worst_delta:
            worst_delta = delta
            worst_box = box["name"]
    return worst_delta, worst_box, missing


def verify_glb():
    """复检模式：新进程导入 GLB，校验对象数/包围盒/材质与 boxes JSON 一致（±1u）。"""
    if not GLB_PATH.exists():
        print(f"VERIFY_FAIL 缺少 GLB: {GLB_PATH}")
        return 1
    info = parse_glb(GLB_PATH)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=str(GLB_PATH))
    boxes = load_boxes()
    mesh_objects = [obj for obj in bpy.data.objects if obj.type == "MESH"]
    by_name = {obj.name: obj for obj in mesh_objects}
    worst_delta, worst_box, missing = bbox_deviation(boxes, by_name)
    decor = [obj for obj in mesh_objects if not obj.name.startswith("box_")]
    material_names = sorted(material.name for material in bpy.data.materials)
    print(f"VERIFY_GLB nodes={len(info['nodes'])} meshes={info['meshes']} "
          f"triangles={info['triangles']} bytes={info['bytes']}")
    print(f"VERIFY_GLB_MATERIALS {info['materials']}")
    print(f"VERIFY_IMPORT mesh_objects={len(mesh_objects)} "
          f"expected_boxes={len(boxes)} decor_objects={len(decor)} "
          f"blend_materials={material_names}")
    print(f"VERIFY_BBOX worst_delta={worst_delta:.3f}u box={worst_box} "
          f"missing={len(missing)}")
    if missing:
        print("VERIFY_MISSING " + " ".join(missing[:12]))
    expected_materials = {f"mat_{key}" for key in MATERIAL_SPECS}
    missing_materials = sorted(expected_materials - set(material_names))
    ok = (not missing and worst_delta <= 1.0 and not missing_materials
          and len(mesh_objects) >= len(boxes) + 80)
    print(f"VERIFY_MATERIALS_MISSING {missing_materials}")
    print("VERIFY_OK" if ok else "VERIFY_FAIL")
    return 0 if ok else 1
