"""GLB 与 de_ash_boxes.json 一致性：盒数、索引、中心、kind、材质、碰撞标记。

用 box_* 节点的 extras 对齐，避免“画面换模型、碰撞还是旧盒子”。
用法：python -X utf8 tools/mapgen/check_glb_boxes.py
退出码：0 一致；1 漂移。
"""

from __future__ import annotations

import json
import struct

import paths

EPS = 0.01


def _glb_doc(path):
    data = path.read_bytes()
    magic, version, length = struct.unpack_from("<III", data, 0)
    if magic != 0x46546C67 or version != 2 or length != len(data):
        raise ValueError("不是合法的 GLB 2.0 文件")
    offset = 12
    while offset + 8 <= len(data):
        chunk_len, chunk_type = struct.unpack_from("<II", data, offset)
        if chunk_type == 0x4E4F534A:
            return json.loads(data[offset + 8:offset + 8 + chunk_len])
        offset += 8 + chunk_len
    raise ValueError("GLB 缺少 JSON chunk")


def main() -> int:
    if not paths.GLB.exists():
        print("FAIL 缺少 host/models/map/de_ash.glb")
        return 1
    boxes = json.loads(paths.BOXES.read_text(encoding="utf-8"))["boxes"]
    nodes = [n for n in _glb_doc(paths.GLB).get("nodes", []) if n.get("name", "").startswith("box_")]
    issues = []
    if len(nodes) != len(boxes):
        issues.append(f"盒数漂移: glb {len(nodes)} / json {len(boxes)}")
    for node in nodes:
        extras = node.get("extras", {})
        index = extras.get("box_index")
        if not isinstance(index, int) or index < 0 or index >= len(boxes):
            issues.append(f"{node.get('name')}: box_index 非法")
            continue
        want = boxes[index]
        if node.get("translation") != want["center"]:
            issues.append(f"{node.get('name')}: 中心漂移 {node.get('translation')} != {want['center']}")
        for key, glb_key in (("kind", "kind"), ("material", "material_id"),
                             ("collision", "collision")):
            if extras.get(glb_key) != want.get(key):
                issues.append(f"{node.get('name')}: {key} 漂移")
    if issues:
        print("FAIL " + "; ".join(issues[:8]))
        return 1
    print(f"glbcheck PASS boxes={len(boxes)} nodes={len(nodes)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
