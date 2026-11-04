class_name BlockoutBuilder
extends Node3D
## 从 res://scenes/de_ash_boxes.json 构建 de_ash 全尺寸 blockout（4096），逐盒按 material 选材质。
## JSON 由 tools/mapgen/gen_blockout.py 从 content/maps/de_ash.layout.json 派生（已是 Godot 坐标），
## 盒子含 material / collision 字段；本类只做表现，不参与内核碰撞（内核由 MapDef 收 AABB）。

const BOXES_PATH := "res://scenes/de_ash_boxes.json"

var box_count := 0
var material_counts: Dictionary = {}
var spawn_anchors: Array[Vector3] = []
var site_anchors: Array[Vector3] = []


func build(boxes_path: String = BOXES_PATH) -> bool:
	var text := FileAccess.get_file_as_string(boxes_path)
	if text.is_empty():
		push_error("缺少 %s，先跑 tools/mapgen/gen_blockout.py" % boxes_path)
		return false
	var data: Variant = JSON.parse_string(text)
	if typeof(data) != TYPE_DICTIONARY:
		push_error("blockout JSON 非法: " + boxes_path)
		return false
	var boxes: Array = data.get("boxes", [])
	if boxes.is_empty():
		push_error("blockout 无盒子: " + boxes_path)
		return false
	for item: Dictionary in boxes:
		_add_box(item)
	print("BLOCKOUT boxes=%d kinds=%s" % [box_count, material_counts])
	return true


func _add_box(item: Dictionary) -> void:
	var size := _vector(item.get("size", []), Vector3.ONE)
	var center := _vector(item.get("center", []), Vector3.ZERO)
	var kind := str(item.get("kind", ""))
	var material := _material_for(item, size)
	var mesh_instance := MeshInstance3D.new()
	var box_mesh := BoxMesh.new()
	box_mesh.size = size
	box_mesh.material = material
	mesh_instance.mesh = box_mesh
	mesh_instance.position = center
	mesh_instance.rotation.y = float(item.get("rot_y", 0.0))
	add_child(mesh_instance)
	box_count += 1
	if kind == "spawn":
		spawn_anchors.append(mesh_instance.position)
	elif kind == "site":
		site_anchors.append(mesh_instance.position)
	var label := kind if kind != "prop" else "prop/" + MaterialLib.prop_material_id(size)
	material_counts[label] = int(material_counts.get(label, 0)) + 1


## 优先用派生的 material id（6 类基础材质），未知则按 kind/size 回退。
func _material_for(item: Dictionary, size: Vector3) -> StandardMaterial3D:
	var material_id := str(item.get("material", ""))
	if MaterialLib.is_base_id(material_id):
		return MaterialLib.get_material(material_id)
	var kind := str(item.get("kind", ""))
	if kind == "site":
		return MaterialLib.get_material("site")
	if kind == "spawn":
		return MaterialLib.get_material("spawn")
	return MaterialLib.material_for_kind(kind, size)


func _vector(values: Array, fallback: Vector3) -> Vector3:
	if values.size() < 3:
		return fallback
	return Vector3(float(values[0]), float(values[1]), float(values[2]))
