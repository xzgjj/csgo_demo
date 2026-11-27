extends "res://tests/scene_test.gd"
## de_dust2 开局 E2E：主菜单开局页 → 地图注入 → 回合/人数/购买/拒绝计数。
## 用法：godot --headless --path host --script res://tests/de_dust2_start_smoke.gd


func _run() -> void:
	await process_frame
	var page: Variant = main.get_node_or_null("StartGamePage")
	if page == null:
		failures.append("StartGamePage 未装配")
		finish("DE_DUST2_START_SMOKE")
		return
	page.start_requested.emit("de_dust2", "match")
	for i in 3:
		await physics_frame
	_expect(main.map_id == "de_dust2", "map_id 应为 de_dust2，实际 %s" % main.map_id)
	_expect(int(main.sim.get_wall_count()) == 146, "碰撞墙数应为 146，实际 %d" % int(
			main.sim.get_wall_count()))
	_expect(int(main.sim.get_map_rejected()) == 0, "地图注入不应有拒绝盒")
	_expect(int(main.sim.get_map_first_error()) == 0, "地图首错码应为 0，实际 %d" % int(
			main.sim.get_map_first_error()))
	_expect(int(main.sim.get_alive_count(1)) == 5 and int(main.sim.get_alive_count(2)) == 5,
			"开局双方应各 5 人")
	_expect(int(main.sim.get_round_number()) == 1, "开局回合号应为 1")
	_expect(bool(main.sim.buy_enabled()), "开局后购买应开启")
	_expect(main.world.map_id == "de_dust2", "画面世界应为 de_dust2，实际 %s" % main.world.map_id)
	_expect(int(main.world.object_count) == 175, "画面盒数应为 175，实际 %d" % int(
			main.world.object_count))
	page.start_requested.emit("de_dust2", "training")
	for i in 2:
		await physics_frame
	_expect(not bool(main.match_mode), "训练模式 match_mode 应为 false")
	_expect(int(main.sim.get_wall_count()) == 146, "训练模式也应注入 de_dust2 碰撞")
	_expect(main.world.map_id == "de_dust2", "训练模式画面世界应保持 de_dust2")
	finish("DE_DUST2_START_SMOKE")


func _expect(ok: bool, message: String) -> void:
	if not ok:
		failures.append(message)
