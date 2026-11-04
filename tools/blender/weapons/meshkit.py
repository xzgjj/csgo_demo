"""几何构件：bmesh 盒体 / 圆柱 / 锥台 / 斜切 + 单 mesh 累加 + 武器构建器。"""

from __future__ import annotations

import math

import bmesh
import bpy
from mathutils import Euler, Matrix, Vector

from .materials import MAT_KEYS


class MeshAccum:
    """把多个 bmesh 部件累加成一个 mesh，逐面记录材质索引。"""

    def __init__(self) -> None:
        self.verts: list[tuple[float, float, float]] = []
        self.faces: list[tuple[int, ...]] = []
        self.face_mats: list[int] = []
        self.part_names: list[str] = []

    def add(self, bm: bmesh.types.BMesh, mat_key: str, name: str) -> None:
        mat_index = MAT_KEYS.index(mat_key)
        bm.verts.ensure_lookup_table()
        offset = len(self.verts)
        index_of = {v: i + offset for i, v in enumerate(bm.verts)}
        self.verts.extend(tuple(v.co) for v in bm.verts)
        for face in bm.faces:
            self.faces.append(tuple(index_of[v] for v in face.verts))
            self.face_mats.append(mat_index)
        bm.free()
        self.part_names.append(name)

    def to_object(self, wid: str, mats: dict[str, bpy.types.Material]) -> bpy.types.Object:
        mesh = bpy.data.meshes.new(f"{wid}_mesh")
        mesh.from_pydata(self.verts, [], self.faces)
        mesh.validate()
        for key in MAT_KEYS:
            mesh.materials.append(mats[key])
        for index, polygon in enumerate(mesh.polygons):
            polygon.material_index = self.face_mats[index]
            polygon.use_smooth = False
        mesh.update()
        obj = bpy.data.objects.new(wid, mesh)
        bpy.context.scene.collection.objects.link(obj)
        return obj


def _place(bm: bmesh.types.BMesh, center: tuple[float, float, float],
           rot_deg: tuple[float, float, float]) -> None:
    if center != (0.0, 0.0, 0.0) or rot_deg != (0.0, 0.0, 0.0):
        rotation = Euler(tuple(math.radians(a) for a in rot_deg), "XYZ").to_matrix().to_4x4()
        bm.transform(Matrix.Translation(Vector(center)) @ rotation)


def _bevel(bm: bmesh.types.BMesh, offset: float) -> None:
    if offset <= 0.0:
        return
    bmesh.ops.bevel(bm, geom=list(bm.verts) + list(bm.edges), offset=offset,
                    offset_type="OFFSET", segments=1, profile=0.5, affect="EDGES",
                    clamp_overlap=True)


def _box_bm(size: tuple[float, float, float], bevel: float) -> bmesh.types.BMesh:
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for vert in bm.verts:
        vert.co = Vector((vert.co.x * size[0], vert.co.y * size[1], vert.co.z * size[2]))
    _bevel(bm, bevel)
    return bm


def _cone_bm(radius_neg: float, radius_pos: float, depth: float, segments: int,
             axis: str) -> bmesh.types.BMesh:
    """圆柱 / 锥台：radius_neg 位于该轴负端，radius_pos 位于正端。"""
    bm = bmesh.new()
    if axis == "Y":
        bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=segments,
                              radius1=radius_pos, radius2=radius_neg, depth=depth)
        bm.transform(Matrix.Rotation(math.pi / 2.0, 4, "X"))
    elif axis == "X":
        bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=segments,
                              radius1=radius_neg, radius2=radius_pos, depth=depth)
        bm.transform(Matrix.Rotation(math.pi / 2.0, 4, "Y"))
    elif axis == "Z":
        bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=segments,
                              radius1=radius_neg, radius2=radius_pos, depth=depth)
    else:
        raise ValueError(f"未知轴: {axis}")
    return bm


