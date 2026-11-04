extends SceneTree
## 场景执行器：读取 ScenarioSpec，跑内核脚本 → 代码断言 → 截图检查点。
## 用法：
##   godot --path host --script res://tests/run_scenario.gd -- <scenario.json 绝对路径> <out_dir> [--visual]
## 动作：move / fire / tick / buy_armor / check；check 为逐步断言（断言列表写在 step.asserts），
## 用于在场景中间核对状态（如初始金钱/回合阶段），最终断言仍写在顶层 asserts。

var main: Node
var failures: Array = []

func _initialize() -> void:
	var args: PackedStringArray = OS.get_cmdline_user_args()
	if args.size() < 2:
		push_error("用法: <scenario> <out_dir> [--visual]")
		quit(2)
		return
	var scenario_path := args[0]
	var out_dir := args[1]
	var visual := "--visual" in args
	var text := FileAccess.get_file_as_string(scenario_path)
	if text.is_empty():
		push_error("无法读取场景: " + scenario_path)
		quit(2)
		return
	var data: Variant = JSON.parse_string(text)
	if typeof(data) != TYPE_DICTIONARY:
		push_error("场景 JSON 非法: " + scenario_path)
		quit(2)
		return
	DirAccess.make_dir_recursive_absolute(out_dir)
	main = load("res://main.tscn").instantiate()
	root.add_child(main)
	_run.call_deferred(data, out_dir, visual)

func _run(data: Dictionary, out_dir: String, visual: bool) -> void:
	await process_frame
	if str(data.get("mode", "demo")) == "match":
		main._start_match()
	else:
		main._start_play()
	# 场景执行期间关闭自动物理 tick，保证状态只由脚本推进（截图可复现）。
	main.set_physics_process(false)
	for step: Dictionary in data.get("script", []):
		_run_step(step)

	# 代码断言（逐步 check 已就地记录失败；此处跑最终断言）
	_check(data.get("asserts", []))

	# 截图检查点
	main._update_view()
	main._update_hud()
	if visual:
		for shot: Dictionary in data.get("screenshots", []):
			await process_frame
			await RenderingServer.frame_post_draw
			var image: Image = root.get_texture().get_image()
			var path := out_dir.path_join(str(shot.get("name", "shot")) + ".png")
			image.save_png(path)
			print("SCREENSHOT ", path, " ", image.get_width(), "x", image.get_height())

	# 结果落盘
	var result := {"id": data.get("id", ""), "failures": failures, "checks": data.get("asserts", []).size()}
	var result_path := out_dir.path_join("result.json")
	var file := FileAccess.open(result_path, FileAccess.WRITE)
	if file:
		file.store_string(JSON.stringify(result, "\t"))
		file.close()
	print("SCENARIO ", data.get("id", ""), " checks=", data.get("asserts", []).size(),
			" failures=", failures.size())
	quit(0 if failures.is_empty() else 1)

func _run_step(step: Dictionary) -> void:
	var action: String = step.get("action", "")
	var player: int = int(step.get("player", 0))
	match action:
		"move":
			main.sim.set_move(player, step.get("yaw", 0.0), step.get("pitch", 0.0),
					step.get("forward", 0.0), step.get("side", 0.0),
					step.get("jump", false), step.get("duck", false))
			_tick(int(step.get("ticks", 1)))
		"fire":
			main.sim.fire(player, step.get("yaw", 0.0), step.get("pitch", 0.0))
			_tick(1)
		"tick":
			_tick(int(step.get("ticks", 1)))
		"buy_armor":
			main.sim.buy_armor(player)
			_tick(1)
		"check":
			_check(step.get("asserts", []))
		"auto_tick":
			_auto_tick(int(step.get("ticks", 1)))
		"assert_nav":
			_assert_nav(step)
		"walk_nav":
			_walk_nav(step)
		"restart_match":
			main._start_match()
		_:
			push_error("未知 action: " + action)

func _tick(count: int) -> void:
	for i in count:
		main.sim.tick()

## 5v5：人机由内核战术层生成 Command。
func _auto_tick(count: int) -> void:
	for i in count:
		main.sim.tick_auto()

