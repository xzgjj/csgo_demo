extends SceneTree
## 输入契约回归（headless CLI）：WASD/跳/换弹/切枪/右键瞄准/F5 视角/F3 调试。
## 用法：godot --headless --path host --script res://tests/input_smoke.gd
## 失败时输出 INPUT_FAIL 行并以非 0 退出；run_all.py / CI 可据此断言。

var main: Node
var failures: Array[String] = []
var player_id := 0


func _initialize() -> void:
	main = load("res://main.tscn").instantiate()
	root.add_child(main)
	_run.call_deferred()


func _run() -> void:
	await process_frame
	main.call("_start_play")
	await _check_move()
	await _check_jump()
	await _check_weapon_switch()
	await _check_aim()
	await _check_view_toggle()
	await _check_debug_toggle()
	await _check_reload()
	await _check_combat_fx()
	await _check_kill_feed()
	await _check_spawn_hud()
	await _check_scope()
	await _check_buy_menu()
	await _check_loadout()
	if failures.is_empty():
		print("INPUT_SMOKE PASS")
		quit(0)
	else:
		for item in failures:
			print("INPUT_FAIL ", item)
		quit(1)


func _check_move() -> void:
	var start := Vector2(float(main.sim.get_x(player_id)), float(main.sim.get_y(player_id)))
	Input.action_press("move_forward")
	await physics_frame
	await physics_frame
	for i in 20:
		await physics_frame
	Input.action_release("move_forward")
	var moved := Vector2(float(main.sim.get_x(player_id)), float(main.sim.get_y(player_id)))
	if moved.distance_to(start) < 40.0:
		failures.append("WASD 未移动：%s -> %s" % [start, moved])


func _check_jump() -> void:
	var z0 := float(main.sim.get_z(player_id))
	Input.action_press("jump")
	await physics_frame
	Input.action_release("jump")
	var peak := z0
	for i in 24:
		await physics_frame
		peak = maxf(peak, float(main.sim.get_z(player_id)))
	if peak < z0 + 2.0:
		failures.append("Space 未起跳：z %.1f -> %.1f" % [z0, peak])


func _press_key(code: Key) -> void:
	var event := InputEventKey.new()
	event.physical_keycode = code
	event.keycode = code
	event.pressed = true
	Input.parse_input_event(event)


func _release_key(code: Key) -> void:
	var event := InputEventKey.new()
	event.physical_keycode = code
	event.keycode = code
	event.pressed = false
	Input.parse_input_event(event)


func _press_mouse(button: MouseButton) -> void:
	var event := InputEventMouseButton.new()
	event.button_index = button
	event.pressed = true
	Input.parse_input_event(event)


func _release_mouse(button: MouseButton) -> void:
	var event := InputEventMouseButton.new()
	event.button_index = button
	event.pressed = false
	Input.parse_input_event(event)


func _check_weapon_switch() -> void:
	# 1/2 = 主/副武器槽（内核 weapon_id 权威，手模跟随内核）。
	_press_key(KEY_2)
	await physics_frame
	await physics_frame
	_release_key(KEY_2)
	if int(main.sim.get_weapon_id(0)) != 5:
		failures.append("2 未切到副武器 Glock：weapon_id=%d" % int(main.sim.get_weapon_id(0)))
	_press_key(KEY_1)
	await physics_frame
	await physics_frame
	_release_key(KEY_1)
	if int(main.sim.get_weapon_id(0)) != 1:
		failures.append("1 未切回主武器 AK：weapon_id=%d" % int(main.sim.get_weapon_id(0)))
	if main.viewmodel.current_id != "ak":
		failures.append("主武器手模应为 ak：%s" % main.viewmodel.current_id)


func _check_aim() -> void:
	var base_fov: float = float(main.settings.fov)
	_press_mouse(MOUSE_BUTTON_RIGHT)
	for i in 10:
		await physics_frame
	if float(main.camera.fov) >= base_fov - 3.0:
		failures.append("右键瞄准 FOV 未收窄：%.1f (base %.1f)" % [main.camera.fov, base_fov])
	_release_mouse(MOUSE_BUTTON_RIGHT)
	for i in 10:
		await physics_frame


func _check_view_toggle() -> void:
	_press_key(KEY_F5)
	await physics_frame
	await physics_frame
	_release_key(KEY_F5)
	if not bool(main.third_person):
		failures.append("F5 未切到第三人称")
	await physics_frame
	_press_key(KEY_F5)
	await physics_frame
	await physics_frame
	_release_key(KEY_F5)
	if bool(main.third_person):
		failures.append("F5 未切回第一人称")


func _check_debug_toggle() -> void:
	_press_key(KEY_F3)
	await physics_frame
	await physics_frame
	_release_key(KEY_F3)
	if not bool(main.debug_visible):
		failures.append("F3 未打开调试信息")
	_press_key(KEY_F3)
	await physics_frame
	await physics_frame
	_release_key(KEY_F3)


