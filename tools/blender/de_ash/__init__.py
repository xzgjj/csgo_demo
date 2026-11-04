"""de_ash Blender 构建包：数据 / 材质 / 几何 / 装饰件 / 灯光 / 渲染 / 导出。

主脚本为 ``tools/blender/build_de_ash.py``（只做编排与 CLI）；
本包内每个模块只负责一类职责，便于 CI 的文件行数与函数复杂度预算。
"""

from __future__ import annotations
