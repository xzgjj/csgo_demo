extends SceneTree
## 读取 de_ash_boxes.json 搭建 blockout，输出 6 个机位截图并退出。
## 用法：godot --path host --script res://tests/capture_de_ash.gd

func _initialize() -> void:
	var text := FileAccess.get_file_as_string("res://scenes/de_ash_boxes.json")
	if text.is_empty():
		push_error("缺少 host/scenes/de_ash_boxes.json，先跑 tools/mapgen/gen_blockout.py")
		quit(2)
		return
	var data: Variant = JSON.parse_string(text)
	if typeof(data) != TYPE_DICTIONARY:
		push_error("盒子清单 JSON 非法")
		quit(2)
		return
	var root3d := Node3D.new()
	root.add_child(root3d)
	_build_world(root3d)
	_build_boxes(root3d, data.get("boxes", []))
	var out := ProjectSettings.globalize_path("res://captures")
	DirAccess.make_dir_recursive_absolute(out)
	var views := {
		"overview_top": [Vector3(0, 3400, 0), Vector3(0, 0, 0)],
		"A_site": [Vector3(900, 500, 1200), Vector3(-1100, 0, -700)],
		"B_site": [Vector3(-900, 500, -1200), Vector3(1100, 0, 700)],
		"mid": [Vector3(0, 220, 900), Vector3(0, 0, -400)],
		"T_spawn": [Vector3(0, 200, 1750), Vector3(0, 0, 0)],
		"CT_spawn": [Vector3(0, 200, -1750), Vector3(0, 0, 0)],
	}
	var cam := Camera3D.new()
	cam.fov = 75.0
	cam.far = 6000.0
	root3d.add_child(cam)
	_capture.call_deferred(cam, out, views)

func _capture(cam: Camera3D, out: String, views: Dictionary) -> void:
	await process_frame
	for name in views:
		var view: Array = views[name]
		cam.global_position = view[0]
		cam.look_at(view[1], Vector3(0, 0, -1) if name == "overview_top" else Vector3.UP)
		await process_frame
		await RenderingServer.frame_post_draw
		var img: Image = root.get_texture().get_image()
		img.save_png(out.path_join("de_ash_%s.png" % name))
		print("CAPTURE ", name, " ", img.get_width(), "x", img.get_height())
	quit(0)

func _build_world(root3d: Node3D) -> void:
	var env := Environment.new()
	env.background_mode = Environment.BG_SKY
	var sky := Sky.new()
	var mat := ProceduralSkyMaterial.new()
	mat.sky_top_color = Color(0.16, 0.22, 0.32)
	mat.sky_horizon_color = Color(0.55, 0.60, 0.66)
	sky.sky_material = mat
	env.sky = sky
	env.ambient_light_source = Environment.AMBIENT_SOURCE_SKY
	env.ambient_light_energy = 0.65
	var world_env := WorldEnvironment.new()
	world_env.environment = env
	root3d.add_child(world_env)
	var sun := DirectionalLight3D.new()
	sun.rotation_degrees = Vector3(-55, -30, 0)
	sun.light_energy = 1.5
	sun.shadow_enabled = true
	root3d.add_child(sun)

func _build_boxes(root3d: Node3D, boxes: Array) -> void:
	for item: Dictionary in boxes:
		var mesh := MeshInstance3D.new()
		var box := BoxMesh.new()
		var size: Array = item["size"]
		box.size = Vector3(size[0], size[1], size[2])
		var mat := StandardMaterial3D.new()
		var color: Array = item["color"]
		mat.albedo_color = Color(color[0], color[1], color[2])
		box.material = mat
		mesh.mesh = box
		mesh.position = Vector3(item["center"][0], item["center"][1], item["center"][2])
		mesh.rotation.y = item.get("rot_y", 0.0)
		root3d.add_child(mesh)
