extends SceneTree
## 通用地图机位截图 + Trace 契约（窗口化 GPU CLI，非编辑器）。
## 用法：godot --path host --script res://tests/capture_map.gd -- --map=de_dust2 --out=<dir>
## 产出：<map>_<view>.png（7 机位）与 trace.json（盒子哈希/三角面/材质/相机位姿）。

const MapLoader = preload("res://scripts/map_loader.gd")
const VIEWS := {
	"overview_top": [Vector3(0, 3400, 0), Vector3(0, 0, 0), false],
	"A_site": [Vector3(-1575, 420, -780), Vector3(-1575, 128, -1500), true],
	"B_site": [Vector3(-1790, 420, 950), Vector3(-1790, 64, 1550), true],
	"mid": [Vector3(-700, 220, -320), Vector3(800, 60, -320), true],
	"T_spawn": [Vector3(1835, 210, 850), Vector3(1200, 60, 300), true],
	"CT_spawn": [Vector3(-1420, 210, -800), Vector3(-500, 60, -200), true],
	"long": [Vector3(1500, 210, -1160), Vector3(-1400, 60, -1160), true],
}
var map_id := "de_dust2"
var out_dir := ""
var poses: Array = []


func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		var text := str(arg)
		if text.begins_with("--map="):
			map_id = text.trim_prefix("--map=")
		elif text.begins_with("--out="):
			out_dir = text.trim_prefix("--out=")
	if out_dir.is_empty():
		out_dir = ProjectSettings.globalize_path("res://captures/%s" % map_id)
	DirAccess.make_dir_recursive_absolute(out_dir)
	_run.call_deferred()


func _run() -> void:
	var boxes_path := MapLoader.boxes_path_for(map_id)
	var glb_path := MapLoader.glb_path_for(map_id)
	if boxes_path.is_empty() or glb_path.is_empty():
		push_error("地图 %s 未注册 boxes/glb" % map_id)
		quit(2)
		return
	var root3d := Node3D.new()
	root.add_child(root3d)
	_build_environment(root3d)
	MaterialLib.use_texture_set(map_id)
	var scene := _load_glb(glb_path)
	if scene == null:
		push_error("地图 %s GLB 解析失败：%s" % [map_id, glb_path])
		quit(2)
		return
	root3d.add_child(scene)
	var cam := Camera3D.new()
	cam.fov = 75.0
	cam.near = 1.0
	cam.far = 20000.0
	root3d.add_child(cam)
	await process_frame
	var stats := _scene_stats(scene)
	for view_name: String in VIEWS:
		var view: Array = VIEWS[view_name]
		cam.global_position = view[0]
		cam.look_at(view[1], Vector3.UP if bool(view[2]) else Vector3(0, 0, -1))
		await process_frame
		await process_frame
		await RenderingServer.frame_post_draw
		var image: Image = root.get_texture().get_image()
		var filename := "%s_%s.png" % [map_id, view_name]
		image.save_png(out_dir.path_join(filename))
		poses.append({
			"camera": view_name,
			"pos": [view[0].x, view[0].y, view[0].z],
			"yaw": rad_to_deg(cam.rotation.y),
			"pitch": rad_to_deg(cam.rotation.x),
			"file": filename,
		})
		print("MAP_CAPTURE ", filename, " ", image.get_width(), "x", image.get_height())
	_write_trace(boxes_path, stats)
	print("MAP_TRACE ", out_dir.path_join("trace.json"))
	quit(0)


func _write_trace(boxes_path: String, stats: Dictionary) -> void:
	var trace := {
		"map": map_id,
		"boxes_path": boxes_path,
		"boxes_sha256": _sha256_file(ProjectSettings.globalize_path(boxes_path)),
		"tri_count": stats["tri_count"],
		"materials": stats["materials"],
		"views": poses,
	}
	var file := FileAccess.open(out_dir.path_join("trace.json"), FileAccess.WRITE)
	file.store_string(JSON.stringify(trace, "\t"))
	file.close()


func _scene_stats(scene: Node3D) -> Dictionary:
	var materials: Dictionary = {}
	var tris := _collect_stats(scene, materials)
	return {"tri_count": tris, "materials": materials.size()}


func _collect_stats(node: Node, materials: Dictionary) -> int:
	var tris := 0
	if node is MeshInstance3D:
		var mesh: Mesh = (node as MeshInstance3D).mesh
		if mesh != null:
			tris += mesh.get_faces().size() / 3
			for surface in mesh.get_surface_count():
				var material := mesh.surface_get_material(surface)
				if material != null:
					materials[material.get_instance_id()] = true
	for child in node.get_children():
		tris += _collect_stats(child, materials)
	return tris


func _sha256_file(path: String) -> String:
	var bytes := FileAccess.get_file_as_bytes(path)
	if bytes.is_empty():
		return ""
	var context := HashingContext.new()
	context.start(HashingContext.HASH_SHA256)
	context.update(bytes)
	return context.finish().hex_encode()


func _load_glb(path: String) -> Node3D:
	if not FileAccess.file_exists(path):
		return null
	var document := GLTFDocument.new()
	var state := GLTFState.new()
	if document.append_from_file(ProjectSettings.globalize_path(path), state) != OK:
		return null
	var scene := document.generate_scene(state)
	MaterialLib.apply_to_scene(scene)
	return scene as Node3D


func _build_environment(root3d: Node3D) -> void:
	var env := Environment.new()
	env.background_mode = Environment.BG_SKY
	var sky := Sky.new()
	var mat := ProceduralSkyMaterial.new()
	mat.sky_top_color = Color(0.20, 0.27, 0.38)
	mat.sky_horizon_color = Color(0.66, 0.62, 0.54)
	sky.sky_material = mat
	env.sky = sky
	env.ambient_light_source = Environment.AMBIENT_SOURCE_SKY
	env.ambient_light_energy = 1.0
	env.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	var world_env := WorldEnvironment.new()
	world_env.environment = env
	root3d.add_child(world_env)
	var sun := DirectionalLight3D.new()
	sun.rotation_degrees = Vector3(-52, -35, 0)
	sun.light_energy = 1.9
	sun.shadow_enabled = true
	root3d.add_child(sun)
	var fill := DirectionalLight3D.new()
	fill.rotation_degrees = Vector3(-25, 140, 0)
	fill.light_energy = 0.45
	fill.shadow_enabled = false
	root3d.add_child(fill)
