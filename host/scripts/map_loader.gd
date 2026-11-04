class_name MapLoader
extends RefCounted
## de_ash 派生盒子（Godot 坐标）→ 内核 MapDef AABB：碰撞与画面用同一份数据。
## 盒子由 tools/mapgen/gen_blockout.py 从 layout JSON 派生（唯一几何事实源）。

const BOXES_PATH := "res://scenes/de_ash_boxes.json"


## 返回注入的碰撞盒数量；读取失败时保持调用方默认地图（返回 0）。
static func apply(sim: RefCounted) -> int:
	var text := FileAccess.get_file_as_string(BOXES_PATH)
	if text.is_empty():
		push_warning("缺少 de_ash_boxes.json，保持默认竞技场")
		return 0
	var data: Variant = JSON.parse_string(text)
	if typeof(data) != TYPE_DICTIONARY:
		push_warning("de_ash_boxes.json 非法，保持默认竞技场")
		return 0
	var flat := PackedFloat32Array()
	for item: Dictionary in data.get("boxes", []):
		if not bool(item.get("collision", true)):
			continue
		flat.append_array(_flatten_box(item))
	sim.set_map_boxes(flat)
	return flat.size() / 6


static func _flatten_box(item: Dictionary) -> PackedFloat32Array:
	var center: Array = item["center"]
	var size: Array = item["size"]
	var gx := float(center[0])
	var gy := float(center[1])
	var gz := float(center[2])
	var sx := float(size[0])
	var sy := float(size[1])
	var sz := float(size[2])
	return PackedFloat32Array([
		-gz - sz * 0.5, -gx - sx * 0.5, gy - sy * 0.5,
		-gz + sz * 0.5, -gx + sx * 0.5, gy + sy * 0.5,
	])
