"""武器构建流水线：建模 → 导出 GLB → 存 .blend 版本 → 预览渲染 / GLB 复检。"""

from __future__ import annotations

import bpy
from mathutils import Vector

from common import export_glb, next_version_path, parse_glb, prune_versions

from .materials import make_materials
from .meshkit import WeaponBuilder
from .parts_ak import build_ak47
from .parts_awp import build_awp
from .parts_m4 import build_m4a4
from .paths import DOC_DIR, GLB_DIR, VERSION_DIR
from .render import place_rig, render_all_views, render_view, setup_render

WEAPON_IDS = ("ak47", "m4a4", "awp")
WEAPON_LABELS = {"ak47": "AK-47", "m4a4": "M4A4", "awp": "AWP"}

BUILDERS = {"ak47": build_ak47, "m4a4": build_m4a4, "awp": build_awp}
GLB_VIEWS = ("side", "fp")


def reset_scene() -> bpy.types.Scene:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1.0
    return scene


def tri_count(obj: bpy.types.Object) -> int:
    return sum(len(poly.vertices) - 2 for poly in obj.data.polygons)


def world_bbox(objects: list[bpy.types.Object]) -> tuple[Vector, Vector]:
    low = Vector((1e9, 1e9, 1e9))
    high = Vector((-1e9, -1e9, -1e9))
    for obj in objects:
        for corner in obj.bound_box:
            point = obj.matrix_world @ Vector(corner)
            low = Vector((min(low.x, point.x), min(low.y, point.y), min(low.z, point.z)))
            high = Vector((max(high.x, point.x), max(high.y, point.y), max(high.z, point.z)))
    return low, high


# --------------------------------------------------------------------------- #
# 建模
# --------------------------------------------------------------------------- #
def _warn_part_limits(wid: str, builder: WeaponBuilder, obj: bpy.types.Object,
                      dims: Vector) -> None:
    """部件数 / 面数 / 长度越界时打 WARN（不中断构建，便于回归对比）。"""
    if not (40 <= builder.part_count <= 90):
        print(f"WARN {wid} 部件数越界: {builder.part_count}")
    if tri_count(obj) >= 8000:
        print(f"WARN {wid} 面数越界: {tri_count(obj)}")
    if not (0.8 <= dims.y <= 1.2):
        print(f"WARN {wid} 长度越界: {dims.y:.3f}")


def _build_weapon(wid: str, mats: dict[str, bpy.types.Material]
                  ) -> tuple[bpy.types.Object, int, Vector]:
    builder = WeaponBuilder(wid)
    BUILDERS[wid](builder)
    obj = builder.finish(mats)
    low, high = world_bbox([obj])
    dims = high - low
    print(f"BUILD {wid} label={WEAPON_LABELS[wid]} parts={builder.part_count} "
          f"tris={tri_count(obj)} dims=({dims.x:.3f},{dims.y:.3f},{dims.z:.3f}) "
          f"aabb_lo=({low.x:.3f},{low.y:.3f},{low.z:.3f}) "
          f"aabb_hi=({high.x:.3f},{high.y:.3f},{high.z:.3f})")
    _warn_part_limits(wid, builder, obj, dims)
    return obj, builder.part_count, dims


def _build_weapons(mats: dict[str, bpy.types.Material]
                   ) -> tuple[dict[str, bpy.types.Object], list[str]]:
    objects: dict[str, bpy.types.Object] = {}
    summary: list[str] = []
    for wid in WEAPON_IDS:
        obj, parts, dims = _build_weapon(wid, mats)
        objects[wid] = obj
        summary.append(f"{wid}: parts={parts} tris={tri_count(obj)} len={dims.y:.3f}m")
    return objects, summary


# --------------------------------------------------------------------------- #
# 导出 GLB（先导出再渲染，保证 GLB 里没有灯光/相机）
# --------------------------------------------------------------------------- #
def _print_glb_meta(wid: str, glb_path, meta: dict[str, object]) -> None:
    """打印 GLB 摘要并做轴向自检（glTF 坐标：Z=枪长、Y=高、X=宽，枪口 -Z）。"""
    low = meta["low"]
    high = meta["high"]
    print(f"GLB {wid} file={glb_path.name} bytes={meta['bytes']} "
          f"nodes={meta['nodes']} scenes={meta['scenes']} "
          f"materials={meta['materials']} tris={meta['triangles']} "
          f"lo=({low[0]:.3f},{low[1]:.3f},{low[2]:.3f}) "
          f"hi=({high[0]:.3f},{high[1]:.3f},{high[2]:.3f})")
    dims_gltf = [high[axis] - low[axis] for axis in range(3)]
    long_ok = dims_gltf[2] > dims_gltf[1] > dims_gltf[0]
    muzzle_ok = low[2] < -0.30 and abs(low[2]) > abs(high[2])
    up_ok = high[1] > 0.0 and low[1] < 0.0
    if not (long_ok and muzzle_ok and up_ok):
        print(f"WARN {wid} 轴向可疑 long_ok={long_ok} muzzle_ok={muzzle_ok} "
              f"up_ok={up_ok} dims_gltf={[round(v, 3) for v in dims_gltf]}")


