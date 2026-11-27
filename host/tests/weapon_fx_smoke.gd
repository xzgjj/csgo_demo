extends "res://tests/scene_test.gd"
## 模块 5（表现层）回归：bot 持枪 / 朝向一致 / 枪口闪光 / 后坐力。
## 三动作：准备（5v5、冻结物理循环）→ 执行（内核 fire + 同步表现）→ 断言。
## 用法：godot --headless --path host --script res://tests/weapon_fx_smoke.gd


func _run() -> void:
	await process_frame
	main.call("_start_match")
	main.set_physics_process(false)
	main.bot_visuals.set_animating(false)
	main._update_view()
	await process_frame
	_check_weapons()
	_check_heading()
	_check_muzzle_flash_and_recoil()
	await _check_player_weapon_feedback()
	finish("WEAPONFX_SMOKE")


func _check_weapons() -> void:
	var count: int = int(main.sim.get_player_count())
	expect(count == 10, "玩家数应为 10，实际 %d" % count)
	for index in range(1, count):
		var team: int = int(main.sim.get_team(index))
		var expect_id := 1 if team == 1 else 2  # T=AK47, CT=M4A4
		expect(int(main.sim.get_weapon_id(index)) == expect_id,
				"bot%d 武器应为 %d，实际 %d" % [index, expect_id, int(main.sim.get_weapon_id(index))])
		expect(bool(main.bot_visuals.has_weapon_model(index)), "bot%d 手中应挂武器模型" % index)


func _check_heading() -> void:
	# 准备：让 bot 转向（玩家放到 bot 一侧，bot 的 view_yaw 由内核 Move 命令更新）。
	var index := 1
	var bx: float = float(main.sim.get_x(index))
	var by: float = float(main.sim.get_y(index))
	main.sim.set_player_position(0, bx + 300.0, by, 0.0)
	main.sim.set_move(index, 180.0, 0.0, 0.0, 0.0, false, false)
	main.sim.tick()
	main._update_view()
	var node: Node3D = main.bot_visuals.bot_nodes[index - 1]
	var expected: float = deg_to_rad(float(main.sim.get_view_yaw(index)))
	expect(absf(wrapf(node.rotation.y - expected, -PI, PI)) <= deg_to_rad(5.0),
			"bot%d 朝向与 view_yaw 不一致：%.1f° vs %.1f°" % [
				index, rad_to_deg(node.rotation.y), float(main.sim.get_view_yaw(index))])


func _check_muzzle_flash_and_recoil() -> void:
	var index := 5  # CT bot：向玩家方向开火
	var px: float = float(main.sim.get_x(0))
	var py: float = float(main.sim.get_y(0))
	var bx: float = float(main.sim.get_x(index))
	var by: float = float(main.sim.get_y(index))
	var fire_before: int = int(main.sim.get_fire_count(index))
	var yaw := rad_to_deg(atan2(py - by, px - bx))
	main.sim.fire(index, yaw, 0.0)
	main.sim.tick()
	main._update_view()
	main.bot_visuals.advance(0.0)
	expect(int(main.sim.get_fire_count(index)) == fire_before + 1,
			"bot%d 开火计数应 +1（%d → %d）" % [
				index, fire_before, int(main.sim.get_fire_count(index))])
	expect(bool(main.bot_visuals.flash_visible(index)), "bot%d 开火后枪口闪光应可见" % index)
	expect(float(main.bot_visuals.recoil_amount(index)) > 0.0, "bot%d 开火后应有后坐力位移" % index)
	for step in 12:
		main.bot_visuals.advance(1.0 / 60.0)
	expect(not bool(main.bot_visuals.flash_visible(index)), "0.2s 后枪口闪光应熄灭")
	expect(float(main.bot_visuals.recoil_amount(index)) <= 0.01,
			"0.2s 后后坐力应回位，实际 %.3f" % float(main.bot_visuals.recoil_amount(index)))


## SPEC-02 BAL-06 玩家侧：准星张合 / 相机抬枪 / 枪机参数 / 枪口焰 / 命中标记。
func _check_player_weapon_feedback() -> void:
	main.call("_start_play")
	main.set_physics_process(false)
	await process_frame
	# 玩家 0 对准 +Y 方向 300u 的目标；两发足以让 pattern[1] 抬枪。
	main.sim.set_player_position(0, 0.0, 0.0, 0.0)
	main.sim.set_player_position(1, 0.0, 300.0, 0.0)
	var kick_before: int = int(main.viewmodel.kick_count)
	var shot_before: int = int(main.fx.shot_count)
	var camera_before: float = main.camera.rotation_degrees.x

	# 走真实输入路径：request_fire 记录基线，after_tick 用增量判定"真的开火"。
	main.shot_feedback.request_fire()
	main.sim.fire(0, 90.0, 0.0)
	main.sim.tick()
	main.shot_feedback.after_tick(main.camera)
	main._update_view()
	main._update_hud()
	expect(int(main.viewmodel.kick_count) == kick_before + 1,
			"开火应触发 viewmodel kick（%d → %d）" % [kick_before, int(main.viewmodel.kick_count)])
	expect(int(main.fx.shot_count) == shot_before + 1, "开火应触发枪口焰/枪声计数")
	expect(bool(main.fx.muzzle_flash.visible), "开火当帧枪口焰应可见")
	expect(main.fx.shell.visible, "开火当帧抛壳应可见")
	expect(absf(float(WeaponViewmodel.KICK_TRANSLATION.z) - 0.03) < 0.0001,
			"枪机位移 z 应为 0.03")
	expect(absf(absf(float(WeaponViewmodel.KICK_ROTATION_DEG.x)) - 1.2) < 0.0001,
			"枪机俯仰应为 1.2°")

	for step in 6:
		main.sim.tick()
	main.shot_feedback.request_fire()
	main.sim.fire(0, 90.0, 0.0)
	main.sim.tick()
	main.shot_feedback.after_tick(main.camera)
	main._update_view()
	main._update_hud()
	var recoil_pitch: float = float(main.sim.get_recoil_pitch(0))
	expect(recoil_pitch >= 0.99, "第 2 发后图案抬枪应 ≥1°（实际 %.2f）" % recoil_pitch)
	expect(main.camera.rotation_degrees.x > camera_before + 0.5,
			"相机应同步抬枪（%.2f° → %.2f°）" % [camera_before, main.camera.rotation_degrees.x])
	var inaccuracy: float = float(main.sim.get_effective_inaccuracy(0))
	expect(inaccuracy > 0.35, "第 2 发扩散应大于首发（实际 %.2f）" % inaccuracy)
	expect(float(main.hud.crosshair.gap) > float(Crosshair.BASE_GAP),
			"准星应随扩散张开（gap %.1f）" % float(main.hud.crosshair.gap))
	expect(int(main.sim.get_shot_index(0)) == 2, "连发序号应为 2（实际 %d）"
			% int(main.sim.get_shot_index(0)))

	# 命中标记：由 main 依据 DamageApplied flags 触发（爆头红/穿墙描边）。
	main.hud.show_hit(false, false)
	expect(bool(main.hud.hitmarker.active()), "命中后 hitmarker 应激活")
	main.hud.show_hit(true, true)
	expect(bool(main.hud.hitmarker.active()), "爆头+穿墙 hitmarker 应激活")
