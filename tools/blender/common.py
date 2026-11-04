"""tools/blender 下所有 Blender 后台脚本的共享工具（GLB 导出 / 解析 / 版本保留）。

被 build_de_ash.py（地图）与 build_weapons.py（武器）共用，避免两边各复制一份
export_scene.gltf 参数块与版本管理代码（CI duplicates 阈值 8 行）。
"""

from __future__ import annotations

import hashlib
import json
import struct
from collections.abc import Iterable
from pathlib import Path

import bpy

## 两个构建脚本共用的导出选项；不支持的键在运行时按 Blender 版本过滤。
GLTF_EXPORT_OPTIONS: dict[str, object] = {
    "export_format": "GLB",
    "use_selection": True,
    "export_apply": True,
    "export_yup": True,
    "export_animations": False,
    "export_cameras": False,
    "export_lights": False,
    "export_materials": "EXPORT",
    "export_extras": True,
    "export_texcoords": False,
    "export_normals": True,
}


def gltf_export_kwargs(**kwargs: object) -> dict[str, object]:
    """合并导出选项，只保留当前 Blender 版本 export_scene.gltf 支持的参数。"""
    options = dict(GLTF_EXPORT_OPTIONS)
    options.update(kwargs)
    try:
        supported = bpy.ops.export_scene.gltf.get_rna_type().properties.keys()
    except AttributeError:
        return options
    return {key: value for key, value in options.items() if key in supported}


def export_glb(path: str | Path, objects: Iterable[bpy.types.Object],
               scene_name: str | None = None,
               extra_options: dict[str, object] | None = None) -> int:
    """把 objects 中的 MESH 导出成单个 GLB；返回导出的网格数。

    use_selection=True，相机 / 灯光不会被带进 GLB。scene_name 写到 glTF 场景名，
    Godot 用 GLTFDocument.generate_scene 生成场景时根节点默认取这个名字。
    """
    target = Path(path)
    target.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    exported = 0
    active = None
    for obj in objects:
        if obj.type != "MESH":
            continue
        obj.select_set(True)
        exported += 1
        if active is None:
            active = obj
    if active is None:
        raise ValueError("export_glb: 没有可导出的 MESH 对象")
    bpy.context.view_layer.objects.active = active
    if scene_name:
        bpy.context.scene.name = scene_name
    bpy.ops.export_scene.gltf(filepath=str(target),
                              **gltf_export_kwargs(**(extra_options or {})))
    return exported


def _primitive_triangles(document: dict, primitive: dict) -> int:
    """单个 primitive 的三角形数：有索引用索引数，否则用顶点数。"""
    if "indices" in primitive:
        return document["accessors"][primitive["indices"]]["count"] // 3
    accessor = document["accessors"][primitive["attributes"]["POSITION"]]
    return accessor["count"] // 3


def _merge_position_bounds(document: dict, primitive: dict, low: list[float],
                           high: list[float]) -> None:
    """把 primitive 的 POSITION accessor min/max 合并进包围盒（原地更新）。"""
    accessor = document["accessors"][primitive["attributes"]["POSITION"]]
    if "min" not in accessor or "max" not in accessor:
        return
    for axis in range(3):
        low[axis] = min(low[axis], accessor["min"][axis])
        high[axis] = max(high[axis], accessor["max"][axis])


def _accumulate_geometry(document: dict) -> tuple[list[float], list[float], int]:
    """遍历 meshes/primitives，累加 POSITION 包围盒与三角形数。"""
    low = [1e9, 1e9, 1e9]
    high = [-1e9, -1e9, -1e9]
    triangles = 0
    for mesh in document.get("meshes", []):
        for primitive in mesh.get("primitives", []):
            _merge_position_bounds(document, primitive, low, high)
            triangles += _primitive_triangles(document, primitive)
    return low, high, triangles


def parse_glb(path: str | Path) -> dict[str, object]:
    """解析 GLB 的 JSON 块，返回节点/材质/三角形数 + POSITION 包围盒（glTF 坐标）。"""
    blob = Path(path).read_bytes()
    if len(blob) < 20:
        raise ValueError(f"GLB 太小: {path}")
    magic, version, total = struct.unpack_from("<III", blob, 0)
    if magic != 0x46546C67:
        raise ValueError(f"GLB magic 非法: {path}")
    chunk_length, chunk_type = struct.unpack_from("<II", blob, 12)
    if chunk_type != 0x4E4F534A:
        raise ValueError(f"GLB 首块不是 JSON: {path}")
    document = json.loads(blob[20:20 + chunk_length].decode("utf-8"))
    low, high, triangles = _accumulate_geometry(document)
    return {
        "bytes": len(blob),
        "total": total,
        "version": version,
        "nodes": [node.get("name") for node in document.get("nodes", [])],
        "meshes": len(document.get("meshes", [])),
        "materials": [material.get("name") for material in document.get("materials", [])],
        "scenes": [scene.get("name") for scene in document.get("scenes", [])],
        "triangles": triangles,
        "low": low,
        "high": high,
        "sha256": hashlib.sha256(blob).hexdigest(),
    }


def next_version_path(directory: str | Path, prefix: str) -> Path:
    """directory 下 <prefix>_vNNN.blend 的下一个编号路径（自动建目录）。"""
    base = Path(directory)
    base.mkdir(parents=True, exist_ok=True)
    existing = sorted(base.glob(f"{prefix}_v*.blend"))
    index = 1 + max((int(path.stem.split("_v")[-1]) for path in existing), default=0)
    return base / f"{prefix}_v{index:03d}.blend"


def prune_versions(directory: str | Path, prefix: str, keep: int = 3) -> list[str]:
    """只保留最近 keep 个 <prefix>_v*.blend，返回被删除的文件名列表。"""
    base = Path(directory)
    if not base.is_dir():
        return []
    removed: list[str] = []
    for old in sorted(base.glob(f"{prefix}_v*.blend"))[:-keep]:
        old.unlink()
        removed.append(old.name)
    return removed
