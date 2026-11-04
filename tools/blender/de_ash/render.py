"""de_ash 渲染：EEVEE 固定采样 / 1280x720 / 六机位输出到 docs/design/de_ash_renders。"""

from __future__ import annotations

import time

import bpy

from .data import VIEW_ORDER


def setup_render(scene, width, height, samples):
    """固定渲染参数：EEVEE、固定采样、关闭运动模糊（保证可复现）。"""
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = width
    scene.render.resolution_y = height
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGB"
    scene.render.image_settings.color_depth = "8"
    scene.render.film_transparent = False
    if hasattr(scene.render, "use_motion_blur"):
        scene.render.use_motion_blur = False
    if hasattr(scene, "eevee"):
        scene.eevee.taa_render_samples = samples
        scene.eevee.use_shadows = True
        scene.eevee.use_fast_gi = True
        _set_eevee_options(scene.eevee)
    for transform in ("Standard", "Filmic", "AgX"):
        try:
            scene.view_settings.view_transform = transform
            break
        except (AttributeError, TypeError):
            continue
    try:
        scene.view_settings.look = "None"
    except (AttributeError, TypeError):
        pass
    scene.view_settings.exposure = 0.0
    scene.view_settings.gamma = 1.0
    scene.frame_set(1)


def _set_eevee_options(eevee):
    """按版本设置 EEVEE 选项（属性不存在时静默跳过）。"""
    try:
        eevee.fast_gi_distance = 700.0
        eevee.fast_gi_ray_count = 2
        eevee.fast_gi_step_count = 8
    except (AttributeError, TypeError):
        pass
    try:
        eevee.use_raytracing = False
    except (AttributeError, TypeError):
        pass


def render_views(scene, cameras, output_dir):
    """按 VIEW_ORDER 渲染 6 机位 PNG，返回输出路径列表。"""
    output_dir.mkdir(parents=True, exist_ok=True)
    paths = []
    for name in VIEW_ORDER:
        scene.camera = cameras[name]
        path = output_dir / f"de_ash_{name}.png"
        scene.render.filepath = str(path)
        started = time.time()
        bpy.ops.render.render(write_still=True)
        paths.append(path)
        elapsed = time.time() - started
        print(f"RENDER {name} {elapsed:.1f}s -> {path}")
    return paths
