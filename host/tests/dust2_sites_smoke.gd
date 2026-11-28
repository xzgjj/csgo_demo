extends "res://tests/scene_test.gd"
## A/B 独立重设的行为验收（骨架级，不替代核心单测）：
## ① 台面高度：A=128 / B=64，碰撞面与几何一致；
## ② 三条入口坡：A-14 东坡、B-16 西坡、B-18 南坡，脚踩后连续抬升。
## 用法：godot --headless --path host --script res://tests/dust2_sites_smoke.gd

const LIVE_WAIT := 1800
const RAMP_FRAMES := 12

# 台面落地允许 1.5u 误差（盒心/脚点取整），坡道只要连续抬升。
func _until_live(sim: Variant) -> bool:
	var poll := 0
	while poll < LIVE_WAIT:
		poll += 1
		if int(sim.get_round_phase()) == 1:
			return true
		await physics_frame
	return false


func _run() -> void:
	for _boot in 1:
		await process_frame
	var page: Variant = main.get_node_or_null("StartGamePage")
	expect(page != null, "StartGamePage 未装配")
	if page == null:
		finish("DUST2_SITES_SMOKE")
		return
	page.start_requested.emit("de_dust2", "match")
	for i in 3:
		await physics_frame
	var sim: Variant = main.sim
	expect(await _until_live(sim), "30 秒内应进入 Live")
	sim.set_bot_fire_enabled(false)
	main.set_physics_process(false)

	var a_z := await _floor_at(sim, 1500.0, 1550.0, 128.0)
	var b_z := await _floor_at(sim, -1500.0, 1850.0, 64.0)
	expect(absf(a_z - 128.0) < 1.5, "A 台面应 128，实际 %.1f" % a_z)
	expect(absf(b_z - 64.0) < 1.5, "B 台面应 64，实际 %.1f" % b_z)

	await _ramp(sim, Vector2(780.0, 1580.0), 0.0, "A-14 东坡")
	await _ramp(sim, Vector2(-1040.0, 1630.0), 180.0, "B-16 西坡")
	await _ramp(sim, Vector2(-1170.0, 1380.0), 90.0, "B-18 南坡")
	finish("DUST2_SITES_SMOKE")


func _floor_at(sim: Variant, x: float, y: float, nominal: float) -> float:
	sim.set_player_position(0, x, y, nominal + 8.0)
	sim.set_move(0, 0.0, 0.0, 0.0, 0.0, false, false)
	for i in 30:
		sim.tick_auto()
	return float(sim.get_z(0))


func _ramp(sim: Variant, start: Vector2, yaw: float, label: String) -> void:
	sim.set_player_position(0, start.x, start.y, 0.0)
	sim.set_move(0, yaw, 0.0, 0.0, 0.0, false, false)
	for i in 2:
		sim.tick_auto()
	var prev_z := float(sim.get_z(0))
	var max_step := 0.0
	for i in RAMP_FRAMES:
		sim.set_move(0, yaw, 0.0, 1.0, 0.0, false, false)
		sim.tick_auto()
		var now_z := float(sim.get_z(0))
		max_step = maxf(max_step, absf(now_z - prev_z))
		prev_z = now_z
	expect(max_step < 32.0, "%s 单帧抬升应 <32，实际 %.1f" % [label, max_step])
	expect(prev_z > 5.0, "%s 应抬升，实际 z=%.1f" % [label, prev_z])
	expect(max_step > 0.5, "%s 应有连续抬升，实际最大帧 %.1f" % [label, max_step])
