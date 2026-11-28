class_name ActorVisuals
extends RefCounted
## 角色/烟雾等表现层节点的工厂：只建 Mesh，不读内核状态。


static func make_capsule(parent: Node3D, color: Color, actor_name: String = "Actor") -> MeshInstance3D:
	var mesh_instance := MeshInstance3D.new()
	var capsule := CapsuleMesh.new()
	capsule.radius = 16.0
	capsule.height = 72.0
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	capsule.material = material
	mesh_instance.mesh = capsule
	mesh_instance.name = actor_name
	parent.add_child(mesh_instance)
	return mesh_instance


## 第三人称下同步玩家胶囊：胶囊原点在几何中心，脚底 z 上抬半身高 36。
static func sync_player(mesh_instance: Node3D, sim: RefCounted) -> void:
	mesh_instance.position = Vector3(-sim.get_y(0), float(sim.get_z(0)) + 36.0,
			-sim.get_x(0))


static func make_smoke(parent: Node3D, radius: float = 144.0) -> MeshInstance3D:
	var mesh_instance := MeshInstance3D.new()
	var sphere := SphereMesh.new()
	sphere.radius = radius
	sphere.height = radius * 2.0
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(0.75, 0.78, 0.82, 0.85)
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	sphere.material = material
	mesh_instance.mesh = sphere
	mesh_instance.visible = false
	parent.add_child(mesh_instance)
	return mesh_instance


static func update_bot_meshes(parent: Node3D, sim: RefCounted,
		meshes: Array[Node3D]) -> void:
	var count: int = int(sim.get_player_count())
	for index in range(1, count):
		while meshes.size() < index:
			var team: int = int(sim.get_team(meshes.size() + 1))
			meshes.append(make_humanoid_bot(parent, meshes.size() + 1, team))
		var mesh: Node3D = meshes[index - 1]
		var alive: bool = sim.is_alive(index)
		mesh.visible = alive
		if alive:
			# 类人低模的脚底在原点，直接贴内核 z；写死 0 会让平台/台阶上的单位陷进地里。
			mesh.position = Vector3(-float(sim.get_y(index)), float(sim.get_z(index)), -float(sim.get_x(index)))


## 类人低模：T 红装、CT 黑色复杂轮廓（肩甲 + 红色目镜），替代“红色药丸”胶囊。
static func make_humanoid_bot(parent: Node3D, slot: int, team: int) -> Node3D:
	var root := Node3D.new()
	root.name = "Bot%d" % slot
	var body_color := Color(0.55, 0.16, 0.14) if team == 1 else Color(0.06, 0.06, 0.07)
	var head_color := Color(0.82, 0.68, 0.55) if team == 1 else Color(0.10, 0.10, 0.11)
	_humanoid_part(root, Vector3(40, 66, 26), Vector3(0, 40, 0), body_color)
	_humanoid_part(root, Vector3(30, 20, 24), Vector3(0, 13, 0), Color(0.20, 0.18, 0.16))
	_humanoid_part(root, Vector3(24, 24, 24), Vector3(0, 84, 0), head_color)
	_humanoid_part(root, Vector3(34, 10, 28), Vector3(0, 94, 0), Color(0.10, 0.10, 0.11))
	_humanoid_part(root, Vector3(12, 38, 12), Vector3(-26, 42, 0), body_color)
	_humanoid_part(root, Vector3(12, 38, 12), Vector3(26, 42, 0), body_color)
	_humanoid_part(root, Vector3(14, 40, 14), Vector3(-11, -8, 0), Color(0.14, 0.14, 0.15))
	_humanoid_part(root, Vector3(14, 40, 14), Vector3(11, -8, 0), Color(0.14, 0.14, 0.15))
	if team == 2:
		_humanoid_part(root, Vector3(40, 8, 30), Vector3(0, 70, 0), Color(0.25, 0.04, 0.04))
		_humanoid_part(root, Vector3(26, 6, 4), Vector3(0, 86, 12), Color(0.95, 0.15, 0.10))
	parent.add_child(root)
	return root


static func _humanoid_part(parent: Node3D, size: Vector3, offset: Vector3, color: Color) -> void:
	var mesh_instance := MeshInstance3D.new()
	var box := BoxMesh.new()
	box.size = size
	box.material = _flat_material(color)
	mesh_instance.mesh = box
	mesh_instance.position = offset
	parent.add_child(mesh_instance)


static func _flat_material(color: Color) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = 0.9
	return material
