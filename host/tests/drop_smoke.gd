extends SceneTree
## 模块 6（表现层）回归：死亡倒地动画 + 地面掉落武器 + E 拾取提示 + 拾取后武器变化/回合复位。
## 三动作：准备（5v5、冻结物理）→ 执行（击杀 bot / 靠近 / 拾取 / 回合重置）→ 断言。
## 用法：godot --headless --path host --script res://tests/drop_smoke.gd

var main: Node
var failures: Array[String] = []


func _initialize() -> void:
	main = load("res://main.tscn").instantiate()
	root.add_child(main)
	_run.call_deferred()


func _run() -> void:
	await process_frame
	main.call("_start_match")
	main.set_physics_process(false)
	main.bot_visuals.set_animating(false)
	var victim := _kill_ct_bot()
	await process_frame
	_check_drop(victim)
	_check_ragdoll(victim)
	_check_pickup(victim)
	_check_round_reset()
	if failures.is_empty():
		print("DROP_SMOKE PASS")
		quit(0)
	else:
		for item in failures:
			print("DROP_FAIL ", item)
		quit(1)


func _expect(condition: bool, message: String) -> void:
	if not condition:
		failures.append(message)


## 把玩家放到第一个 CT bot 的出生位（同一格，保证无墙体遮挡）并开枪打死它；返回该 bot 索引。
func _kill_ct_bot() -> int:
	for index in range(1, int(main.sim.get_player_count())):
		if int(main.sim.get_team(index)) != 2:
			continue
		var bx: float = float(main.sim.get_x(index))
		var by: float = float(main.sim.get_y(index))
		main.sim.set_player_position(0, bx, by, 0.0)
		main.sim.tick()
		for shot in 8:
			if not bool(main.sim.is_alive(index)):
				break
			main.sim.fire(0, 0.0, 0.0)
			for wait in 8:
				main.sim.tick()
		return index
	return -1


func _check_drop(victim: int) -> void:
	main._update_view()
	_expect(victim > 0, "应找到可击杀的 CT bot")
	_expect(not bool(main.sim.is_alive(victim)), "bot%d 应死亡" % victim)
	_expect(int(main.sim.get_ground_weapon_count()) == 1,
			"死亡后应掉落 1 把主武器，实际 %d" % int(main.sim.get_ground_weapon_count()))
	_expect(int(main.sim.get_ground_weapon_id(0)) == 2, "CT bot 应掉落 M4A4（id=2）")
	_expect(main.bot_visuals.ground_count() >= 1, "地面武器表现节点应存在")
	if main.bot_visuals.ground_count() >= 1:
		_expect(bool(main.bot_visuals.ground_nodes[0].visible), "地面武器节点应可见")


func _check_ragdoll(victim: int) -> void:
	var node: Node3D = main.bot_visuals.bot_nodes[victim - 1]
	# 执行：推进 0.5s 倒地动画
	for step in 30:
		main.bot_visuals.advance(1.0 / 60.0)
	_expect(absf(rad_to_deg(node.rotation.z)) >= 30.0,
			"bot%d 倒地应旋转（实际 %.1f°）" % [victim, rad_to_deg(node.rotation.z)])
	_expect(node.position.y < 0.0, "bot%d 倒地应下沉（y=%.1f）" % [victim, node.position.y])
	for step in 40:
		main.bot_visuals.advance(1.0 / 60.0)
	_expect(not bool(node.visible), "倒地动画结束后 bot 应隐藏")


func _check_pickup(victim: int) -> void:
	var gx: float = float(main.sim.get_ground_weapon_x(0))
	var gy: float = float(main.sim.get_ground_weapon_y(0))
	main.sim.set_player_position(0, gx, gy - 32.0, 0.0)
	main.sim.tick()
	main._update_view()
	_expect(int(main.sim.get_nearest_ground_weapon(0)) == 0, "玩家靠近后应找到最近掉落武器")
	_expect(bool(main.bot_visuals.prompt_visible()), "靠近掉落物应显示 E 拾取提示")
	_expect(main.bot_visuals.prompt_text().contains("拾取"),
			"提示文字应含拾取：%s" % main.bot_visuals.prompt_text())
	var dropped_id := int(main.sim.get_ground_weapon_id(0))
	# 执行：拾取
	_expect(bool(main._try_pickup()), "E 拾取应生效")
	main.sim.tick()
	main._update_view()
	_expect(int(main.sim.get_ground_weapon_count()) == 0, "拾取后地面武器应消失")
	_expect(int(main.sim.get_weapon_id(0)) == dropped_id,
			"拾取后玩家武器应为 %d，实际 %d" % [dropped_id, int(main.sim.get_weapon_id(0))])
	if main.bot_visuals.ground_count() >= 1:
		_expect(not bool(main.bot_visuals.ground_nodes[0].visible), "拾取后地面武器节点应隐藏")
	_expect(not bool(main.bot_visuals.prompt_visible()), "拾取后提示应隐藏")


func _check_round_reset() -> void:
	# 执行：推进到回合重置（首回合 Freeze 192 + Live 7360 到点 → RoundEnd 320 → ResetRound）
	var round_before := int(main.sim.get_round_number())
	for tick in 9000:
		main.sim.tick()
		if int(main.sim.get_round_number()) > round_before:
			break
	main._update_view()
	_expect(int(main.sim.get_round_number()) == round_before + 1,
			"应进入下一回合（%d → %d）" % [round_before, int(main.sim.get_round_number())])
	var alive_count: int = int(main.sim.get_alive_count(2))
	_expect(alive_count == 5, "新回合 CT 应 5 人存活，实际 %d" % alive_count)
	var reset_ok := true
	for index in range(1, int(main.sim.get_player_count())):
		if not bool(main.sim.is_alive(index)):
			continue
		var node: Node3D = main.bot_visuals.bot_nodes[index - 1]
		if absf(node.rotation.z) > 0.01 or absf(node.position.y) > 0.01 or not node.visible:
			reset_ok = false
	_expect(reset_ok, "新回合存活 bot 的倒地姿态/位置/可见性应复位")
