class_name MapDecor
extends RefCounted
## 地图装饰（纯视觉，无碰撞）：A/B 招牌、棕榈、街道油桶。
## 位置来自真实的包点锚点 + 盒子净空检测：候选点卡进墙/道具就换一个候选，绝不再用固定坐标穿模。
## 只做表现，不参与规则；`boxes` 是 BlockoutBuilder 收的 AABB 列表（Godot 坐标）。

const SIGN_COLOR := Color(0.95, 0.75, 0.25)
const TRUNK_COLOR := Color(0.35, 0.24, 0.14)
const LEAF_COLOR := Color(0.20, 0.45, 0.18)
const BARREL_COLOR := Color(0.30, 0.38, 0.42)
## 装饰占地半径：树干 16 / 棕榈冠 40 / 油桶 30 / 招牌柱 12。
const PALM_RADIUS := 40.0
const PALM_HEIGHT := 230.0
const BARREL_RADIUS := 30.0
const BARREL_HEIGHT := 70.0


static func add(parent: Node3D, site_a: Vector3, site_b: Vector3,
		boxes: Array = []) -> void:
	_add_site_sign(parent, "A", site_a, boxes)
	_add_site_sign(parent, "B", site_b, boxes)
	for site in [site_a, site_b]:
		for offset in [Vector3(420, 0, 280), Vector3(-420, 0, 320),
				Vector3(380, 0, -300), Vector3(-380, 0, -320),
				Vector3(300, 0, 0), Vector3(-300, 0, 0)]:
			_add_if_free(parent, site + offset, PALM_RADIUS, PALM_HEIGHT, boxes,
					_add_palm)
		for offset in [Vector3(220, 0, -120), Vector3(-240, 0, 160),
				Vector3(260, 0, 180)]:
			_add_if_free(parent, site + offset, BARREL_RADIUS, BARREL_HEIGHT, boxes,
					_add_barrel)


## 在候选点可站立的情况下放置一个装饰；被实体占住就直接跳过（宁缺毋滥）。
static func _add_if_free(parent: Node3D, base: Vector3, radius: float, height: float,
		boxes: Array, builder: Callable) -> void:
	if blocked(boxes, base, radius, height):
		return
	builder.call(parent, base)


## 圆柱占地近似为 AABB；只挡“脚底到顶”这段，屋顶层（base_z 高于 2m）不参与。
static func blocked(boxes: Array, base: Vector3, radius: float, height: float) -> bool:
	var probe := {
		"min": [base.x - radius, base.y, base.z - radius],
		"max": [base.x + radius, base.y + height, base.z + radius],
	}
	for box: Dictionary in boxes:
		var box_min: Array = box.get("min", [])
		var box_max: Array = box.get("max", [])
		if box_min.size() < 3 or box_max.size() < 3:
			continue
		if float(box_max[1]) <= 2.0:
			continue  # 地面板不阻挡装饰
		var hit := true
		for axis in 3:
			if (float(probe["min"][axis]) >= float(box_max[axis])
					or float(probe["max"][axis]) <= float(box_min[axis])):
				hit = false
				break
		if hit:
			return true
	return false


static func _add_site_sign(parent: Node3D, text: String, site: Vector3,
		boxes: Array) -> void:
	var base := site
	if blocked(boxes, base, 12.0, 240.0):
		base = site + Vector3(0, 0, 96)
		if blocked(boxes, base, 12.0, 240.0):
			return
	var label := Label3D.new()
	label.text = text
	label.font_size = 140
	label.modulate = SIGN_COLOR
	label.outline_size = 16
	label.outline_modulate = Color(0.12, 0.08, 0.04)
	label.position = base + Vector3(0, 236, 0)
	label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	label.no_depth_test = false  # 让招牌被墙挡住：游戏里不该穿墙
	parent.add_child(label)
	var post := MeshInstance3D.new()
	var mesh := BoxMesh.new()
	mesh.size = Vector3(14, 236, 14)
	post.mesh = mesh
	post.material_override = _flat(TRUNK_COLOR)
	post.position = base + Vector3(0, 118, 0)
	parent.add_child(post)


static func _add_palm(parent: Node3D, base: Vector3) -> void:
	var trunk := MeshInstance3D.new()
	var trunk_mesh := CylinderMesh.new()
	trunk_mesh.top_radius = 7.0
	trunk_mesh.bottom_radius = 11.0
	trunk_mesh.height = 200.0
	trunk.mesh = trunk_mesh
	trunk.material_override = _flat(TRUNK_COLOR)
	trunk.position = base + Vector3(0, 100, 0)
	parent.add_child(trunk)
	for index in range(6):
		var leaf := MeshInstance3D.new()
		var leaf_mesh := BoxMesh.new()
		leaf_mesh.size = Vector3(140, 5, 34)
		leaf.mesh = leaf_mesh
		leaf.material_override = _flat(LEAF_COLOR)
		leaf.position = base + Vector3(0, 200, 0)
		leaf.rotation_degrees = Vector3(-14.0, index * 60.0, 0.0)
		parent.add_child(leaf)


static func _add_barrel(parent: Node3D, base: Vector3) -> void:
	var barrel := MeshInstance3D.new()
	var mesh := CylinderMesh.new()
	mesh.top_radius = 24.0
	mesh.bottom_radius = 24.0
	mesh.height = 68.0
	barrel.mesh = mesh
	barrel.material_override = _flat(BARREL_COLOR)
	barrel.position = base + Vector3(0, 34, 0)
	parent.add_child(barrel)


static func _flat(color: Color) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = 0.85
	return material
