"""Blender GUI 内渲染 dust2 顶视预览（不关闭 Blender，供 watch_map 取帧）。

由 tools/blender/blender.ps1 blender_py 注入执行；读取当前场景的 dust2_build 集合。
输出：assets/preview/dust2_blender_top.png
"""

import bpy  # type: ignore


def main() -> int:
    scene = bpy.context.scene
    cam = bpy.data.objects.get("dust2_top_cam")
    if cam is None:
        data = bpy.data.cameras.new("dust2_top_cam")
        cam = bpy.data.objects.new("dust2_top_cam", data)
        scene.collection.objects.link(cam)
    cam.location = (0.0, -300.0, 2600.0)
    cam.rotation_euler = (0.0, 0.0, 0.0)
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = 5200.0
    cam.data.clip_end = 100000.0
    scene.camera = cam
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.render.resolution_x = 1024
    scene.render.resolution_y = 1024
    scene.render.resolution_percentage = 100
    scene.render.filepath = (
        "D:/c++_myproject/game_project/csgo_demo/assets/preview/dust2_blender_top.png"
    )
    bpy.ops.render.render(write_still=True)
    print("DUST2_FRAME", scene.render.filepath)
    return 0


main()
