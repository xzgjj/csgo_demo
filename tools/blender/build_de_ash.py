"""de_ash 地图 Blender 构建（确定性，后台独立进程；本文件只做编排与 CLI）。

用法：
    blender --background --factory-startup --python tools/blender/build_de_ash.py -- [选项]

选项：
    --no-render     只建模 / 存版本 / 导 GLB，不渲染
    --preview       快速预览（1280x720 / 16 采样），输出到
                    docs/design/de_ash_renders/preview/，不存版本、不导 GLB
    --verify-glb    复检模式：新进程导入 content/maps/de_ash.glb，校验对象数 / 包围盒 /
                    材质与 host/scenes/de_ash_boxes.json 一致（±1u）

坐标：Godot (x, y_up, z) → Blender (x, -z, y)（Blender Z 轴向上，地面 z=0）；
      盒子的 center/size 直接读 host/scenes/de_ash_boxes.json（唯一几何事实源）。
      装饰件只做视觉贴合，不写入任何碰撞数据。

产物：
    assets/source/de_ash/versions/de_ash_vNNN.blend（只保留最近 3 个）
    docs/design/de_ash_renders/de_ash_<view>.png（6 机位，1280x720，EEVEE）
    content/maps/de_ash.glb（GLB，嵌入材质）

实现拆分在 tools/blender/de_ash/ 包（data / materials / geometry / details_* /
lighting / render / export），GLB 导出与版本管理复用 tools/blender/common.py。
"""

from __future__ import annotations

import sys
import time
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import bpy  # noqa: E402

from common import parse_glb  # noqa: E402
from de_ash import data, details_access, details_openings  # noqa: E402
from de_ash import details_props, details_structures, geometry, lighting  # noqa: E402
from de_ash import materials, render  # noqa: E402
from de_ash import export as map_export  # noqa: E402

# 装饰件构建顺序（每类只做视觉贴合，不动任何碰撞盒）
DETAIL_BUILDERS = (
    details_structures.build_coping,
    details_structures.build_wall_details,
    details_structures.build_roofs,
    details_structures.build_door_frames,
    details_structures.build_arches,
    details_openings.build_windows,
    details_openings.build_awnings,
    details_openings.build_crates,
    details_access.build_stair_rails,
    details_access.build_catwalk_details,
    details_props.build_lamps,
    details_props.build_signs,
    details_props.build_ground_skirt,
)


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    options = {"render": True, "preview": False, "verify": False}
    for argument in argv:
        if argument == "--no-render":
            options["render"] = False
        elif argument == "--preview":
            options["preview"] = True
        elif argument == "--verify-glb":
            options["verify"] = True
        else:
            raise SystemExit(f"未知参数: {argument}")
    return options


def build_map(scene):
    """建 108 个主盒 + 全部分类装饰件，返回 (boxes, materials, collections)。"""
    boxes = data.load_boxes()
    mats = materials.make_materials()
    collections = geometry.make_collections()
    for box in boxes:
        geometry.make_block_object(box, mats, collections["blockout"])
    for builder in DETAIL_BUILDERS:
        builder(boxes, mats, collections["detail"])
    return boxes, mats, collections


def setup_scene(scene, options, boxes):
    """灯光 + 6 机位 + 渲染参数 + 场景元数据；返回相机表。"""
    lighting.setup_lighting(scene, bpy.data.collections["Lighting"])
    cameras = lighting.build_cameras(bpy.data.collections["Cameras"])
    scene.camera = cameras["overview_top"]
    scene["source"] = "host/scenes/de_ash_boxes.json"
    scene["generator"] = "tools/blender/build_de_ash.py"
    scene["schema"] = "de-ash-blender/1"
    scene["box_count"] = len(boxes)
    if options["preview"]:
        render.setup_render(scene, data.PREVIEW_W, data.PREVIEW_H,
                            data.PREVIEW_SAMPLES)
    else:
        render.setup_render(scene, data.RENDER_W, data.RENDER_H,
                            data.RENDER_SAMPLES)
    return cameras


def summarize():
    """打印并返回 Blockout / Detail 网格统计。"""
    blockout = bpy.data.collections["Blockout"]
    detail = bpy.data.collections["Detail"]
    block_objects = [obj for obj in blockout.objects if obj.type == "MESH"]
    detail_objects = [obj for obj in detail.objects if obj.type == "MESH"]
    triangles = 0
    for obj in block_objects + detail_objects:
        triangles += sum(len(poly.vertices) - 2 for poly in obj.data.polygons)
    print(f"STATS blockout={len(block_objects)} detail={len(detail_objects)} "
          f"total_mesh={len(block_objects) + len(detail_objects)} "
          f"triangles={triangles} materials={len(bpy.data.materials)}")
    return block_objects, detail_objects


def export_outputs(collections):
    """导出 GLB 并打印摘要（含 sha256，便于确认确定性）。"""
    exported = map_export.export_glb(collections["blockout"], collections["detail"])
    info = parse_glb(data.GLB_PATH)
    print(f"GLB {data.GLB_PATH.relative_to(data.PROJECT_ROOT)} exported={exported} "
          f"bytes={info['bytes']} triangles={info['triangles']} "
          f"nodes={len(info['nodes'])} materials={info['materials']}")
    print(f"GLB_SHA256 {info['sha256']}")
    print(f"VERSIONS removed={map_export.prune_versions(3)}")


def build(options):
    """完整构建：建模 → 灯光相机 → 存版本 → 渲染 → 导 GLB → 版本裁剪。"""
    started = time.time()
    scene = geometry.reset_scene()
    boxes, _mats, collections = build_map(scene)
    cameras = setup_scene(scene, options, boxes)
    version_path = None
    if not options["preview"]:
        version_path = map_export.next_version_path()
        bpy.ops.wm.save_as_mainfile(filepath=str(version_path), compress=True)
        print(f"VERSION saved={version_path.relative_to(data.PROJECT_ROOT)}")
    block_objects, detail_objects = summarize()
    render_paths = []
    if options["render"]:
        output_dir = (data.RENDER_DIR / "preview" if options["preview"]
                      else data.RENDER_DIR)
        render_paths = render.render_views(scene, cameras, output_dir)
    if not options["preview"]:
        export_outputs(collections)
    elapsed = time.time() - started
    version_name = version_path.name if version_path else "preview"
    print(f"BLENDER_BUILD_OK objects={len(block_objects) + len(detail_objects)} "
          f"renders={len(render_paths)} version={version_name} "
          f"elapsed={elapsed:.1f}s")
    return 0


def main():
    options = parse_args()
    if options["verify"]:
        return map_export.verify_glb()
    return build(options)


if __name__ == "__main__":
    sys.exit(main())
