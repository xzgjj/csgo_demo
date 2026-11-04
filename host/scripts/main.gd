extends Node3D
## Project Recoil demo 宿主：输入 → Command，内核 tick → 只读状态 → 相机 / HUD / 页面。
## 页面流：主菜单 → 对局 →（Esc）暂停 → 设置；回合/比赛结束自动弹结算；Tab 比分板。
## run_scenario.gd / capture.gd 依赖 sim、_start_play()、_update_view()、_update_hud()，签名不变。

const EYE_HEIGHT := 64.0
const TICKS_PER_SECOND := 64.0
const CombatFxScript = preload("res://scripts/combat_fx.gd")

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
var fx
var blind_overlay: ColorRect
var yaw := 0.0
var pitch := 0.0
var playing := false
var paused := false
var match_mode := false
var bot_meshes: Array[MeshInstance3D] = []
var _settings_return_pause := false
var _last_phase := -1
var _last_score_t := 0
var _last_score_ct := 0
var _fire_was_down := false
var _buy_was_down := false
var _bomb_was_down := false
var _weapon_key_was := -1
var _throw_was := {"he": false, "flash": false, "smoke": false}
var aiming := false
var third_person := false
var debug_visible := false
var _player_mesh: MeshInstance3D
var _death_label: Label
var _debug_label: Label
var _f3_was := false
var _last_damage := 0
var _f5_was := false
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
	camera.far = 9000.0
	add_child(camera)
	viewmodel = WeaponViewmodel.new()
	add_child(viewmodel)
	viewmodel.attach(camera)
	fx = CombatFxScript.new()
	add_child(fx)
	_player_mesh = ActorVisuals.make_capsule(self, Color(0.35, 0.55, 0.90), "Player")
	_player_mesh.visible = false

func _build_ui() -> void:
	HostPages.build(self)
	_death_label = HudOverlays.make_death_label(hud)
	_debug_label = HudOverlays.make_debug_label(hud)

func _make_target_mesh() -> MeshInstance3D:
	return ActorVisuals.make_capsule(self, Color(0.75, 0.20, 0.20), "Target")

func _make_smoke_mesh() -> MeshInstance3D:
	return ActorVisuals.make_smoke(self)

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

## 场景/截图执行器用的 2 人灰盒（±800 竞技场）模式，保持既有断言与 golden 不变。
func _start_play() -> void:
	sim.reset()
	_enter_game(false)

## 可玩模式：de_ash 全尺寸 MapDef + 10 人 5v5（玩家 0 为人类 T，其余由内核战术层驱动）。
func _start_match() -> void:
	sim.reset()
	_configure_sim_map()
	sim.set_sites(-1200.0, -900.0, 1200.0, 900.0)
	sim.begin_match()
	_enter_game(true)

func _restart() -> void:
	if match_mode:
		_start_match()
	else:
		_start_play()

func _enter_game(is_match: bool) -> void:
	match_mode = is_match
	playing = true
	paused = false
	_last_phase = -1
	_last_score_t = 0
	_last_score_ct = 0
	_last_damage = 0
	yaw = 90.0 if is_match else 0.0
	pitch = 0.0
	hud.visible = true
	hud.reset_feed()
	menu.visible = false
	pause_page.visible = false
	settings_page.visible = false
	result_page.hide_result()
	scoreboard.visible = false
	Input.mouse_mode = Input.MOUSE_MODE_CAPTURED

## 读取派生盒子（Godot 坐标）→ sim AABB 传入内核：碰撞与画面用同一份数据。
func _configure_sim_map() -> void:
	var text := FileAccess.get_file_as_string("res://scenes/de_ash_boxes.json")
	if text.is_empty():
		push_warning("缺少 de_ash_boxes.json，保持默认竞技场")
		return
	var data: Variant = JSON.parse_string(text)
	if typeof(data) != TYPE_DICTIONARY:
		push_warning("de_ash_boxes.json 非法，保持默认竞技场")
		return
	var flat := PackedFloat32Array()
	for item: Dictionary in data.get("boxes", []):
		if not bool(item.get("collision", true)):
			continue
		var c: Array = item["center"]
		var s: Array = item["size"]
		var gx := float(c[0])
		var gy := float(c[1])
		var gz := float(c[2])
		var sx := float(s[0])
		var sy := float(s[1])
		var sz := float(s[2])
		flat.append_array(PackedFloat32Array([
			-gz - sz * 0.5, -gx - sx * 0.5, gy - sy * 0.5,
			-gz + sz * 0.5, -gx + sx * 0.5, gy + sy * 0.5,
		]))
	sim.set_map_boxes(flat)

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
	if Input.is_key_pressed(KEY_SHIFT):
		forward *= 0.5
		side *= 0.5
	sim.set_move(0, yaw, pitch, forward, side, Input.is_action_pressed("jump"),
			Input.is_action_pressed("duck"))
	aiming = Input.is_mouse_button_pressed(MOUSE_BUTTON_RIGHT)
	var aim_fov := 30.0 if viewmodel.current_id == "awp" else 50.0
	camera.fov = lerpf(camera.fov, aim_fov if aiming else settings.fov, 0.25)
	viewmodel.set_aiming(aiming)
	_toggle_debug()
	_toggle_third_person()
	var fire_down := Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT)
	if fire_down and not _fire_was_down:
		sim.fire(0, yaw, pitch)
		viewmodel.kick()
		fx.trigger_shot()
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
	if match_mode:
		sim.tick_auto()
	else:
		sim.tick()
	var damage: int = int(sim.get_total_damage_events())
	if damage > _last_damage:
		fx.trigger_hit(CombatFxScript.hit_point(sim, camera))
		_last_damage = damage
	_update_view()
	_update_hud()

func _toggle_debug() -> void:
	var down := Input.is_key_pressed(KEY_F3)
	if down and not _f3_was:
		debug_visible = not debug_visible
	_f3_was = down

func _toggle_third_person() -> void:
	var down := Input.is_key_pressed(KEY_F5)
	if down and not _f5_was:
		third_person = not third_person
	_f5_was = down

func _update_view() -> void:
	var px: float = sim.get_x(0)
	var py: float = sim.get_y(0)
	var pz: float = sim.get_z(0)
	var eye := Vector3(-py, pz + EYE_HEIGHT, -px)
	camera.rotation_degrees = Vector3(pitch, yaw, 0)
	camera.position = eye
	if third_person:
		camera.position = eye - camera.basis.z * 170.0 + Vector3(0.0, 55.0, 0.0)
	_player_mesh.visible = third_person and match_mode and sim.is_alive(0)
	if _player_mesh.visible:
		_player_mesh.position = Vector3(-py, 36.0, -px)
	var scoped := aiming and viewmodel.current_id == "awp"
	viewmodel.visible = not third_person and not scoped
	if match_mode:
		target_mesh.visible = false
		ActorVisuals.update_bot_meshes(self, sim, bot_meshes)
	else:
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
	var dead: bool = playing and match_mode and not bool(sim.is_alive(0))
	_death_label.visible = dead
	if hud.crosshair != null:
		hud.crosshair.visible = not dead and not (aiming and viewmodel.current_id == "awp")
	_debug_label.visible = debug_visible
	if debug_visible:
		_debug_label.text = HudOverlays.debug_text(sim, aiming)

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
