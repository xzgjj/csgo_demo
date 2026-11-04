extends Node3D
## Project Recoil demo 宿主：输入 → Command，内核 tick → 只读状态 → 相机 / HUD / 页面。
## 页面流：主菜单 → 对局 →（Esc）暂停 → 设置；回合/比赛结束自动弹结算；Tab 比分板。
## run_scenario.gd / capture.gd 依赖 sim、_start_play()、_update_view()、_update_hud()，签名不变。

const EYE_HEIGHT := 64.0
const TICKS_PER_SECOND := 64.0

var sim: RefCounted
var world: DeAshWorld
var settings: SettingsStore
var hud: GameHud
var menu: MainMenuPage
var pause_page: PausePage
var settings_page: SettingsPage
var result_page: ResultPage
var scoreboard: ScoreboardPanel
var camera: Camera3D
var target_mesh: MeshInstance3D
var smoke_mesh: MeshInstance3D
var viewmodel: WeaponViewmodel
var blind_overlay: ColorRect
var yaw := 0.0
var pitch := 0.0
var playing := false
var paused := false
var _settings_return_pause := false
var _last_phase := -1
var _last_score_t := 0
var _last_score_ct := 0
var _fire_was_down := false
var _buy_was_down := false
var _bomb_was_down := false
var _weapon_key_was := -1
var _throw_was := {"he": false, "flash": false, "smoke": false}


func _ready() -> void:
	sim = RecoilSim.new()
	sim.reset()
	settings = SettingsStore.new()
	settings.load_settings()
	_build_world()
	_build_ui()
	_apply_settings()
	_show_menu()


func _build_world() -> void:
	world = DeAshWorld.new()
	world.name = "World"
	add_child(world)
	world.build()
	target_mesh = _make_target_mesh()
	smoke_mesh = _make_smoke_mesh()
	camera = Camera3D.new()
	camera.fov = settings.fov
	camera.near = 0.05
	camera.far = 4000.0
	add_child(camera)
	viewmodel = WeaponViewmodel.new()
	add_child(viewmodel)
	viewmodel.attach(camera)


func _build_ui() -> void:
	hud = GameHud.new()
	add_child(hud)
	hud.build()
	menu = MainMenuPage.new()
	add_child(menu)
	menu.build()
	menu.start_button.pressed.connect(_start_play)
	menu.settings_button.pressed.connect(show_settings_page)
	menu.quit_button.pressed.connect(_quit)
	pause_page = PausePage.new()
	add_child(pause_page)
	pause_page.build()
	pause_page.resume_button.pressed.connect(_resume)
	pause_page.settings_button.pressed.connect(show_settings_page)
	pause_page.menu_button.pressed.connect(_show_menu)
	settings_page = SettingsPage.new()
	add_child(settings_page)
	settings_page.build(settings)
	settings_page.changed.connect(_apply_settings)
	settings_page.back_button.pressed.connect(close_settings_page)
	result_page = ResultPage.new()
	add_child(result_page)
	result_page.build()
	result_page.restart_button.pressed.connect(_start_play)
	result_page.menu_button.pressed.connect(_show_menu)
	scoreboard = ScoreboardPanel.new()
	add_child(scoreboard)
	scoreboard.build()
	blind_overlay = ColorRect.new()
	blind_overlay.color = Color(1.0, 1.0, 1.0, 0.0)
	blind_overlay.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	blind_overlay.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var overlay_layer := CanvasLayer.new()
	overlay_layer.name = "BlindOverlay"
	overlay_layer.layer = 1
	add_child(overlay_layer)
	overlay_layer.add_child(blind_overlay)


func _make_target_mesh() -> MeshInstance3D:
	var mesh_instance := MeshInstance3D.new()
	var capsule := CapsuleMesh.new()
	capsule.radius = 16.0
	capsule.height = 72.0
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(0.75, 0.20, 0.20)
	capsule.material = material
	mesh_instance.mesh = capsule
	add_child(mesh_instance)
	return mesh_instance


func _make_smoke_mesh() -> MeshInstance3D:
	var mesh_instance := MeshInstance3D.new()
	var sphere := SphereMesh.new()
	sphere.radius = 144.0
	sphere.height = 288.0
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(0.75, 0.78, 0.82, 0.85)
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	sphere.material = material
	mesh_instance.mesh = sphere
	mesh_instance.visible = false
	add_child(mesh_instance)
	return mesh_instance


func _show_menu() -> void:
	playing = false
	paused = false
	hud.visible = false
	menu.visible = true
	pause_page.visible = false
	settings_page.visible = false
	result_page.hide_result()
	scoreboard.visible = false
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE


func _start_play() -> void:
	sim.reset()
	playing = true
	paused = false
	_last_phase = -1
	_last_score_t = 0
	_last_score_ct = 0
	yaw = 0.0
	pitch = 0.0
	hud.visible = true
	hud.reset_feed()
	menu.visible = false
	pause_page.visible = false
	settings_page.visible = false
	result_page.hide_result()
	scoreboard.visible = false
	Input.mouse_mode = Input.MOUSE_MODE_CAPTURED


func _show_pause() -> void:
	if not playing:
		return
	playing = false
	paused = true
	pause_page.visible = true
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE


