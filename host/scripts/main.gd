extends Node3D
## Project Recoil demo 宿主：输入 → Command，内核 tick → 只读状态 → 相机 / HUD / 页面。
## 页面流：主菜单 → 对局 →（Esc）暂停 → 设置；回合/比赛结束自动弹结算；Tab 比分板。
## run_scenario.gd / capture.gd 依赖 sim、_start_play()、_update_view()、_update_hud()，签名不变。
const EYE_HEIGHT := 64.0
const AccountStoreScript = preload("res://scripts/account_store.gd")
const LoadoutPageScript = preload("res://scripts/loadout_page.gd")
const CombatFxScript = preload("res://scripts/combat_fx.gd")
const ShotFeedbackScript = preload("res://scripts/shot_feedback.gd")
const MapLoaderScript = preload("res://scripts/map_loader.gd")
const BotVisualsScript = preload("res://scripts/bot_visuals.gd")
const PurchaseFlowScript = preload("res://scripts/purchase_flow.gd")
const MatchFlowScript = preload("res://scripts/match_flow.gd")
var sim: RefCounted
var world: DeAshWorld
var map_id := "de_ash"
var settings: SettingsStore
var account
var loadout_page
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
var scope
var radar
var buy_menu
var purchase
var match_flow
var bot_visuals: Node3D
var fx
var blind_overlay: ColorRect
var yaw := 0.0
var pitch := 0.0
var playing := false
var paused := false
var match_mode := false
var _settings_return_pause := false
var _fire_was_down := false
var _reload_was_down := false
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
var shot_feedback
var _f5_was := false


func _ready() -> void:
	sim = RecoilSim.new()
	sim.reset()
	settings = SettingsStore.new()
	settings.load_settings()
	account = AccountStoreScript.new()
	account.load_account()
	map_id = MapLoaderScript.cli_map_id()
	_build_world()
	_build_ui()
	_apply_settings()
	_show_menu()
func _build_world() -> void:
	world = DeAshWorld.new()
	world.name = "World"
	add_child(world)
	world.build(MapLoaderScript.boxes_path_for(map_id), map_id)
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
	bot_visuals = BotVisualsScript.new()
	bot_visuals.name = "BotVisuals"
	add_child(bot_visuals)
	_player_mesh = ActorVisuals.make_capsule(self, Color(0.35, 0.55, 0.90), "Player")
	_player_mesh.visible = false
func _build_ui() -> void:
	HostPages.build(self)
	purchase = PurchaseFlowScript.new()
	purchase.setup(sim, account, buy_menu, viewmodel)
	buy_menu.purchase_requested.connect(Callable(purchase, "on_purchase"))
	shot_feedback = ShotFeedbackScript.new()
	shot_feedback.setup(sim, viewmodel, fx)
	match_flow = MatchFlowScript.new()
	match_flow.setup(self)
	_death_label = HudOverlays.make_death_label(hud)
	_debug_label = HudOverlays.make_debug_label(hud)
func _make_target_mesh() -> MeshInstance3D:
	return ActorVisuals.make_capsule(self, Color(0.75, 0.20, 0.20), "Target")
func _make_smoke_mesh() -> MeshInstance3D:
	return ActorVisuals.make_smoke(self)
func show_loadout_page() -> void:
	menu.visible = false
	loadout_page.open()
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE

func close_loadout_page() -> void:
	loadout_page.close()
	_show_menu()

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

## 可玩模式：注册表地图 + 10 人 5v5（玩家 0 为人类 T，其余由内核战术层驱动）。
func _start_match() -> void:
	sim.reset()
	if not MapLoaderScript.start_match(sim, map_id):
		_show_menu()
		return
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
	match_flow.reset()
	shot_feedback.reset()
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

