"""程序化武器建模：AK-47 / M4A4 / AWP → GLB（Blender 后台独立进程，只编排）。

用法：
    blender --background --factory-startup --python tools/blender/build_weapons.py -- [选项]

选项：
    --no-render   只建模 / 导出 / 存版本，不渲染预览图
    --recheck     反查模式：重新导入已导出的 GLB 渲染「GLB 复检图」，不重新建模

坐标约定：建模使用 Blender 坐标（+X 右、+Y 枪口、+Z 上）。glTF 默认 Y-up 导出，
落地到 Godot 相机空间（+X 右、+Y 上、-Z 枪口）；单位米，原点位于机匣/握把附近。

产物：
    host/models/weapons/{ak47,m4a4,awp}.glb
    assets/source/weapons/versions/weapons_vNNN.blend（只保留最近 3 个）
    docs/design/weapons/<id>_{side,front34,top,fp}.png
    docs/design/weapons/<id>_glb_{side,fp}.png（--recheck）

代码分布：几何构件 weapons/meshkit.py，部件表 weapons/parts_{ak,m4,awp}.py，
材质 weapons/materials.py，渲染 weapons/render.py，流水线 weapons/build.py；
GLB 导出/解析/版本保留与地图脚本共用 tools/blender/common.py。
确定性：无随机数、无外部素材/贴图；几何全部由盒体 / 圆柱 / 锥台 / 倒角组合。
"""

from __future__ import annotations

import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from weapons import build  # noqa: E402  （必须先设置 sys.path 再导入）


def main() -> int:
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except (AttributeError, ValueError):
        pass
    if "--recheck" in argv:
        return build.recheck_glb()
    return build.build_all(render="--no-render" not in argv)


if __name__ == "__main__":
    sys.exit(main())
