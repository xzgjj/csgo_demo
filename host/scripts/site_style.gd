class_name SiteStyle
extends RefCounted
## A/B 场景表现层：共享 kit 只提供定义；两区各自 tint 与灯光，独立但不改碰撞。
## 数据源：content/defs/site_kits.json（与 CSV/boxes 同一 map 口径）。

const DEFAULT_RELATIVE := "../content/defs/site_kits.json"


static func default_path() -> String:
	var root := ProjectSettings.globalize_path("res://")
	return root.path_join(DEFAULT_RELATIVE).simplify_path()


static func load_config(path: String = "") -> Dictionary:
	var target := path if not path.is_empty() else default_path()
	if not FileAccess.file_exists(target):
		push_warning("site_kits 缺失：%s" % target)
		return {}
	var parsed: Variant = JSON.parse_string(FileAccess.get_file_as_string(target))
	return parsed if typeof(parsed) == TYPE_DICTIONARY else {}


## 只对 de_dust2 生效；其它图保留原表现，避免旧 golden 漂移。
static func apply(world: Node3D, visual: Node3D, map_id: String,
		site_anchors: Array[Vector3]) -> void:
	if map_id != "de_dust2" or visual == null:
		return
	var config := load_config()
	if config.is_empty():
		return
	var sites: Dictionary = config.get("sites", {})
	for site_id in ["A", "B"]:
		var site: Dictionary = sites.get(site_id, {})
		if site.is_empty():
			continue
		var tinted := _tint_site(visual, site)
		_add_lights(world, site, site_id)
		print("SITE_STYLE %s tinted=%d lights=%d" % [site_id, tinted,
				site.get("lights", []).size()])


static func _tint_site(visual: Node3D, site: Dictionary) -> int:
	var tint: Dictionary = site.get("tint", {})
	if tint.is_empty():
		return 0
	var rect: Array = site.get("rect", [])
	if rect.size() < 4:
		return 0
	var strength := clampf(float(site.get("tint_strength", 0.55)), 0.0, 1.0)
	var emission := clampf(float(site.get("tint_emission", 0.0)), 0.0, 0.3)
	var tinted := 0
	var meshes := visual.find_children("*", "MeshInstance3D", true, false)
	for node in meshes:
		var mesh := node as MeshInstance3D
		var gp := mesh.global_position
		# Godot (x,y,z) → sim (x=-z, y=-x)
		var sim_x := -gp.z
		var sim_y := -gp.x
		if sim_x < float(rect[0]) or sim_x > float(rect[2]) or sim_y < float(rect[1]) or sim_y > float(rect[3]):
			continue
		var source := mesh.get_active_material(0)
		if source is not StandardMaterial3D:
			continue
		var color_key := _surface_key(mesh)
		var target := Color(str(tint.get(color_key, "#FFFFFF")))
		var copy := (source as StandardMaterial3D).duplicate() as StandardMaterial3D
		copy.albedo_color = (source as StandardMaterial3D).albedo_color.lerp(target, strength)
		if emission > 0.0:
			copy.emission_enabled = true
			copy.emission = target
			copy.emission_energy_multiplier = emission
		mesh.material_override = copy
		tinted += 1
	return tinted


## 材质名形如 mat_concrete；按主材质给 tint 键，未知按 concrete。
static func _surface_key(mesh: MeshInstance3D) -> String:
	var material := mesh.get_active_material(0)
	if material == null:
		return "concrete"
	var res_name := str(material.resource_name)
	for key in ["concrete", "stone", "wood", "metal", "fabric", "glass", "dirt"]:
		if res_name.contains(key):
			return key
	return "concrete"


static func _add_lights(world: Node3D, site: Dictionary, site_id: String) -> void:
	for entry: Dictionary in site.get("lights", []):
		var sim: Array = entry.get("sim", [])
		if sim.size() < 3:
			continue
		var light := OmniLight3D.new()
		light.name = str(entry.get("name", "%s_Light" % site_id))
		light.position = Vector3(-float(sim[1]), float(sim[2]), -float(sim[0]))
		light.light_color = Color(str(entry.get("color", "#FFFFFF")))
		light.light_energy = float(entry.get("energy", 1.0))
		light.omni_range = float(entry.get("range", 400.0))
		light.shadow_enabled = bool(entry.get("shadow", false))
		world.add_child(light)


static func _count_lights(world: Node3D, prefix: String) -> int:
	var count := 0
	for child in world.get_children():
		if child is OmniLight3D and str(child.name).begins_with(prefix):
			count += 1
	return count
