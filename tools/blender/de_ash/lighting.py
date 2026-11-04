"""de_ash 灯光与机位：落日主光 + 冷色补光 + 渐变天空 + 6 台与 Godot 一致的相机。"""

from __future__ import annotations

import math

import bpy
from mathutils import Vector

from .data import FOV_DEG, VIEWS, VIEW_ORDER, godot_to_blender_point


def _sky_background(nodes, links):
    """可见天空：Generated（视线方向）Z 分量做竖直渐变，暖地平线 → 深蓝天顶。"""
    tex_coord = nodes.new("ShaderNodeTexCoord")
    separate = nodes.new("ShaderNodeSeparateXYZ")
    remap = nodes.new("ShaderNodeMapRange")
    remap.inputs["From Min"].default_value = -1.0
    remap.inputs["From Max"].default_value = 1.0
    remap.inputs["To Min"].default_value = 0.0
    remap.inputs["To Max"].default_value = 1.0
    ramp = nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.interpolation = "EASE"
    stops = (
        (0.44, (0.26, 0.18, 0.12, 1.0)),   # 地平线以下：暗沙
        (0.50, (0.98, 0.58, 0.28, 1.0)),   # 地平线：暖金
        (0.62, (0.72, 0.55, 0.38, 1.0)),   # 低空：暖雾
        (0.78, (0.24, 0.36, 0.58, 1.0)),   # 中空：天蓝
        (0.95, (0.07, 0.15, 0.34, 1.0)),   # 天顶：深蓝
    )
    elements = ramp.color_ramp.elements
    elements[0].position = stops[0][0]
    elements[0].color = stops[0][1]
    elements[1].position = stops[-1][0]
    elements[1].color = stops[-1][1]
    for position, color in stops[1:-1]:
        elements.new(position).color = color
    links.new(tex_coord.outputs["Generated"], separate.inputs[0])
    links.new(separate.outputs["Z"], remap.inputs["Value"])
    links.new(remap.outputs["Result"], ramp.inputs[0])
    return ramp


def _add_sun(collection, name, direction, energy, color, angle_deg, shadow):
    data = bpy.data.lights.new(name, type="SUN")
    data.energy = energy
    data.color = color
    data.angle = math.radians(angle_deg)
    data.use_shadow = shadow
    if hasattr(data, "shadow_cascade_count"):
        try:
            data.shadow_cascade_count = 4
            data.shadow_cascade_max_distance = 6000.0
            data.shadow_cascade_exponent = 1.0
            data.shadow_cascade_fade = 0.15
            data.shadow_maximum_resolution = 0.05
        except (AttributeError, TypeError):
            pass
    obj = bpy.data.objects.new(name, data)
    collection.objects.link(obj)
    vector = Vector(direction).normalized()
    obj.rotation_euler = vector.to_track_quat("-Z", "Y").to_euler()
    return obj


def setup_lighting(scene, collection):
    """暖色落日主光 + 冷色补光 + 沙漠天空环境光（固定参数，无随机）。"""
    world = bpy.data.worlds.new("de_ash_sky")
    scene.world = world
    world.use_nodes = True
    nodes = world.node_tree.nodes
    links = world.node_tree.links
    nodes.clear()
    output = nodes.new("ShaderNodeOutputWorld")
    mix = nodes.new("ShaderNodeMixShader")
    light_path = nodes.new("ShaderNodeLightPath")
    ambient = nodes.new("ShaderNodeBackground")
    visible = nodes.new("ShaderNodeBackground")
    ambient.inputs[0].default_value = (0.48, 0.54, 0.66, 1.0)
    ambient.inputs[1].default_value = 1.0
    visible.inputs[0].default_value = (0.5, 0.5, 0.5, 1.0)
    visible.inputs[1].default_value = 1.0
    links.new(_sky_background(nodes, links).outputs["Color"], visible.inputs[0])
    links.new(light_path.outputs["Is Camera Ray"], mix.inputs[0])
    links.new(ambient.outputs[0], mix.inputs[1])
    links.new(visible.outputs[0], mix.inputs[2])
    links.new(mix.outputs[0], output.inputs[0])

    if hasattr(scene, "eevee"):
        scene.eevee.direct_light_intensity = 1.0
        scene.eevee.indirect_light_intensity = 1.0

    _add_sun(collection, "sun_key", (-0.52, 0.60, -0.61), 5.0,
             (1.0, 0.87, 0.70), 1.4, True)
    _add_sun(collection, "sun_fill", (0.58, -0.42, -0.70), 1.35,
             (0.94, 0.88, 0.82), 12.0, False)

    # 地面暖色反光（低强度面光，只提亮地面与墙面下缘，不投影）
    bounce_data = bpy.data.lights.new("light_bounce", type="AREA")
    bounce_data.shape = "RECTANGLE"
    bounce_data.size = 6000.0
    bounce_data.size_y = 6000.0
    bounce_data.energy = 220000.0
    bounce_data.color = (1.0, 0.82, 0.66)
    bounce_data.use_shadow = False
    bounce = bpy.data.objects.new("light_bounce", bounce_data)
    collection.objects.link(bounce)
    bounce.location = (0.0, 0.0, 900.0)
    bounce.rotation_euler = (0.0, 0.0, 0.0)


def build_cameras(collection):
    """6 台相机：位置/朝向与 host/tests/capture_de_ash.gd 的 views 一一对应。"""
    cameras = {}
    for name in VIEW_ORDER:
        position, target, top_down = VIEWS[name]
        cameras[name] = _build_camera(collection, name, position, target, top_down)
    return cameras


def _build_camera(collection, name, position, target, top_down):
    data = bpy.data.cameras.new(f"cam_{name}")
    data.sensor_fit = "VERTICAL"
    data.angle = math.radians(FOV_DEG)
    data.clip_start = 5.0
    data.clip_end = 20000.0
    camera = bpy.data.objects.new(f"cam_{name}", data)
    collection.objects.link(camera)
    camera.location = Vector(godot_to_blender_point(position))
    if top_down:
        # Godot 俯视机位 up=(0,0,-1) → Blender 恒等旋转（局部 -Z 朝下、+Y 朝上）
        camera.rotation_euler = (0.0, 0.0, 0.0)
    else:
        direction = Vector(godot_to_blender_point(target)) - camera.location
        camera.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    return camera
