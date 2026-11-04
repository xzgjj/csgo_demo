class_name MapDecor
extends RefCounted
## 地图装饰（纯视觉，无碰撞）：A/B 招牌、棕榈、街道油桶/长椅。由 de_ash_world 调用。
## 与碰撞/规则无关；位置全部在布局空白处，避让车道与包点通道。

const SIGN_COLOR := Color(0.95, 0.75, 0.25)
const TRUNK_COLOR := Color(0.35, 0.24, 0.14)
const LEAF_COLOR := Color(0.20, 0.45, 0.18)


static func add(parent: Node3D, site_a: Vector3, site_b: Vector3) -> void:
	_add_sign(parent, "A", site_a + Vector3(0, 260, 0))
	_add_sign(parent, "B", site_b + Vector3(0, 260, 0))
	for offset in [Vector3(420, 0, 280), Vector3(-420, 0, 320), Vector3(380, 0, -300),
			Vector3(-380, 0, -320)]:
		_add_palm(parent, site_a + offset)
		_add_palm(parent, site_b - offset)
	for offset in [Vector3(220, 0, -120), Vector3(-240, 0, 160), Vector3(260, 0, 180)]:
		_add_barrel(parent, site_a + offset)
		_add_barrel(parent, site_b - offset)


static func _add_sign(parent: Node3D, text: String, position: Vector3) -> void:
	var label := Label3D.new()
	label.text = text
	label.font_size = 160
	label.modulate = SIGN_COLOR
	label.outline_size = 18
	label.outline_modulate = Color(0.12, 0.08, 0.04)
	label.position = position
	label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	label.no_depth_test = true
	parent.add_child(label)
	var post := MeshInstance3D.new()
	var mesh := BoxMesh.new()
	mesh.size = Vector3(16, 240, 16)
	post.mesh = mesh
	post.material_override = _flat(TRUNK_COLOR)
	post.position = position - Vector3(0, 120, 0)
	parent.add_child(post)


static func _add_palm(parent: Node3D, base: Vector3) -> void:
	var trunk := MeshInstance3D.new()
	var trunk_mesh := CylinderMesh.new()
	trunk_mesh.top_radius = 8.0
	trunk_mesh.bottom_radius = 12.0
	trunk_mesh.height = 210.0
	trunk.mesh = trunk_mesh
	trunk.material_override = _flat(TRUNK_COLOR)
	trunk.position = base + Vector3(0, 105, 0)
	parent.add_child(trunk)
	for index in range(6):
		var leaf := MeshInstance3D.new()
		var leaf_mesh := BoxMesh.new()
		leaf_mesh.size = Vector3(150, 6, 40)
		leaf.mesh = leaf_mesh
		leaf.material_override = _flat(LEAF_COLOR)
		leaf.position = base + Vector3(0, 210, 0)
		leaf.rotation_degrees = Vector3(0, index * 60.0, -18.0)
		parent.add_child(leaf)


static func _add_barrel(parent: Node3D, base: Vector3) -> void:
	var barrel := MeshInstance3D.new()
	var mesh := CylinderMesh.new()
	mesh.top_radius = 26.0
	mesh.bottom_radius = 26.0
	mesh.height = 70.0
	barrel.mesh = mesh
	barrel.material_override = _flat(Color(0.30, 0.38, 0.42))
	barrel.position = base + Vector3(0, 35, 0)
	parent.add_child(barrel)


static func _flat(color: Color) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = 0.85
	return material
