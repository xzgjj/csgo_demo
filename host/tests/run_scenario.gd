extends SceneTree
## 场景执行器：读取 ScenarioSpec，跑内核脚本 → 代码断言 → 截图检查点。
## 用法：
##   godot --path host --script res://tests/run_scenario.gd -- <scenario.json 绝对路径> <out_dir 绝对路径> [--visual]

var main: Node

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
	main._start_play()
	# 场景执行期间关闭自动物理 tick，保证状态只由脚本推进（截图可复现）。
	main.set_physics_process(false)
	for step: Dictionary in data.get("script", []):
		var action: String = step.get("action", "")
		if action == "move":
			main.sim.set_move(0, step.get("yaw", 0.0), step.get("pitch", 0.0),
					step.get("forward", 0.0), step.get("side", 0.0),
					step.get("jump", false), step.get("duck", false))
			for i in int(step.get("ticks", 1)):
				main.sim.tick()
		elif action == "fire":
			main.sim.fire(0, step.get("yaw", 0.0), step.get("pitch", 0.0))
			main.sim.tick()
		else:
			push_error("未知 action: " + action)

	# 代码断言
	var failures: Array = []
	for check: Dictionary in data.get("asserts", []):
		var player: int = int(check.get("player", 0))
		var field: String = check.get("field", "")
		var expected: Variant = check.get("equals")
		var actual: Variant = _read_field(player, field)
		if actual != expected:
			failures.append("player %d %s: expected %s got %s" % [player, field, str(expected), str(actual)])

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
	print("SCENARIO ", data.get("id", ""), " checks=", data.get("asserts", []).size(), " failures=", failures.size())
	quit(0 if failures.is_empty() else 1)

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
		"x":
			return main.sim.get_x(player)
		"y":
			return main.sim.get_y(player)
		"z":
			return main.sim.get_z(player)
	push_error("未知字段: " + field)
	return null
