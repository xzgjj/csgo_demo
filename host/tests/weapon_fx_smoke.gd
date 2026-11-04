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
