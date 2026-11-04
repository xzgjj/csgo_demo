extends "res://tests/scene_test.gd"
## 输入契约回归（headless CLI）：WASD/跳/换弹/切枪/右键瞄准/F5 视角/F3 调试/开火与命中反馈归因/击杀 feed。
## 用法：godot --headless --path host --script res://tests/input_smoke.gd

var player_id := 0


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
	await _check_hit_fx_attribution()
	await _check_kill_feed()
	finish("INPUT_SMOKE")

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


func _check_weapon_switch() -> void:
	# 回归（bug2：初始应该只有实际持有的武器，且 1/2 切枪要"看得见"）：
	# 初始 = 主武器 AK(1) + 副武器 Glock(5)，手模/HUD 均以内核 weapon_id 为准。
	if int(main.sim.get_weapon_id(0)) != 1:
		failures.append("初始主武器应为 AK(1)，实际 %d" % int(main.sim.get_weapon_id(0)))
	if main.viewmodel.current_id != "ak":
		failures.append("初始手模应为 ak，实际 %s" % main.viewmodel.current_id)
	_press_key(KEY_2)
	await physics_frame
	await physics_frame
	_release_key(KEY_2)
	if int(main.sim.get_weapon_id(0)) != 5:
		failures.append("2 未切到副武器 Glock：weapon_id=%d" % int(main.sim.get_weapon_id(0)))
	if main.viewmodel.current_id != "glock":
		failures.append("2 切副武器后手模应为 glock（可见切换），实际 %s"
				% main.viewmodel.current_id)
	if not main.hud.weapon_label.text.contains("Glock"):
		failures.append("副武器 HUD 名称应来自内核 Glock，实际 %s" % main.hud.weapon_label.text)
	_press_key(KEY_1)
	await physics_frame
	await physics_frame
	_release_key(KEY_1)
	if int(main.sim.get_weapon_id(0)) != 1:
		failures.append("1 未切回主武器 AK：weapon_id=%d" % int(main.sim.get_weapon_id(0)))
	if main.viewmodel.current_id != "ak":
		failures.append("主武器手模应为 ak：%s" % main.viewmodel.current_id)
	if not main.hud.weapon_label.text.contains("AK-47"):
		failures.append("主武器 HUD 名称应来自内核 AK-47，实际 %s" % main.hud.weapon_label.text)


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
	# 回归（bug3：空弹还能开枪 / R 换弹无反应）：
	# ① 打空弹匣 → 空弹扣扳机不得产生枪声/后坐（只记 dryfire），命中计数不变；
	# ② 按 R → 内核进入换弹（HUD 显示换弹中）→ 换弹完成后弹匣满、备弹减少。
	main.call("_start_play")
	await process_frame
	for shot in 40:
		if int(main.sim.get_mag(player_id)) == 0:
			break
		main.sim.fire(player_id, 180.0, 0.0)  # 朝身后开火，避免打死靶子影响后续用例
		for wait in 8:
			main.sim.tick()
	if int(main.sim.get_mag(player_id)) != 0:
		failures.append("打空弹匣失败：mag=%d" % int(main.sim.get_mag(player_id)))
	var fire_before: int = int(main.sim.get_fire_count(0))
	var shots_before: int = int(main.fx.shot_count)
	var dry_before: int = int(main.fx.dryfire_count)
	_press_mouse(MOUSE_BUTTON_LEFT)
	for i in 3:
		await physics_frame
	_release_mouse(MOUSE_BUTTON_LEFT)
	for i in 3:
		await physics_frame
	if int(main.sim.get_fire_count(0)) != fire_before:
		failures.append("空弹仍触发了内核开火（fire_count %d -> %d）" % [
			fire_before, int(main.sim.get_fire_count(0))])
	if int(main.fx.shot_count) != shots_before:
		failures.append("空弹仍播放了枪声/后坐（shot_count %d -> %d）" % [
			shots_before, int(main.fx.shot_count)])
	if int(main.fx.dryfire_count) <= dry_before:
		failures.append("空弹扣扳机应计 dryfire（保持 %d 未变）" % dry_before)
	# 按 R 换弹：换弹中 HUD 提示 + 结束弹匣满
	_press_key(KEY_R)
	for i in 4:
		await physics_frame
	_release_key(KEY_R)
	var saw_reloading := false
	for i in 12:
		await physics_frame
		if bool(main.sim.is_reloading(0)):
			saw_reloading = true
		if main.hud.weapon_label.text.contains("换弹中"):
			saw_reloading = true
	if not saw_reloading:
		failures.append("R 换弹未生效（内核 reloading 从未为真 / HUD 无换弹提示）")
	main._update_hud()
	for i in 200:
		await physics_frame
	if int(main.sim.get_mag(player_id)) != int(main.sim.get_weapon_mag_capacity(0)):
		failures.append("换弹完成后弹匣应满：mag=%d cap=%d" % [
			int(main.sim.get_mag(player_id)), int(main.sim.get_weapon_mag_capacity(0))])
	if int(main.sim.get_reserve(player_id)) >= 90:
		failures.append("换弹后备弹应减少，实际 %d" % int(main.sim.get_reserve(player_id)))


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
		failures.append("命中未触发命中音/火花（dmg=%d last=%d hit=%d target_alive=%s hp=%d）" % [int(main.sim.get_total_damage_events()), int(main.shot_feedback.last_damage), int(main.fx.hit_count), str(main.sim.is_alive(1)), int(main.sim.get_health(1))])
	if not bool(main.fx.spark.emitting):
		failures.append("命中火花未发射")


func _check_hit_fx_attribution() -> void:
	# 回归（bug1：只瞄准/bot 交火时误触发玩家击中特效）：伤害必须按射手归因。
	# ① bot 开火造成伤害 → 玩家自己的 hit/shot FX 不得变化；
	# ② 玩家自己命中 → hit_count +1。
	main.call("_start_play")
	await process_frame
	main.sim.set_player_position(0, 0.0, 0.0, 0.0)
	main.sim.set_player_position(1, 300.0, 0.0, 0.0)
	main.sim.tick()
	var hits0: int = int(main.fx.hit_count)
	var shots0: int = int(main.fx.shot_count)
	var dmg_by_1: int = int(main.sim.get_damage_count_by(1))
	main.sim.fire(1, 180.0, 0.0)
	for i in 8:
		main.sim.tick()
		await physics_frame
	if int(main.sim.get_damage_count_by(1)) <= dmg_by_1:
		failures.append("bot 开火应造成伤害（damage_count_by(1) 未增加）")
	if int(main.fx.hit_count) != hits0:
		failures.append("bot 交火误触发玩家击中特效：hit %d -> %d" % [
			hits0, int(main.fx.hit_count)])
	if int(main.fx.shot_count) != shots0:
		failures.append("bot 开火误触发玩家枪声：shot %d -> %d" % [
			shots0, int(main.fx.shot_count)])
	var hits1: int = int(main.fx.hit_count)
	main.sim.set_player_position(0, 0.0, 0.0, 0.0)
	main.sim.set_player_position(1, 300.0, 0.0, 0.0)
	main.yaw = 0.0
	main.pitch = 0.0
	main.sim.tick()
	_press_mouse(MOUSE_BUTTON_LEFT)
	for i in 3:
		await physics_frame
	_release_mouse(MOUSE_BUTTON_LEFT)
	for i in 3:
		await physics_frame
	if int(main.fx.hit_count) <= hits1:
		failures.append("玩家自己命中未触发击中特效（hit_count 未增加）")


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

