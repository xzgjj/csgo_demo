"""武器 4 类共享材质：深灰金属 / 棕木 / 近黑聚合物 / 镜片玻璃（glTF 只嵌入颜色）。"""

from __future__ import annotations

import bpy

MAT_KEYS = ("gun_metal", "gun_wood", "gun_polymer", "gun_glass")

## 4 类共享材质：深灰金属 / 棕木 / 近黑聚合物（深橄榄调，供 AWP 绿黑枪身）/ 镜片玻璃。
MATERIAL_SPECS = {
    "gun_metal": {
        "color": (0.130, 0.140, 0.155), "metallic": 0.88, "roughness": 0.36,
    },
    "gun_wood": {
        "color": (0.110, 0.050, 0.023), "metallic": 0.0, "roughness": 0.78,
    },
    "gun_polymer": {
        "color": (0.038, 0.050, 0.034), "metallic": 0.12, "roughness": 0.60,
    },
    "gun_glass": {
        "color": (0.035, 0.085, 0.150), "metallic": 0.0, "roughness": 0.06,
        "emission": (0.120, 0.320, 0.620), "emission_strength": 0.28,
    },
}


def make_materials() -> dict[str, bpy.types.Material]:
    mats: dict[str, bpy.types.Material] = {}
    for key, spec in MATERIAL_SPECS.items():
        material = bpy.data.materials.new(key)
        material.use_nodes = True
        bsdf = material.node_tree.nodes.get("Principled BSDF")
        color = spec["color"]
        bsdf.inputs["Base Color"].default_value = (*color, 1.0)
        bsdf.inputs["Metallic"].default_value = spec["metallic"]
        bsdf.inputs["Roughness"].default_value = spec["roughness"]
        if "emission" in spec:
            emission = spec["emission"]
            bsdf.inputs["Emission Color"].default_value = (*emission, 1.0)
            bsdf.inputs["Emission Strength"].default_value = spec["emission_strength"]
        material.diffuse_color = (*color, 1.0)
        mats[key] = material
    return mats