def _prism_bm(y0: float, y1: float, c0: tuple[float, float], h0: tuple[float, float],
              c1: tuple[float, float], h1: tuple[float, float],
              bevel: float) -> bmesh.types.BMesh:
    """沿 Y 的梯形棱柱：两端矩形截面（c=中心 xz，h=半宽/半高）。"""
    corners = [
        (c0[0] - h0[0], y0, c0[1] - h0[1]),
        (c0[0] + h0[0], y0, c0[1] - h0[1]),
        (c0[0] + h0[0], y0, c0[1] + h0[1]),
        (c0[0] - h0[0], y0, c0[1] + h0[1]),
        (c1[0] - h1[0], y1, c1[1] - h1[1]),
        (c1[0] + h1[0], y1, c1[1] - h1[1]),
        (c1[0] + h1[0], y1, c1[1] + h1[1]),
        (c1[0] - h1[0], y1, c1[1] + h1[1]),
    ]
    faces = [(0, 1, 2, 3), (7, 6, 5, 4), (0, 4, 5, 1), (1, 5, 6, 2),
             (2, 6, 7, 3), (3, 7, 4, 0)]
    bm = bmesh.new()
    verts = [bm.verts.new(co) for co in corners]
    bm.verts.index_update()
    for face in faces:
        bm.faces.new([verts[i] for i in face])
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    _bevel(bm, bevel)
    return bm


def _slant_cone_bm(radius: float, depth: float, segments: int,
                   cut_co: tuple[float, float, float],
                   cut_no: tuple[float, float, float]) -> bmesh.types.BMesh:
    """沿 Y 的圆柱，用平面斜切出一个斜面（AK 斜切枪口）。"""
    bm = _cone_bm(radius, radius, depth, segments, "Y")
    result = bmesh.ops.bisect_plane(
        bm, geom=list(bm.verts) + list(bm.edges) + list(bm.faces), dist=1e-6,
        plane_co=cut_co, plane_no=cut_no, clear_outer=True)
    cut_edges = [e for e in result["geom_cut"] if isinstance(e, bmesh.types.BMEdge)]
    if cut_edges:
        bmesh.ops.contextual_create(bm, geom=cut_edges)
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    return bm


# --------------------------------------------------------------------------- #
# 武器构建器：以米为单位，axis 约定 +X 右 / +Y 枪口 / +Z 上
# --------------------------------------------------------------------------- #
class WeaponBuilder:
    def __init__(self, wid: str) -> None:
        self.wid = wid
        self.accum = MeshAccum()

    # -- 基本体 ----------------------------------------------------------- #
    def box(self, name: str, center: tuple[float, float, float],
            size: tuple[float, float, float], mat: str,
            rot: tuple[float, float, float] = (0.0, 0.0, 0.0),
            bevel: float = 0.0015) -> None:
        bevel = min(bevel, 0.22 * min(size))
        bm = _box_bm(size, bevel)
        _place(bm, center, rot)
        self.accum.add(bm, mat, name)

    def cyl(self, name: str, center: tuple[float, float, float], length: float,
            radius: float, mat: str, axis: str = "Y", segments: int = 12,
            radius_end: float | None = None,
            rot: tuple[float, float, float] = (0.0, 0.0, 0.0)) -> None:
        end = radius if radius_end is None else radius_end
        bm = _cone_bm(radius, end, length, segments, axis)
        _place(bm, center, rot)
        self.accum.add(bm, mat, name)

    def prism(self, name: str, y0: float, y1: float, c0: tuple[float, float],
              h0: tuple[float, float], c1: tuple[float, float],
              h1: tuple[float, float], mat: str, bevel: float = 0.003) -> None:
        bm = _prism_bm(y0, y1, c0, h0, c1, h1, bevel)
        self.accum.add(bm, mat, name)

    def slant_cyl(self, name: str, center: tuple[float, float, float], length: float,
                  radius: float, mat: str, cut_co: tuple[float, float, float],
                  cut_no: tuple[float, float, float], segments: int = 12) -> None:
        bm = _slant_cone_bm(radius, length, segments, cut_co, cut_no)
        _place(bm, center, (0.0, 0.0, 0.0))
        self.accum.add(bm, mat, name)

    # -- 收尾 ------------------------------------------------------------- #
    def finish(self, mats: dict[str, bpy.types.Material]) -> bpy.types.Object:
        obj = self.accum.to_object(self.wid, mats)
        return obj

    @property
    def part_count(self) -> int:
        return len(self.accum.part_names)
