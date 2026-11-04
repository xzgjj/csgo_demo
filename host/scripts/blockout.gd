class_name BlockoutBuilder
extends Node3D
## 从 res://scenes/de_ash_boxes.json 构建 blockout，并逐盒按 kind/prop 选择材质。
## JSON 由 tools/mapgen/gen_blockout.py 从 content/maps/de_ash.layout.json 生成（已是 Godot 坐标）。
## 内核当前仍用 MakeArena（±800，跨度 1600），所以 blockout 等比缩放到 1600 使可见墙与碰撞墙对齐；
## 内核接入 de_ash MapDef 后把 TARGET_SPAN 改为 4096 并删除缩放即可。

const BOXES_PATH := "res://scenes/de_ash_boxes.json"
const TARGET_SPAN := 1600.0

var box_count := 0
var material_counts: Dictionary = {}
var scale_factor := 1.0
var ground_offset := 0.0


func build() -> bool:
	var text := FileAccess.get_file_as_string(BOXES_PATH)
	if text.is_empty():
		push_error("缺少 %s，先跑 tools/mapgen/gen_blockout.py" % BOXES_PATH)
		return false
	var data: Variant = JSON.parse_string(text)
	if typeof(data) != TYPE_DICTIONARY:
		push_error("blockout JSON 非法: " + BOXES_PATH)
		return false
	var boxes: Array = data.get("boxes", [])
	if boxes.is_empty():
		push_error("blockout 无盒子: " + BOXES_PATH)
		return false
	scale_factor = TARGET_SPAN / maxf(_source_span(boxes), 1.0)
	ground_offset = _ground_offset(boxes, scale_factor)
	for item: Dictionary in boxes:
		_add_box(item)
	print("BLOCKOUT boxes=%d scale=%.4f ground=%.2f kinds=%s" % [box_count, scale_factor, ground_offset, material_counts])
	return true


func _source_span(boxes: Array) -> float:
	var span := 0.0
	for item: Dictionary in boxes:
		for axis in item.get("size", []):
			span = maxf(span, float(axis))
	return span


## 让 floor 盒顶面对齐 sim 地面 y=0（生成器把地板做成 -8..+8 的厚板）。
func _ground_offset(boxes: Array, scale: float) -> float:
	for item: Dictionary in boxes:
		if str(item.get("kind", "")) != "floor":
			continue
		var size := _vector(item.get("size", []), Vector3.ZERO)
		var center := _vector(item.get("center", []), Vector3.ZERO)
		return -(center.y + size.y * 0.5) * scale
	return 0.0


func _add_box(item: Dictionary) -> void:
	var size := _vector(item.get("size", []), Vector3.ONE)
	var center := _vector(item.get("center", []), Vector3.ZERO)
	var kind := str(item.get("kind", ""))
	var material := MaterialLib.material_for_kind(kind, size)
	var mesh_instance := MeshInstance3D.new()
	var box_mesh := BoxMesh.new()
	box_mesh.size = size * scale_factor
	box_mesh.material = material
	mesh_instance.mesh = box_mesh
	mesh_instance.position = center * scale_factor + Vector3(0.0, ground_offset, 0.0)
	mesh_instance.rotation.y = float(item.get("rot_y", 0.0))
	add_child(mesh_instance)
	box_count += 1
	var label := kind if kind != "prop" else "prop/" + MaterialLib.prop_material_id(size)
	material_counts[label] = int(material_counts.get(label, 0)) + 1


func _vector(values: Array, fallback: Vector3) -> Vector3:
	if values.size() < 3:
		return fallback
	return Vector3(float(values[0]), float(values[1]), float(values[2]))