func _check_reload() -> void:
	main.sim.fire(player_id, 0.0, 0.0)
	await physics_frame
	var mag_before := int(main.sim.get_mag(player_id))
	_press_key(KEY_R)
	await physics_frame
	await physics_frame
	_release_key(KEY_R)
	for i in 170:
		await physics_frame
	if int(main.sim.get_mag(player_id)) != 30:
		failures.append("R 换弹未生效：mag %d -> %d" % [mag_before, main.sim.get_mag(player_id)])


func _check_combat_fx() -> void:
	main.sim.set_player_position(0, 0.0, 0.0, 0.0)
	var tx := float(main.sim.get_x(1))
	var ty := float(main.sim.get_y(1))
	main.yaw = atan2(ty, tx) * 180.0 / PI
	main.pitch = 0.0
	var shot_before: int = int(main.fx.shot_count)
	var hit_before: int = int(main.fx.hit_count)
	_press_mouse(MOUSE_BUTTON_LEFT)
	for i in 3:
		await physics_frame
	_release_mouse(MOUSE_BUTTON_LEFT)
	for i in 3:
		await physics_frame
	if int(main.fx.shot_count) <= shot_before:
		failures.append("开火未触发枪声（shot_count 未增加）")
	if int(main.fx.hit_count) <= hit_before:
		failures.append("命中未触发命中音/火花（dmg=%d last=%d hit=%d target_alive=%s hp=%d）" % [int(main.sim.get_total_damage_events()), int(main._last_damage), int(main.fx.hit_count), str(main.sim.is_alive(1)), int(main.sim.get_health(1))])
	if not bool(main.fx.spark.emitting):
		failures.append("命中火花未发射")


func _check_kill_feed() -> void:
	# 干净状态：无击杀 → 无任何击杀记录（三动作：准备 _start_play / 执行瞄准 / 断言无记录）
	main.call("_start_play")
	await process_frame
	if not main.hud.feed_lines().is_empty():
		failures.append("开局 feed 非空（无击杀却有击杀记录）")
	if int(main.hud.kill_total) != 0 or int(main.hud.death_total) != 0:
		failures.append("开局击杀/阵亡计数非 0：%d/%d" % [
			int(main.hud.kill_total), int(main.hud.death_total)])
	var hit_before_aim: int = int(main.fx.hit_count)
	_press_mouse(MOUSE_BUTTON_RIGHT)
	for i in 12:
		await physics_frame
	_release_mouse(MOUSE_BUTTON_RIGHT)
	for i in 2:
		await physics_frame
	if int(main.fx.hit_count) != hit_before_aim:
		failures.append("瞄准产生了命中 FX（hit_count %d -> %d）" % [
			hit_before_aim, int(main.fx.hit_count)])
	# 确定性对枪：逐枪开火直到目标死亡；HUD 只应收到真实 PlayerDied（1 杀 1 死、feed 1 条）
	main.yaw = atan2(float(main.sim.get_y(1)), float(main.sim.get_x(1))) * 180.0 / PI
	main.pitch = 0.0
	for shot in 6:
		if not bool(main.sim.is_alive(1)):
			break
		main.sim.fire(0, main.yaw, 0.0)
		for tick in 8:
			main.sim.tick()
			await physics_frame
	for frame in 3:
		await physics_frame
	var lines: Array[String] = main.hud.feed_lines()
	if int(main.sim.get_kill_count()) != 1:
		failures.append("击杀事件数应为 1（实际 %d，hp=%d）" % [
			int(main.sim.get_kill_count()), int(main.sim.get_health(1))])
	if bool(main.sim.is_alive(1)):
		failures.append("目标未死亡（hp=%d）" % int(main.sim.get_health(1)))
	if lines.size() != 1:
		failures.append("击杀 feed 应为 1 条，实际 %d 条：%s" % [lines.size(), str(lines)])
	elif not lines[0].contains("T·P0") or not lines[0].contains("CT·P1"):
		failures.append("feed 未记录真 shooter/victim：%s" % lines[0])
	if int(main.hud.kill_total) != 1:
		failures.append("个人击杀数应为 1（实际 %d）" % int(main.hud.kill_total))
	if int(main.hud.death_total) != 0:
		failures.append("玩家 0 不应阵亡（death_total=%d）" % int(main.hud.death_total))


## 模块 1：出生点 HUD 队友存活（雷达存活条；灯数/数字与内核 5v5 存活一致）。
func _check_spawn_hud() -> void:
	main.call("_start_match")
	for i in 3:
		await physics_frame
	if not bool(main.radar.visible):
		failures.append("出生点雷达/队友存活条未显示")
	var lamps: Array[bool] = main.radar.squad_lamps()
	var alive_expected: int = int(main.sim.get_alive_count(1))
	if lamps.size() != 5:
		failures.append("队友存活灯应为 5 盏，实际 %d" % lamps.size())
	var alive_lamps := 0
	for lit: bool in lamps:
		if lit:
			alive_lamps += 1
	if alive_lamps != alive_expected:
		failures.append("存活灯数与 alive_count(1) 不一致：灯 %d / 内核 %d" % [
			alive_lamps, alive_expected])
	if int(main.radar.squad_alive_count()) != alive_expected:
		failures.append("存活数字与 alive_count(1) 不一致：%d / %d" % [
			int(main.radar.squad_alive_count()), alive_expected])
	if alive_expected != 5:
		failures.append("出生点队友应 5 人存活，实际 %d" % alive_expected)


