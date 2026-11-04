"""预览渲染：固定三点灯 + 4 机位（右侧面 / 3/4 前 / 俯视 / FP），EEVEE 960x540。"""

from __future__ import annotations

import math
from pathlib import Path

import bpy
from mathutils import Euler, Vector

from .paths import DOC_DIR

RENDER_W = 960
RENDER_H = 540
RENDER_SAMPLES = 32


LIGHT_SPECS = (
    ("key", (1.10, 0.95, 1.15), 45.0, 1.10, (1.0, 0.97, 0.92)),
    ("fill", (-1.25, 0.55, 0.45), 10.0, 1.40, (0.84, 0.90, 1.0)),
    ("rim", (0.25, -1.30, 0.95), 28.0, 0.80, (0.88, 0.94, 1.0)),
)
VIEW_NAMES = ("side", "front34", "top", "fp")


def setup_render(scene: bpy.types.Scene) -> dict[str, bpy.types.Object]:
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = RENDER_W
    scene.render.resolution_y = RENDER_H
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGB"
    scene.render.film_transparent = False
    try:
        scene.eevee.taa_render_samples = RENDER_SAMPLES
    except AttributeError:
        pass
    try:
        scene.view_settings.view_transform = "Standard"
        scene.view_settings.look = "None"
    except (AttributeError, TypeError):
        pass
    world = bpy.data.worlds.new("weapon_world")
    world.use_nodes = True
    nodes = world.node_tree.nodes
    links = world.node_tree.links
    nodes.clear()
    output = nodes.new("ShaderNodeOutputWorld")
    mix = nodes.new("ShaderNodeMixShader")
    light_path = nodes.new("ShaderNodeLightPath")
    background_camera = nodes.new("ShaderNodeBackground")
    background_ambient = nodes.new("ShaderNodeBackground")
    background_camera.inputs[0].default_value = (0.560, 0.580, 0.610, 1.0)
    background_camera.inputs[1].default_value = 1.0
    background_ambient.inputs[0].default_value = (0.350, 0.370, 0.420, 1.0)
    background_ambient.inputs[1].default_value = 0.45
    links.new(light_path.outputs["Is Camera Ray"], mix.inputs[0])
    links.new(background_ambient.outputs[0], mix.inputs[1])
    links.new(background_camera.outputs[0], mix.inputs[2])
    links.new(mix.outputs[0], output.inputs[0])
    scene.world = world
    rig = bpy.data.collections.new("RenderRig")
    scene.collection.children.link(rig)
    target = bpy.data.objects.new("render_target", None)
    target.empty_display_size = 0.1
    rig.objects.link(target)
    lights: dict[str, bpy.types.Object] = {}
    for name, offset, energy, size, color in LIGHT_SPECS:
        data = bpy.data.lights.new(f"light_{name}", type="AREA")
        data.energy = energy
        data.color = color
        light = bpy.data.objects.new(f"light_{name}", data)
        rig.objects.link(light)
        light["offset"] = offset
        light["size"] = size
        lights[name] = light
    cameras: dict[str, bpy.types.Object] = {}
    for name in VIEW_NAMES:
        data = bpy.data.cameras.new(f"cam_{name}")
        data.clip_start = 0.01
        data.clip_end = 100.0
        camera = bpy.data.objects.new(f"cam_{name}", data)
        rig.objects.link(camera)
        cameras[name] = camera
    scene.camera = cameras["side"]
    return {"target": target, "cameras": cameras, "lights": lights}


def _aim(obj: bpy.types.Object, target: Vector) -> None:
    direction = target - obj.location
    if direction.length < 1e-6:
        return
    obj.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()


def place_rig(rig: dict[str, bpy.types.Object], center: Vector,
              dims: Vector) -> None:
    length = max(dims)
    for name, light in rig["lights"].items():
        offset = Vector(light["offset"]) * length
        light.location = center + offset
        light.data.size = light["size"] * length
        _aim(light, center)
    cameras = rig["cameras"]
    side = cameras["side"]
    side.data.type = "ORTHO"
    side.data.ortho_scale = max(dims.y, dims.z) * 1.14
    side.location = center + Vector((length * 2.4, 0.0, 0.0))
    _aim(side, center)

    top = cameras["top"]
    top.data.type = "ORTHO"
    top.data.ortho_scale = max(dims.y, dims.x) * 1.14
    top.location = center + Vector((0.0, 0.0, length * 2.4))
    top.rotation_euler = Euler((0.0, 0.0, math.radians(90.0)), "XYZ")

    front = cameras["front34"]
    front.data.type = "PERSP"
    front.data.lens = 55.0
    direction = Vector((0.62, 0.62, 0.42)).normalized()
    front.location = center + direction * (length * 2.0)
    _aim(front, center - Vector((0.0, 0.0, dims.z * 0.04)))

    ## FP：模拟 FPS 视角——枪身从画面右下向远处延伸，看见枪左侧面（枪托被画面底边裁掉）。
    fp = cameras["fp"]
    fp.data.type = "PERSP"
    fp.data.lens = 22.0
    fp.location = center + Vector((-0.30, -0.68, 0.32)) * (length / 0.95)
    _aim(fp, center + Vector((0.0, 1.6, -0.10)))


def render_view(scene: bpy.types.Scene, camera: bpy.types.Object, path: Path) -> None:
    scene.camera = camera
    path.parent.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str(path)
    bpy.ops.render.render(write_still=True)


def render_all_views(scene: bpy.types.Scene, rig: dict[str, bpy.types.Object],
                     wid: str, objects: dict[str, bpy.types.Object]) -> list[Path]:
    """渲染单把枪的 4 个机位；渲染时隐藏另外两把（同场景叠在一起会糊成一团）。"""
    for other_id, other in objects.items():
        other.hide_render = other_id != wid
    outputs: list[Path] = []
    for view in VIEW_NAMES:
        path = DOC_DIR / f"{wid}_{view}.png"
        render_view(scene, rig["cameras"][view], path)
        outputs.append(path)
    return outputs
