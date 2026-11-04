"""武器子包与主脚本共用的项目路径（层级：tools/blender/weapons/paths.py）。"""

from __future__ import annotations

from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[3]
GLB_DIR = PROJECT_ROOT / "host" / "models" / "weapons"
VERSION_DIR = PROJECT_ROOT / "assets" / "source" / "weapons" / "versions"
DOC_DIR = PROJECT_ROOT / "docs" / "design" / "weapons"
