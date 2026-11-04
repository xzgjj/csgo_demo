"""de_ash 场景与几何累加：集合、倒角、主盒对象、合批盒体构造器。"""

from __future__ import annotations

import math

import bmesh
import bpy
from mathutils import Matrix, Vector


def reset_scene():
    """清空场景（后台进程从空场景开始，保证结果可复现）。"""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.name = "de_ash"
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1.0
    return scene


def make_collections():
    """建 de_ash 根集合 + Blockout / Detail / Lighting / Cameras 子集合。"""
    root = bpy.data.collections.new("de_ash")
    bpy.context.scene.collection.children.link(root)
    collections = {}
    for key, label in (("blockout", "Blockout"), ("detail", "Detail"),
                       ("lighting", "Lighting"), ("cameras", "Cameras")):
        collection = bpy.data.collections.new(label)
        root.children.link(collection)
        collections[key] = collection
    return collections


def add_bevel(obj, width, segments):
    """给盒体加倒角：让边缘在侧光下有高光线，避免纯方块观感。"""
    modifier = obj.modifiers.new("Bevel", "BEVEL")
    modifier.width = width
    modifier.segments = segments
    modifier.limit_method = "ANGLE"
    modifier.angle_limit = math.radians(30.0)
    modifier.use_clamp_overlap = True
    return modifier


def rot_axes(axis_x, axis_y, axis_z):
    """由 3 个正交单位向量（局部 x/y/z 轴）构造 3x3 旋转矩阵。"""
    return Matrix((
        (axis_x[0], axis_y[0], axis_z[0]),
        (axis_x[1], axis_y[1], axis_z[1]),
        (axis_x[2], axis_y[2], axis_z[2]),
    ))


class GeoBuilder:
    """把若干盒体累加进同一个 mesh（逐面记录材质索引），最后生成单个 object。"""

    def __init__(self):
        self.bm = bmesh.new()
        self.materials = []
        self._slots = {}

    def _slot_index(self, material):
        if material.name not in self._slots:
            self._slots[material.name] = len(self.materials)
            self.materials.append(material)
        return self._slots[material.name]

    def box(self, center, size, material, rotation=None):
        """加一个盒体：center/size 为 Blender 坐标；rotation 为可选 3x3 旋转。"""
        before = len(self.bm.faces)
        matrix = Matrix.Translation(Vector(center))
        if rotation is not None:
            matrix = matrix @ rotation.to_4x4()
        matrix = matrix @ Matrix.Diagonal(Vector((size[0], size[1], size[2], 1.0)))
        bmesh.ops.create_cube(self.bm, size=1.0, matrix=matrix)
        self.bm.faces.ensure_lookup_table()
        index = self._slot_index(material)
        for face_index in range(before, len(self.bm.faces)):
            self.bm.faces[face_index].material_index = index
        return self

    def finish(self, name, collection, bevel_width=0.8, bevel_segments=1):
        mesh = bpy.data.meshes.new(name)
        self.bm.to_mesh(mesh)
        self.bm.free()
        for material in self.materials:
            mesh.materials.append(material)
        obj = bpy.data.objects.new(name, mesh)
        collection.objects.link(obj)
        if bevel_width > 0.0:
            add_bevel(obj, bevel_width, bevel_segments)
        return obj


def make_block_object(box, materials, collection):
    """主盒：与 boxes JSON 一一对应、原点在盒心、尺寸精确（导出 GLB 供复检）。"""
    mesh = bpy.data.meshes.new(box["name"])
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=Vector(box["size"]), verts=bm.verts)
    bm.to_mesh(mesh)
    bm.free()
    mesh.materials.append(materials[box["material"]])
    obj = bpy.data.objects.new(box["name"], mesh)
    obj.location = Vector(box["center"])
    obj["kind"] = box["kind"]
    obj["material_id"] = box["material"]
    obj["collision"] = box["collision"]
    obj["box_index"] = box["index"]
    collection.objects.link(obj)
    add_bevel(obj, 1.2, 2)
    return obj