## de_ash 全尺寸地图注入已拆到 MapLoader（main 只保留页面流与输入）。
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
	var ui_open := _ui_page_open()
	_sync_mouse_mode()
	var forward := Input.get_axis("move_back", "move_forward")
	var side := Input.get_axis("move_right", "move_left")
	if Input.is_key_pressed(KEY_SHIFT):
		forward *= 0.5
		side *= 0.5
	sim.set_move(0, yaw, pitch, forward, side, Input.is_action_pressed("jump"),
			Input.is_action_pressed("duck"))
	# 页面打开时不响应瞄准/开火，避免点 UI 按钮时误开火、或被遮罩"卡住"视角。
	aiming = Input.is_mouse_button_pressed(MOUSE_BUTTON_RIGHT) and not ui_open
	var magnification: float = weapon_magnification()
	scope.sync(aiming, magnification, viewmodel.display_name())
	camera.fov = scope.fps_step(camera.fov, settings.fov)
	viewmodel.set_aiming(aiming)
	_toggle_debug()
	_toggle_third_person()
	var fire_down := Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT) and not ui_open
	if fire_down and not _fire_was_down:
		sim.fire(0, yaw, pitch)
		shot_feedback.request_fire()
	_fire_was_down = fire_down
	var weapon_key := _pressed_weapon_key()
	if weapon_key != -1 and weapon_key != _weapon_key_was:
		_switch_weapon(weapon_key)
	_weapon_key_was = weapon_key
	var reload_down := Input.is_key_pressed(KEY_R)
	if reload_down and not _reload_was_down:
		sim.reload(0)
	_reload_was_down = reload_down
	var buy_down := Input.is_key_pressed(KEY_B)
	if buy_down and not _buy_was_down:
		_toggle_buy()
	_buy_was_down = buy_down
	var pickup_down := Input.is_key_pressed(KEY_E)
	if pickup_down and not _bomb_was_down:
		if not _try_pickup():
			sim.plant_bomb(0)
	_bomb_was_down = pickup_down
	_throw_input("he", KEY_G, 0)
	_throw_input("flash", KEY_F, 1)
	_throw_input("smoke", KEY_V, 2)
	if match_mode:
		sim.tick_auto()
	else:
		sim.tick()
	shot_feedback.after_tick(camera)
	if purchase != null:
		purchase.after_tick()
	_update_view()
	_update_hud()

func _ui_page_open() -> bool:
	return HostPages.page_open(self)


func want_mouse_mode() -> int:
	return Input.MOUSE_MODE_VISIBLE if _ui_page_open() else Input.MOUSE_MODE_CAPTURED


func _sync_mouse_mode() -> void:
	if Input.mouse_mode != want_mouse_mode():
		Input.mouse_mode = want_mouse_mode()


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
	var scoped: bool = scope.is_scoped()
	viewmodel.visible = not third_person and not scoped
	# 所有玩家（含 2 人灰盒模式的靶子）统一用 BotVisuals 类人+持枪表现，旧胶囊靶隐藏。
	target_mesh.visible = false
	bot_visuals.sync(sim)
	var smoke_on: bool = sim.get_smoke_active() != 0
	smoke_mesh.visible = smoke_on
	if smoke_on:
		smoke_mesh.position = Vector3(-sim.get_smoke_y(), 144.0, -sim.get_smoke_x())
	blind_overlay.color = Color(1.0, 1.0, 1.0,
			clampf(float(sim.get_blind_ticks(0)) / 128.0, 0.0, 0.85))
	match_flow.update(sim)
	if scoreboard.visible:
		scoreboard.refresh(sim)

func _update_hud() -> void:
	var weapon_name := "-"
	if viewmodel != null:
		weapon_name = _weapon_name()
	hud.refresh(sim, weapon_name)
	if purchase != null and purchase.auto_sync:
		purchase.sync_viewmodel()  # 手模跟随内核当前武器（切枪/购买/拾取后下一帧即同步）
	if purchase != null and purchase.is_open():
		purchase.refresh()
	var dead: bool = playing and match_mode and not bool(sim.is_alive(0))
	_death_label.visible = dead
	if hud.crosshair != null:
		hud.crosshair.visible = not dead and not scope.is_scoped()
	_debug_label.visible = debug_visible
	if debug_visible:
		_debug_label.text = HudOverlays.debug_text(sim, aiming)

## 武器名/倍率以内核为准（模块 7：weapon_id → 名字/倍镜），手模始终跟随内核当前武器。
func _weapon_name() -> String:
	if sim != null and sim.has_method("get_weapon_name"):
		return str(sim.get_weapon_name(int(sim.get_weapon_id(0))))
	return viewmodel.display_name()


func weapon_magnification() -> float:
	if sim != null and sim.has_method("get_weapon_scope"):
		return float(sim.get_weapon_scope(0))
	return viewmodel.scope_magnification()


func _pressed_weapon_key() -> int:
	if Input.is_key_pressed(KEY_1):
		return 0
	if Input.is_key_pressed(KEY_2):
		return 1
	return -1

func _switch_weapon(index: int) -> void:
	if purchase != null:
		purchase.switch_weapon(index)

## 模块 6：E 键靠近掉落物优先拾取；附近无掉落物才下包。
func _try_pickup() -> bool:
	if sim == null or not sim.has_method("pickup_weapon"):
		return false
	if int(sim.get_nearest_ground_weapon(0)) < 0:
		return false
	sim.pickup_weapon(0)
	if purchase != null:
		purchase.sync_viewmodel()
	return true


func _toggle_buy() -> void:
	if purchase == null:
		return
	purchase.toggle()
	_sync_mouse_mode()

func _throw_input(name: String, key: int, kind: int) -> void:
	var down := Input.is_key_pressed(key)
	if down and not _throw_was[name]:
		sim.throw_grenade(0, kind, yaw, pitch)
	_throw_was[name] = down
