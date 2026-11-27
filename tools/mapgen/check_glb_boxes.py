"""GLB 与 de_ash_boxes.json 一致性：盒数、索引、中心、kind、材质、碰撞标记。

用 box_* 节点的 extras 对齐，避免“画面换模型、碰撞还是旧盒子”。
用法：python -X utf8 tools/mapgen/check_glb_boxes.py
退出码：0 一致；1 漂移。
"""

from __future__ import annotations

import argparse
import json
import struct

from paths import ROOT

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


def _node_size(node, doc):
    """节点尺寸 = 网格包围盒 × 节点缩放。

    两种导出方式都要覆盖：de_dust2 用单位立方体 + scale 表达尺寸；
    de_ash 的旧 GLB 直接把尺寸烘进网格顶点、scale 恒为 1。
    """
    scale = node.get("scale", [1.0, 1.0, 1.0])
    mesh_index = node.get("mesh")
    meshes = doc.get("meshes", [])
    if not isinstance(mesh_index, int) or mesh_index >= len(meshes):
        return None
    bounds = None
    for prim in meshes[mesh_index].get("primitives", []):
        pos = prim.get("attributes", {}).get("POSITION", -1)
        accessors = doc.get("accessors", [])
        if not isinstance(pos, int) or pos < 0 or pos >= len(accessors):
            continue
        accessor = accessors[pos]
        lo, hi = accessor.get("min"), accessor.get("max")
        if lo and hi:
            bounds = [float(b - a) for a, b in zip(lo, hi)]
            break
    if bounds is None:
        return None
    return [abs(float(bounds[i] * float(scale[i]))) for i in range(3)]


def _check_center(node, want) -> str:
    got = node.get("translation", [])
    if len(got) < 3 or any(abs(float(a) - float(b)) > EPS for a, b in zip(got, want["center"])):
        return f"{node.get('name')}: 中心漂移 {got} != {want['center']}"
    return ""


def _check_size(node, want, doc) -> str:
    # 尺寸漂移比中心漂移更隐蔽：Blender 里改墙高不会改中心，旧版本只比中心会漏掉 512→256。
    got = _node_size(node, doc)
    if got is None or any(abs(a - float(b)) > EPS for a, b in zip(got, want["size"])):
        return f"{node.get('name')}: 尺寸漂移 {got or node.get('scale')} != {want['size']}"
    return ""


def _check_meta(node, want) -> list[str]:
    issues = []
    for key, glb_key in (("kind", "kind"), ("material", "material_id"),
                         ("collision", "collision")):
        # GLB 只有几何，没有“碰撞开关”；屋顶/上层体块标 collision=false 只做视觉，这里不比较。
        if key == "collision" and not want.get(key):
            continue
        if node.get("extras", {}).get(glb_key) != want.get(key):
            issues.append(f"{node.get('name')}: {key} 漂移")
    return issues


def _compare(boxes, nodes, doc) -> list[str]:
    issues = []
    if len(nodes) != len(boxes):
        issues.append(f"盒数漂移: glb {len(nodes)} / json {len(boxes)}")
    for node in nodes:
        index = node.get("extras", {}).get("box_index")
        if not isinstance(index, int) or index < 0 or index >= len(boxes):
            issues.append(f"{node.get('name')}: box_index 非法")
            continue
        want = boxes[index]
        center_err = _check_center(node, want)
        size_err = _check_size(node, want, doc)
        issues += [x for x in (center_err, size_err) if x]
        issues += _check_meta(node, want)
    return issues


def main() -> int:
    parser = argparse.ArgumentParser(description="GLB ↔ boxes 一致性")
    parser.add_argument("--map", default="de_ash")
    args = parser.parse_args()
    boxes_path = ROOT / f"host/scenes/{args.map}_boxes.json"
    glb_path = ROOT / ("host/models/map/de_ash.glb" if args.map == "de_ash"
                       else f"host/models/map/{args.map}.glb")
    if not glb_path.exists():
        print(f"FAIL 缺少 {glb_path.relative_to(ROOT)}")
        return 1
    boxes = json.loads(boxes_path.read_text(encoding="utf-8"))["boxes"]
    doc = _glb_doc(glb_path)
    nodes = [n for n in doc.get("nodes", []) if n.get("name", "").startswith("box_")]
    issues = _compare(boxes, nodes, doc)
    if issues:
        print("FAIL " + "; ".join(issues[:8]))
        return 1
    print(f"glbcheck PASS map={args.map} boxes={len(boxes)} nodes={len(nodes)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
