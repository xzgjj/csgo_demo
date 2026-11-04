extends Node3D
## Project Recoil 灰盒 demo：C++ 内核（GDExtension）驱动，Godot 只做表现与输入。
## 输入 → Command，内核 tick → 只读状态 → 相机/靶子/HUD。

const ARENA_HALF := 800.0
const WALL_THICKNESS := 32.0
const WALL_HEIGHT := 200.0
const EYE_HEIGHT := 64.0
const SENSITIVITY := 0.08

var sim: RefCounted
var yaw := 0.0
var pitch := 0.0
var camera: Camera3D
var target_mesh: MeshInstance3D
var smoke_mesh: MeshInstance3D
var blind_overlay: ColorRect
var throw_was := {"he": false, "flash": false, "smoke": false}
var hud_label: Label
var menu_layer: CanvasLayer
var pause_layer: CanvasLayer
var settings_layer: CanvasLayer
var playing := false
var fire_was_down := false
var buy_was_down := false
var last_hits := 0
var last_shots := 0
var last_dry := 0

func _ready() -> void:
	sim = RecoilSim.new()
	sim.reset()
	_build_world()
	_build_ui()
	_show_menu()

func _build_world() -> void:
	var env := Environment.new()
	env.background_mode = Environment.BG_SKY
	var sky := Sky.new()
	var sky_mat := ProceduralSkyMaterial.new()
	sky_mat.sky_top_color = Color(0.15, 0.2, 0.3)
	sky_mat.sky_horizon_color = Color(0.55, 0.6, 0.68)
	sky_mat.ground_bottom_color = Color(0.08, 0.09, 0.1)
	sky_mat.ground_horizon_color = Color(0.3, 0.32, 0.35)
	sky.sky_material = sky_mat
	env.sky = sky
	env.ambient_light_source = Environment.AMBIENT_SOURCE_SKY
	env.ambient_light_energy = 0.6
	env.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	var world_env := WorldEnvironment.new()
	world_env.environment = env
	add_child(world_env)

	var sun := DirectionalLight3D.new()
	sun.rotation_degrees = Vector3(-50, -35, 0)
	sun.light_energy = 1.6
	sun.shadow_enabled = true
	add_child(sun)

	_add_box(Vector3(0, -10, 0), Vector3(ARENA_HALF * 2.0, 20.0, ARENA_HALF * 2.0), Color(0.32, 0.33, 0.35))
	# 与 GDExtension 中 MapDef 的四周边界墙一一对应（sim 坐标 → Godot 坐标：(-y, z, -x)）。
	_add_box(Vector3(0, WALL_HEIGHT * 0.5, -ARENA_HALF - WALL_THICKNESS * 0.5),
			Vector3(ARENA_HALF * 2.0, WALL_HEIGHT, WALL_THICKNESS), Color(0.45, 0.42, 0.38))
	_add_box(Vector3(0, WALL_HEIGHT * 0.5, ARENA_HALF + WALL_THICKNESS * 0.5),
			Vector3(ARENA_HALF * 2.0, WALL_HEIGHT, WALL_THICKNESS), Color(0.45, 0.42, 0.38))
	_add_box(Vector3(-ARENA_HALF - WALL_THICKNESS * 0.5, WALL_HEIGHT * 0.5, 0),
			Vector3(WALL_THICKNESS, WALL_HEIGHT, ARENA_HALF * 2.0), Color(0.42, 0.4, 0.36))
	_add_box(Vector3(ARENA_HALF + WALL_THICKNESS * 0.5, WALL_HEIGHT * 0.5, 0),
			Vector3(WALL_THICKNESS, WALL_HEIGHT, ARENA_HALF * 2.0), Color(0.42, 0.4, 0.36))

	# 几个箱子（仅表现，内核暂无对应碰撞体）。
	_add_box(Vector3(-180, 40, -260), Vector3(120, 80, 120), Color(0.5, 0.36, 0.22))
	_add_box(Vector3(260, 40, 180), Vector3(120, 80, 120), Color(0.5, 0.36, 0.22))

	target_mesh = MeshInstance3D.new()
	var capsule := CapsuleMesh.new()
	capsule.radius = 16.0
	capsule.height = 72.0
	var target_mat := StandardMaterial3D.new()
	target_mat.albedo_color = Color(0.75, 0.2, 0.2)
	capsule.material = target_mat
	target_mesh.mesh = capsule
	add_child(target_mesh)

	smoke_mesh = MeshInstance3D.new()
	var smoke_sphere := SphereMesh.new()
	smoke_sphere.radius = 144.0
	smoke_sphere.height = 288.0
	var smoke_mat := StandardMaterial3D.new()
	smoke_mat.albedo_color = Color(0.75, 0.78, 0.82, 0.85)
	smoke_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	smoke_sphere.material = smoke_mat
	smoke_mesh.mesh = smoke_sphere
	smoke_mesh.visible = false
	add_child(smoke_mesh)

	camera = Camera3D.new()
	camera.fov = 90.0
	camera.near = 0.05
	camera.far = 4000.0
	add_child(camera)

