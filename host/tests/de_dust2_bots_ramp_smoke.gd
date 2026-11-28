extends "res://tests/scene_test.gd"
## de_dust2 玩法 E2E（spec-debug2 AC-15 / AC-03）：
##   ① 进入 Live 后队友（bot）必须自己动起来（旧版热身 45 秒全员呆立）；
##   ② 宿主注入的 A-14 坡道确实“托脚”：站上坡脚后高度连续上升、单帧 <32u。
##      完整爬到 128 平台由内核单测 `test_ramp.cpp::A-14 实际尺寸` 覆盖（这里只验注入）。
## 用法：godot --headless --path host --script res://tests/de_dust2_bots_ramp_smoke.gd

const BOT_FRAMES := 180      # 3 秒
const RAMP_FRAMES := 12      # 0.2 秒；A 坡暴露在 CT 视野内，久了会被打死
const LIVE_WAIT := 1800      # 冻结期最长 30 秒


func _wait_live(sim: Variant) -> bool:
	for i in LIVE_WAIT:
		if int(sim.get_round_phase()) == 1:
			return true
		await physics_frame
	return false


func _run() -> void:
	await process_frame
	var page: Variant = main.get_node_or_null("StartGamePage")
	expect(page != null, "StartGamePage 未装配")
	if page == null:
		finish("DE_DUST2_BOTS_RAMP_SMOKE")
		return
	page.start_requested.emit("de_dust2", "match")
	for i in 3:
		await physics_frame
	var sim: Variant = main.sim
	expect(await _wait_live(sim), "30 秒内应进入 Live，实际 phase=%d" % int(sim.get_round_phase()))
	# 本用例只验“会不会动/坡道托不托脚”，关掉 bot 开火避免 A 坡上的玩家中途被打死。
	sim.set_bot_fire_enabled(false)
	
	# ── ① 队友会不会动 ────────────────────────────────────────────────
	var before: Array[Vector2] = []
	for i in range(1, 10):
		before.append(Vector2(float(sim.get_x(i)), float(sim.get_y(i))))
	for i in BOT_FRAMES:
		await physics_frame
	var movers := 0
	for i in range(1, 10):
		var now := Vector2(float(sim.get_x(i)), float(sim.get_y(i)))
		if before[i - 1].distance_to(now) > 64.0:
			movers += 1
	expect(movers >= 4, "3 秒内应有 ≥4 个 bot 移动 >64u，实际 %d" % movers)
	
	# ── ② A-14 坡脚托脚 ──────────────────────────────────────────────
	main.set_physics_process(false) # 关掉每帧输入覆盖，按 64Hz 直接给内核下命令
	sim.set_player_position(0, 745.0, 1550.0, 0.0)
	expect(bool(sim.is_alive(0)), "坡道测试前玩家应存活（hp=%d）" % int(sim.get_health(0)))
	var max_step := 0.0
	var prev_z := float(sim.get_z(0))
	for i in RAMP_FRAMES:
		sim.set_move(0, 0.0, 0.0, 1.0, 0.0, false, false)
		sim.tick_auto()
		var z := float(sim.get_z(0))
		max_step = maxf(max_step, absf(z - prev_z))
		prev_z = z
	expect(max_step < 32.0, "坡上单帧抬升应 <32u，实际 %.1f" % max_step)
	expect(prev_z > 0.5, "坡脚应托脚（z>0.5），实际 %.1f" % prev_z)
	expect(float(sim.get_x(0)) > 748.0, "应沿坡前进，实际 x=%.0f" % float(sim.get_x(0)))
	
	finish("DE_DUST2_BOTS_RAMP_SMOKE")