func _resume() -> void:
	if not paused:
		return
	playing = true
	paused = false
	pause_page.visible = false
	settings_page.visible = false
	Input.mouse_mode = Input.MOUSE_MODE_CAPTURED


func show_settings_page() -> void:
	_settings_return_pause = paused
	menu.visible = false
	pause_page.visible = false
	settings_page.sync_from_store()
	settings_page.visible = true
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE


func close_settings_page() -> void:
	settings_page.save_settings()
	settings_page.visible = false
	_apply_settings()
	pause_page.visible = _settings_return_pause
	menu.visible = not _settings_return_pause
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE


func toggle_scoreboard() -> void:
	if not (playing or paused):
		return
	scoreboard.visible = not scoreboard.visible
	if scoreboard.visible:
		scoreboard.refresh(sim)


func _apply_settings() -> void:
	if camera != null:
		camera.fov = settings.fov
	settings.apply_audio()


func _quit() -> void:
	get_tree().quit()


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseMotion and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		yaw -= event.relative.x * settings.sensitivity
		pitch = clampf(pitch - event.relative.y * settings.sensitivity, -89.0, 89.0)
	if event.is_action_pressed("pause"):
		_handle_pause_action()
	if event is InputEventKey and event.pressed and not event.echo and event.keycode == KEY_TAB:
		toggle_scoreboard()


func _handle_pause_action() -> void:
	if settings_page.visible:
		close_settings_page()
	elif result_page.visible and result_page.mode == "match":
		_show_menu()
	elif playing:
		_show_pause()
	elif paused:
		_resume()


func _physics_process(_delta: float) -> void:
	if not playing:
		return
	var forward := Input.get_axis("move_back", "move_forward")
	var side := Input.get_axis("move_right", "move_left")
	sim.set_move(0, yaw, pitch, forward, side, Input.is_action_pressed("jump"),
			Input.is_action_pressed("duck"))
	var fire_down := Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT)
	if fire_down and not _fire_was_down:
		sim.fire(0, yaw, pitch)
		viewmodel.kick()
	_fire_was_down = fire_down
	var weapon_key := _pressed_weapon_key()
	if weapon_key != -1 and weapon_key != _weapon_key_was:
		_switch_weapon(weapon_key)
	_weapon_key_was = weapon_key
	if Input.is_action_just_pressed("reload"):
		sim.reload(0)
	var buy_down := Input.is_key_pressed(KEY_B)
	if buy_down and not _buy_was_down:
		sim.buy_armor(0)
	_buy_was_down = buy_down
	var bomb_down := Input.is_key_pressed(KEY_E)
	if bomb_down and not _bomb_was_down:
		sim.plant_bomb(0)
	_bomb_was_down = bomb_down
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
	blind_overlay.color = Color(1.0, 1.0, 1.0,
			clampf(float(sim.get_blind_ticks(0)) / 128.0, 0.0, 0.85))
	_update_round_pages()
	if scoreboard.visible:
		scoreboard.refresh(sim)


func _update_round_pages() -> void:
	var phase: int = sim.get_round_phase()
	if phase != _last_phase:
		_handle_phase_change(_last_phase, phase)
		_last_phase = phase
	_last_score_t = sim.get_score_t()
	_last_score_ct = sim.get_score_ct()
	if result_page.visible and result_page.mode == "round":
		result_page.update_countdown(float(sim.get_round_ticks()) / TICKS_PER_SECOND)


func _handle_phase_change(old_phase: int, new_phase: int) -> void:
	if new_phase == 2:
		result_page.show_round_result(_round_winner(), sim.get_score_t(), sim.get_score_ct(),
				sim.get_kills(0), sim.get_money(0),
				float(sim.get_round_ticks()) / TICKS_PER_SECOND)
	elif new_phase == 3:
		_enter_match_end()
	elif new_phase == 0 and old_phase >= 2:
		result_page.hide_result()


func _round_winner() -> String:
	if sim.get_score_t() > _last_score_t:
		return "T"
	if sim.get_score_ct() > _last_score_ct:
		return "CT"
	return "—"


func _enter_match_end() -> void:
	playing = false
	paused = false
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	var winner := "T" if sim.get_score_t() > sim.get_score_ct() else "CT"
	result_page.show_match_result(winner, sim.get_score_t(), sim.get_score_ct(),
			sim.get_kills(0), sim.get_money(0), sim.get_score_t() + sim.get_score_ct())


func _update_hud() -> void:
	var weapon_name := "-"
	if viewmodel != null:
		weapon_name = viewmodel.display_name()
	hud.refresh(sim, weapon_name)


func _pressed_weapon_key() -> int:
	if Input.is_key_pressed(KEY_1):
		return 0
	if Input.is_key_pressed(KEY_2):
		return 1
	if Input.is_key_pressed(KEY_3):
		return 2
	return -1


func _switch_weapon(index: int) -> void:
	if viewmodel == null:
		return
	var ids := viewmodel.weapon_ids()
	if index < ids.size():
		viewmodel.select_weapon(ids[index])


func _throw_input(name: String, key: int, kind: int) -> void:
	var down := Input.is_key_pressed(key)
	if down and not _throw_was[name]:
		sim.throw_grenade(0, kind, yaw, pitch)
	_throw_was[name] = down