def _export_weapon(wid: str, obj: bpy.types.Object) -> None:
    glb_path = GLB_DIR / f"{wid}.glb"
    export_glb(glb_path, [obj], scene_name=wid)
    _print_glb_meta(wid, glb_path, parse_glb(glb_path))


def _save_version() -> None:
    bpy.context.scene.name = "weapon_source"
    version_path = next_version_path(VERSION_DIR, "weapons")
    bpy.ops.wm.save_as_mainfile(filepath=str(version_path))
    removed = prune_versions(VERSION_DIR, "weapons", 3)
    print(f"VERSION saved={version_path.name} removed={removed}")


# --------------------------------------------------------------------------- #
# 预览渲染（逐把枪摆机位；渲染时隐藏另外两把，避免重叠）
# --------------------------------------------------------------------------- #
def _render_previews(scene: bpy.types.Scene, rig: dict[str, bpy.types.Object],
                     objects: dict[str, bpy.types.Object]) -> None:
    for wid in WEAPON_IDS:
        low, high = world_bbox([objects[wid]])
        place_rig(rig, (low + high) / 2.0, high - low)
        outputs = render_all_views(scene, rig, wid, objects)
        print(f"RENDER {wid} -> {[p.name for p in outputs]}")
    for obj in objects.values():
        obj.hide_render = False


def build_all(render: bool) -> int:
    scene = reset_scene()
    mats = make_materials()
    objects, summary = _build_weapons(mats)
    rig = setup_render(scene)
    for wid in WEAPON_IDS:
        _export_weapon(wid, objects[wid])
    _save_version()
    if render:
        _render_previews(scene, rig, objects)
    print("BUILD_OK " + " | ".join(summary))
    return 0


# --------------------------------------------------------------------------- #
# GLB 复检：重新导入导出的 GLB，核对网格/材质/尺寸并重渲染
# --------------------------------------------------------------------------- #
def _import_meshes(glb_path, before: set) -> list[bpy.types.Object]:
    bpy.ops.import_scene.gltf(filepath=str(glb_path))
    return [obj for obj in bpy.data.objects if obj not in before and obj.type == "MESH"]


def _imported_root_names(before: set) -> list[str]:
    return [obj.name for obj in bpy.data.objects
            if obj not in before and obj.parent is None]


def _imported_material_names(imported: list[bpy.types.Object]) -> list[str]:
    return sorted({slot.material.name.split(".")[0] for obj in imported
                   for slot in obj.material_slots if slot.material})


def _check_import(wid: str, dims: Vector, tris: int, material_names: list[str],
                  failures: list[str]) -> None:
    if dims.y < 0.8 or dims.y > 1.2:
        failures.append(f"{wid} 长度异常 {dims.y:.3f}")
    if tris >= 8000:
        failures.append(f"{wid} 面数异常 {tris}")
    if not material_names:
        failures.append(f"{wid} 材质缺失")


def _render_recheck(scene: bpy.types.Scene, rig: dict[str, bpy.types.Object],
                    wid: str, imported: list[bpy.types.Object]) -> None:
    low, high = world_bbox(imported)
    place_rig(rig, (low + high) / 2.0, high - low)
    for other in bpy.data.objects:
        if other.type == "MESH":
            other.hide_render = other not in imported
    for view in GLB_VIEWS:
        path = DOC_DIR / f"{wid}_glb_{view}.png"
        render_view(scene, rig["cameras"][view], path)
        print(f"RECHECK_RENDER {wid} {view} -> {path.name}")


def _recheck_weapon(wid: str, scene: bpy.types.Scene,
                    rig: dict[str, bpy.types.Object], failures: list[str]) -> None:
    glb_path = GLB_DIR / f"{wid}.glb"
    if not glb_path.is_file():
        failures.append(f"缺少 {glb_path}")
        return
    before = set(bpy.data.objects)
    imported = _import_meshes(glb_path, before)
    if not imported:
        failures.append(f"{wid} 导入无网格")
        return
    low, high = world_bbox(imported)
    dims = high - low
    tris = sum(tri_count(obj) for obj in imported)
    material_names = _imported_material_names(imported)
    print(f"RECHECK {wid} meshes={len(imported)} tris={tris} "
          f"dims=({dims.x:.3f},{dims.y:.3f},{dims.z:.3f}) "
          f"root={_imported_root_names(before)} mats={material_names}")
    _check_import(wid, dims, tris, material_names, failures)
    _render_recheck(scene, rig, wid, imported)


def recheck_glb() -> int:
    """导入已导出的 GLB，渲染复检图并核对网格/材质/尺寸，确认导出无损。"""
    scene = reset_scene()
    rig = setup_render(scene)
    failures: list[str] = []
    for wid in WEAPON_IDS:
        _recheck_weapon(wid, scene, rig, failures)
    if failures:
        for failure in failures:
            print(f"RECHECK_FAIL {failure}")
        print("RECHECK_STATUS=FAIL")
        return 1
    print("RECHECK_STATUS=PASS")
    return 0