func _add_box(pos: Vector3, size: Vector3, color: Color) -> void:
	var mesh_instance := MeshInstance3D.new()
	var box := BoxMesh.new()
	box.size = size
	var mat := StandardMaterial3D.new()
	mat.albedo_color = color
	box.material = mat
	mesh_instance.mesh = box
	mesh_instance.position = pos
	add_child(mesh_instance)

func _build_ui() -> void:
	var hud := CanvasLayer.new()
	add_child(hud)
	hud_label = Label.new()
	hud_label.position = Vector2(20, 16)
	hud_label.add_theme_font_size_override("font_size", 20)
	hud_label.add_theme_color_override("font_color", Color(0.95, 0.97, 1.0))
	hud.add_child(hud_label)

	var crosshair := Label.new()
	crosshair.text = "+"
	crosshair.add_theme_font_size_override("font_size", 26)
	crosshair.set_anchors_preset(Control.PRESET_CENTER)
	crosshair.position = Vector2(632, 344)
	hud.add_child(crosshair)

	blind_overlay = ColorRect.new()
	blind_overlay.color = Color(1, 1, 1, 0)
	blind_overlay.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	blind_overlay.mouse_filter = Control.MOUSE_FILTER_IGNORE
	hud.add_child(blind_overlay)

	menu_layer = _make_menu("PROJECT RECOIL — GRAYBOX DEMO",
			["开始游戏", "设置", "退出"], [_start_play, _show_settings, _quit])

	pause_layer = _make_menu("已暂停", ["继续", "设置", "返回主菜单"], [_resume, _show_settings, _show_menu])
	pause_layer.visible = false

	settings_layer = _make_menu("设置（占位）", ["返回"], [_hide_settings])
	settings_layer.visible = false

func _make_menu(title: String, buttons: Array, callbacks: Array) -> CanvasLayer:
	var layer := CanvasLayer.new()
	add_child(layer)
	var panel := PanelContainer.new()
	panel.set_anchors_preset(Control.PRESET_CENTER)
	panel.custom_minimum_size = Vector2(420, 320)
	var box := VBoxContainer.new()
	box.add_theme_constant_override("separation", 12)
	panel.add_child(box)
	var label := Label.new()
	label.text = title
	label.add_theme_font_size_override("font_size", 26)
	box.add_child(label)
	for i in buttons.size():
		var button := Button.new()
		button.text = buttons[i]
		button.custom_minimum_size = Vector2(360, 44)
		button.pressed.connect(callbacks[i])
		box.add_child(button)
	layer.add_child(panel)
	return layer

func _show_menu() -> void:
	playing = false
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	menu_layer.visible = true
	pause_layer.visible = false
	settings_layer.visible = false