## 模块 8：AWP 4x 开镜 overlay + HUD 倍率；AK 机瞄只有提示不遮罩。
func _check_scope() -> void:
	main.call("_start_match")
	await process_frame
	main.playing = false
	main.aiming = true
	main.viewmodel.select_weapon("awp")
	var base_fov: float = float(main.settings.fov)
	var magnification: float = float(main.weapon_magnification())
	main.scope.sync(true, magnification, main.viewmodel.display_name())
	main.camera.fov = main.scope.target_fov(base_fov)
	main._update_view()
	main._update_hud()
	await process_frame
	if absf(magnification - 4.0) > 0.01:
		failures.append("AWP 倍率应为 4.0x，实际 %.2f" % magnification)
	if not bool(main.scope.overlay.visible):
		failures.append("AWP 开镜 overlay 未显示")
	if not bool(main.scope.is_scoped()):
		failures.append("AWP 开镜未判定为倍镜")
	if not str(main.scope.overlay.scope_text()).contains("4.0x"):
		failures.append("HUD 倍率文字不含 4.0x：%s" % str(main.scope.overlay.scope_text()))
	if bool(main.hud.crosshair.visible):
		failures.append("开镜时准星应隐藏")
	if absf(float(main.camera.fov) - base_fov / 4.0) > 0.5:
		failures.append("AWP 开镜 FOV 应为 base/4：%.1f / base %.1f" % [
			float(main.camera.fov), base_fov])
	main.viewmodel.select_weapon("ak")
	var iron: float = float(main.weapon_magnification())
	main.scope.sync(true, iron, main.viewmodel.display_name())
	main._update_view()
	await process_frame
	if bool(main.scope.is_scoped()):
		failures.append("AK 机瞄被误判为倍镜（倍率 %.2f）" % iron)
	if absf(main.scope.target_fov(base_fov) - base_fov / 1.15) > 0.5:
		failures.append("AK 机瞄 FOV 应为 base/1.15：%.1f" % main.scope.target_fov(base_fov))
	if not str(main.scope.overlay.scope_text()).contains("机瞄"):
		failures.append("HUD 机瞄文字异常：%s" % str(main.scope.overlay.scope_text()))
	main.aiming = false
	main.playing = true
	main.scope.sync(false, iron, main.viewmodel.display_name())
	main.camera.fov = base_fov


## 模块 9：购买菜单分类/价格/余额/灰显 + 购买扣钱/装备进内核（800 起始金 → 只买得起手枪）。
func _check_buy_menu() -> void:
	main.call("_start_play")
	await process_frame
	main.purchase.toggle()
	await process_frame
	if not bool(main.buy_menu.is_open()):
		failures.append("B 未打开购买菜单")
	if int(main.buy_menu.balance) != int(main.sim.get_money(0)):
		failures.append("菜单余额与内核不一致：%d / %d" % [
			int(main.buy_menu.balance), int(main.sim.get_money(0))])
	if not bool(main.buy_menu.buyable("glock_18")):
		failures.append("800 起始金应可买 Glock-18")
	if bool(main.buy_menu.buyable("ak47")):
		failures.append("800 起始金不应可买 AK-47（应灰显）")
	var money_before: int = int(main.sim.get_money(0))
	if bool(main.buy_menu.buy_item("ak47")):
		failures.append("买不起的 AK-47 不应发出购买请求")
	if int(main.sim.get_money(0)) != money_before:
		failures.append("买不起时余额不应变化")
	if not bool(main.buy_menu.buy_item("glock_18")):
		failures.append("买 Glock-18 应发出购买请求")
	# 内核 Buy 是命令：下一 tick 才结算（真实对局里由 _physics_process 推进）。
	await physics_frame
	await physics_frame
	if int(main.sim.get_money(0)) != money_before - 200:
		failures.append("买 Glock-18 后余额应为 %d，实际 %d" % [
			money_before - 200, int(main.sim.get_money(0))])
	if int(main.buy_menu.balance) != int(main.sim.get_money(0)):
		failures.append("购买后菜单余额未刷新：%d" % int(main.buy_menu.balance))
	main.buy_menu.close()
	await process_frame
	if bool(main.buy_menu.is_open()):
		failures.append("购买菜单未关闭")


func _check_loadout() -> void:
	main.show_loadout_page()
	await process_frame
	if not bool(main.loadout_page.visible):
		failures.append("物品栏页面未打开")
	main.account.set_primary("awp")
	await process_frame
	if str(main.account.loadout["primary"]) != "awp":
		failures.append("物品栏主武器选择未持久化")
	main.close_loadout_page()
	await process_frame
	if bool(main.loadout_page.visible):
		failures.append("物品栏页面未关闭")
