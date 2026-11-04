"""de_ash 材质：按 assets/materials/README.md 的 PBR 参数建 8 个材质。"""

from __future__ import annotations

import bpy

from .data import MATERIAL_SPECS


def srgb_to_linear(value):
    """sRGB 0-1 → 线性（Blender Principled / glTF baseColor 均为线性）。"""
    value = min(max(float(value), 0.0), 1.0)
    if value <= 0.04045:
        return value / 12.92
    return ((value + 0.055) / 1.055) ** 2.4


def linear_rgb(color):
    return tuple(srgb_to_linear(channel) for channel in color)


def make_materials():
    """建 6 类 PBR + site/spawn 高亮材质；玻璃用 alpha 混合，站点用自发光。"""
    materials = {}
    for key, spec in MATERIAL_SPECS.items():
        materials[key] = _make_material(key, spec)
    return materials


def _make_material(key, spec):
    material = bpy.data.materials.new(f"mat_{key}")
    material.use_nodes = True
    nodes = material.node_tree.nodes
    bsdf = next(node for node in nodes if node.type == "BSDF_PRINCIPLED")
    base = linear_rgb(spec["color"])
    alpha = spec.get("alpha", 1.0)
    bsdf.inputs["Base Color"].default_value = (*base, 1.0)
    bsdf.inputs["Metallic"].default_value = spec["metallic"]
    bsdf.inputs["Roughness"].default_value = spec["roughness"]
    bsdf.inputs["Alpha"].default_value = alpha
    if "emission" in spec:
        emission = linear_rgb(spec["emission"])
        bsdf.inputs["Emission Color"].default_value = (*emission, 1.0)
        bsdf.inputs["Emission Strength"].default_value = spec["emission_strength"]
    if alpha < 1.0:
        _set_blended(material)
    material.diffuse_color = (*base, alpha)
    return material


def _set_blended(material):
    """Blender 4.2+ 用 surface_render_method，旧版用 blend_method。"""
    if not hasattr(material, "surface_render_method"):
        return
    for method in ("BLENDED", "BLEND"):
        try:
            material.surface_render_method = method
        except (AttributeError, TypeError):
            continue
        material.use_transparent_shadow = True
        return