func _start_play() -> void:
	playing = true
	menu_layer.visible = false
	pause_layer.visible = false
	settings_layer.visible = false
	Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
	yaw = 0.0
	pitch = 0.0
	sim.reset()

func _resume() -> void:
	playing = true
	pause_layer.visible = false
	Input.mouse_mode = Input.MOUSE_MODE_CAPTURED

func _show_pause() -> void:
	playing = false
	pause_layer.visible = true
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE

func _show_settings() -> void:
	settings_layer.visible = true

func _hide_settings() -> void:
	settings_layer.visible = false

func _quit() -> void:
	get_tree().quit()

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseMotion and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		yaw -= event.relative.x * SENSITIVITY
		pitch = clampf(pitch - event.relative.y * SENSITIVITY, -89.0, 89.0)
	if event.is_action_pressed("pause"):
		if playing:
			_show_pause()
		elif pause_layer.visible:
			_resume()

func _physics_process(_delta: float) -> void:
	if not playing:
		return
	var forward := Input.get_axis("move_back", "move_forward")
	var side := Input.get_axis("move_right", "move_left")
	var jump := Input.is_action_pressed("jump")
	var duck := Input.is_action_pressed("duck")
	sim.set_move(0, yaw, pitch, forward, side, jump, duck)
	var fire_down := Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT)
	if fire_down and not fire_was_down:
		sim.fire(0, yaw, pitch)
	fire_was_down = fire_down
	if Input.is_action_just_pressed("reload"):
		sim.reload(0)
	var buy_down := Input.is_key_pressed(KEY_B)
	if buy_down and not buy_was_down:
		sim.buy_armor(0)
	buy_was_down = buy_down
	_throw_input("he", KEY_G, 0)
	_throw_input("flash", KEY_F, 1)
	_throw_input("smoke", KEY_V, 2)
	sim.tick()
	_update_view()
	_update_hud()

func _update_view() -> void:
	var px: float = sim.get_x(0)
	var py: float = sim.get_y(0)
	var pz: float = sim.get_z(0)
	camera.position = Vector3(-py, pz + EYE_HEIGHT, -px)
	camera.rotation_degrees = Vector3(pitch, yaw, 0)
	var tx: float = sim.get_x(1)
	var ty: float = sim.get_y(1)
	target_mesh.position = Vector3(-ty, 36.0, -tx)
	target_mesh.visible = sim.is_alive(1)
	var smoke_on: bool = sim.get_smoke_active() != 0
	smoke_mesh.visible = smoke_on
	if smoke_on:
		smoke_mesh.position = Vector3(-sim.get_smoke_y(), 144.0, -sim.get_smoke_x())
	blind_overlay.color = Color(1, 1, 1, clampf(float(sim.get_blind_ticks(0)) / 128.0, 0.0, 0.85))

func _update_hud() -> void:
	last_hits = sim.get_total_damage_events()
	last_shots = sim.get_total_fire_events()
	last_dry = sim.get_total_dryfire_events()
	hud_label.text = "HP %d  ARMOR %d   AMMO %d/%d   TARGET HP %d   R%d %s  $%d  %d:%d   shots %d  hits %d  dry %d" % [
		sim.get_health(0), sim.get_armor(0), sim.get_mag(0), sim.get_reserve(0),
		sim.get_health(1), 0, _phase_text(), sim.get_money(0),
		sim.get_score_t(), sim.get_score_ct(), sim.get_smoke_active(), sim.get_blind_ticks(0),
		last_shots, last_hits, last_dry,
	]

func _throw_input(name: String, key: int, kind: int) -> void:
	var down := Input.is_key_pressed(key)
	if down and not throw_was[name]:
		sim.throw_grenade(0, kind, yaw, pitch)
	throw_was[name] = down

func _phase_text() -> String:
	match sim.get_round_phase():
		0:
			return "FREEZE"
		1:
			return "LIVE"
		2:
			return "ROUND END"
		_:
			return "MATCH END"
