"""de_dust2 GLB 构建：boxes JSON → 每盒一节点（extras 带 box_index/kind/material/collision）。

用法（headless，CI 可复现）：
  blender --background --python tools/blender/build_de_dust2.py
用法（常驻 GUI，经 dsh-blender-plugin MCP 桥）：
  blender_py {"code": "exec(open('tools/blender/build_de_dust2.py').read())", "see": true}
GUI 模式不重置场景，只清理并重建名为 dust2_build 的集合，不碰用户当前工程。
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

import bpy

PROJECT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))

from common import export_glb, parse_glb  # noqa: E402

BOXES_PATH = PROJECT / "host/scenes/de_dust2_boxes.json"
GLB_PATH = PROJECT / "host/models/map/de_dust2.glb"
SOURCE_COPY = PROJECT / "content/maps/de_dust2.glb"
COLLECTION = "dust2_build"

COLORS = {
    "concrete": (0.58, 0.48, 0.36, 1.0),
    "stone": (0.60, 0.53, 0.42, 1.0),
    "metal": (0.52, 0.54, 0.58, 1.0),
    "wood": (0.42, 0.29, 0.17, 1.0),
    "glass": (0.70, 0.82, 0.88, 0.34),
    "fabric": (0.45, 0.40, 0.32, 1.0),
    "dirt": (0.42, 0.34, 0.24, 1.0),
    "road": (0.50, 0.44, 0.34, 1.0),
    "site": (0.62, 0.22, 0.18, 1.0),
    "spawn": (0.20, 0.38, 0.62, 1.0),
}

def material_for(key: str) -> bpy.types.Material:
    name = f"mat_{key}"
    material = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    material.diffuse_color = COLORS.get(key, (0.5, 0.5, 0.5, 1.0))
    material.use_nodes = True
    # Blender 5.x 的节点名可能是本地化/重命名过的，按类型找 Principled，别按名字找。
    bsdf = next((node for node in material.node_tree.nodes
                 if node.type == "BSDF_PRINCIPLED"), None)
    if bsdf is not None:
        bsdf.inputs["Base Color"].default_value = material.diffuse_color
        bsdf.inputs["Roughness"].default_value = 0.85
        bsdf.inputs["Metallic"].default_value = 0.8 if key == "metal" else 0.0
        if key == "glass":
            bsdf.inputs["Alpha"].default_value = 0.34
            material.blend_method = "BLEND"
    return material


def reset_collection() -> bpy.types.Collection:
    existing = bpy.data.collections.get(COLLECTION)
    if existing is not None:
        for obj in list(existing.objects):
            bpy.data.objects.remove(obj, do_unlink=True)
        bpy.data.collections.remove(existing)
    collection = bpy.data.collections.new(COLLECTION)
    bpy.context.scene.collection.children.link(collection)
    return collection


def build() -> int:
    data = json.loads(BOXES_PATH.read_text(encoding="utf-8"))
    boxes = data["boxes"]
    collection = reset_collection()
    materials = {key: material_for(key) for key in COLORS}
    for index, box in enumerate(boxes):
        center = box["center"]  # Godot (x, y_up, z)
        size = box["size"]
        # Godot → Blender： (x, y, z)_G → (x, -z, y)_B
        location = (float(center[0]), -float(center[2]), float(center[1]))
        scale = (float(size[0]), float(size[2]), float(size[1]))
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=location)
        obj = bpy.context.object
        obj.name = f"box_{index:03d}_{box['kind']}"
        obj.scale = scale
        obj.data.materials.append(materials[box["material"]])
        obj["box_index"] = index
        obj["kind"] = box["kind"]
        obj["material_id"] = box["material"]
        obj["collision"] = bool(box["collision"])
        for old in list(obj.users_collection):
            old.objects.unlink(obj)
        collection.objects.link(obj)
    objects = [obj for obj in collection.objects if obj.type == "MESH"]
    exported = export_glb(GLB_PATH, objects)
    SOURCE_COPY.parent.mkdir(parents=True, exist_ok=True)
    SOURCE_COPY.write_bytes(GLB_PATH.read_bytes())
    info = parse_glb(GLB_PATH)
    print(f"DUST2_GLB exported={exported} bytes={info['bytes']} "
          f"triangles={info['triangles']} nodes={len(info['nodes'])} materials={info['materials']}")
    print(f"DUST2_GLB_SHA256 {info['sha256']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(build())