## 验收：把玩家 0 放到 from，按内核 NavPath 航点实际走，检查是否到达 to（防“有路走不到”）。
func _walk_nav(step: Dictionary) -> void:
	var from: Array = step.get("from", [0, 0, 0])
	var to: Array = step.get("to", [0, 0, 0])
	var max_ticks: int = int(step.get("max_ticks", 900))
	var name := str(step.get("name", "walk"))
	main.sim.set_player_position(0, float(from[0]), float(from[1]), float(from[2]))
	main.sim.tick()
	var points: PackedVector3Array = main.sim.nav_points(
			float(from[0]), float(from[1]), float(from[2]),
			float(to[0]), float(to[1]), float(to[2]))
	if points.is_empty():
		failures.append("walk_nav %s: 无导航路径" % name)
		return
	var target := Vector3(float(to[0]), float(to[1]), float(to[2]))
	var index := 0
	var reached := false
	for i in max_ticks:
		var pos := Vector3(main.sim.get_x(0), main.sim.get_y(0), main.sim.get_z(0))
		if pos.distance_to(target) < 56.0:
			reached = true
			break
		while index < points.size() - 1 and pos.distance_to(points[index]) < 40.0:
			index += 1
		var waypoint: Vector3 = points[index]
		var yaw := atan2(waypoint.y - pos.y, waypoint.x - pos.x) * (180.0 / PI)
		main.sim.set_move(0, yaw, 0.0, 1.0, 0.0, false, false)
		main.sim.tick()
	if not reached:
		var last := Vector3(main.sim.get_x(0), main.sim.get_y(0), main.sim.get_z(0))
		var next: Vector3 = points[min(index, points.size() - 1)]
		failures.append("walk_nav %s: 未到达 to=%s last=%s index=%d next=%s points=%s" %
				[name, str(target), str(last), index, str(next), str(points)])

func _assert_nav(step: Dictionary) -> void:
	var from: Array = step.get("from", [0, 0, 0])
	var to: Array = step.get("to", [0, 0, 0])
	var points: int = int(main.sim.nav_path_points(
			float(from[0]), float(from[1]), float(from[2]),
			float(to[0]), float(to[1]), float(to[2])))
	var name := str(step.get("name", "nav"))
	var expect := str(step.get("expect", "path"))
	var ok := (points > 0) if expect == "path" else (points == 0)
	if not ok:
		failures.append("nav %s: expect %s, points=%d" % [name, expect, points])

func _check(checks: Array) -> void:
	for check: Dictionary in checks:
		var player: int = int(check.get("player", 0))
		var field: String = check.get("field", "")
		var actual: Variant = _read_field(player, field)
		if check.has("equals"):
			var expected: Variant = check.get("equals")
			if actual != expected:
				failures.append("player %d %s: expected %s got %s" %
						[player, field, str(expected), str(actual)])
		elif check.has("min"):
			if float(actual) < float(check.get("min")):
				failures.append("player %d %s: expected >= %s got %s" %
						[player, field, str(check.get("min")), str(actual)])
		elif check.has("max"):
			if float(actual) > float(check.get("max")):
				failures.append("player %d %s: expected <= %s got %s" %
						[player, field, str(check.get("max")), str(actual)])

func _read_field(player: int, field: String) -> Variant:
	match field:
		"health":
			return main.sim.get_health(player)
		"armor":
			return main.sim.get_armor(player)
		"mag":
			return main.sim.get_mag(player)
		"reserve":
			return main.sim.get_reserve(player)
		"alive":
			return main.sim.is_alive(player)
		"kills":
			return main.sim.get_kills(player)
		"money":
			return main.sim.get_money(player)
		"round_phase":
			return main.sim.get_round_phase()
		"round_ticks":
			return main.sim.get_round_ticks()
		"round_number":
			return main.sim.get_round_number()
		"player_count":
			return main.sim.get_player_count()
		"wall_count":
			return main.sim.get_wall_count()
		"has_bomb":
			return main.sim.get_has_bomb(player)
		"alive_count_t":
			return main.sim.get_alive_count(1)
		"alive_count_ct":
			return main.sim.get_alive_count(2)
		"score_t":
			return main.sim.get_score_t()
		"score_ct":
			return main.sim.get_score_ct()
		"x":
			return main.sim.get_x(player)
		"y":
			return main.sim.get_y(player)
		"z":
			return main.sim.get_z(player)
	push_error("未知字段: " + field)
	return null
