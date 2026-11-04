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
