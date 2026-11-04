extends SceneTree
## 时间线驱动器：把“游玩时间”变成 tick 步进，产出可断言的时间线制品（不依赖真实时钟）。
## 用法（含断言/报告请走 tools/verify/run_timeline.py）：
##   godot [--headless] --path host --script res://tests/timeline_suite.gd -- <spec.json> <out_dir> [--visual]
## level=host 走 main._physics_process（含输入/HUD/相机）；level=sim 只推进内核（更快）。
## 制品：timeline.jsonl（采样）/ transitions.jsonl（阶段/回合边界）/ marks.jsonl / meta.json
## 1 秒 = 64 tick（host/project.godot: common/physics_ticks_per_second=64）。

const TICK_DELTA := 1.0 / 64.0

var main: Node
var spec: Dictionary
var out_dir: String
var visual := false
var level := "host"
var sample_every := 16
var tick := 0
var samples := PackedStringArray()
var transitions := PackedStringArray()
var marks := PackedStringArray()
var shots: Dictionary = {}
var started_usec := 0
var failures: Array[String] = []


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() < 2:
		push_error("用法: <spec.json> <out_dir> [--visual]")
		quit(2)
		return
	var text := FileAccess.get_file_as_string(args[0])
	var data: Variant = JSON.parse_string(text)
	if typeof(data) != TYPE_DICTIONARY:
		push_error("spec 非法: " + args[0])
		quit(2)
		return
	spec = data
	out_dir = args[1]
	visual = "--visual" in args
	level = str(spec.get("level", "host"))
	if level != "host" and level != "sim":
		push_error("level 只支持 host / sim: " + level)
		quit(2)
		return
	sample_every = maxi(1, int(spec.get("sample_every", 16)))
	DirAccess.make_dir_recursive_absolute(out_dir)
	for entry: Variant in spec.get("screenshots", []):
		var shot: Dictionary = entry
		var at := int(shot.get("tick", 0))
		var name := str(shot.get("name", "shot_%d" % at))
		if shots.has(at):
			var names: Array = shots[at]
			names.append(name)
		else:
			shots[at] = [name]
	main = load("res://main.tscn").instantiate()
	root.add_child(main)
	_run.call_deferred()


func _run() -> void:
	await process_frame
	if str(spec.get("mode", "match")) == "match":
		main._start_match()
	else:
		main._start_play()
	main.set_physics_process(false)
	if main.viewmodel != null:
		main.viewmodel.set_process(false)
	started_usec = Time.get_ticks_usec()
	_sample(false)
	for step: Dictionary in spec.get("script", []):
		var action := str(step.get("action", "auto_tick"))
		match action:
			"auto_tick":
				await _advance(true, int(step.get("ticks", 1)))
			"tick":
				await _advance(false, int(step.get("ticks", 1)))
			"wait_until":
				await _wait_until(step)
			"teleport":
				var who := int(step.get("player", 0))
				main.sim.set_player_position(who, float(step.get("x", 0.0)),
						float(step.get("y", 0.0)), float(step.get("z", 0.0)))
				marks.append(JSON.stringify({"tick": tick, "name": "teleport", "player": who}))
			"plant":
				main.sim.plant_bomb(int(step.get("player", 0)))
				await _advance(false, 1)
			"defuse":
				main.sim.defuse_bomb(int(step.get("player", 0)))
				await _advance(false, 1)
			"mark":
				marks.append(JSON.stringify({"tick": tick, "name": str(step.get("name", ""))}))
			_:
				failures.append("未知 action: " + action)
	_flush()


func _advance(auto: bool, count: int) -> void:
	for i in count:
		var phase_before := int(main.sim.get_round_phase())
		var round_before := int(main.sim.get_round_number())
		if level == "host":
			main._physics_process(TICK_DELTA)
		elif auto:
			main.sim.tick_auto()
		else:
			main.sim.tick()
		tick += 1
		_after_tick(phase_before, round_before)
		if visual and shots.has(tick):
			await _capture(tick)


func _after_tick(phase_before: int, round_before: int) -> void:
	var phase := int(main.sim.get_round_phase())
	var round_no := int(main.sim.get_round_number())
	var changed := phase != phase_before or round_no != round_before
	if changed:
		transitions.append(JSON.stringify({
			"tick": tick,
			"from": phase_before,
			"to": phase,
			"round_from": round_before,
			"round_to": round_no,
			"phase_ticks": int(main.sim.get_round_ticks()),
			"score_t": int(main.sim.get_score_t()),
			"score_ct": int(main.sim.get_score_ct()),
		}))
	if changed or tick % sample_every == 0:
		_sample(changed)


func _sample(force: bool) -> void:
	var players: Array = []
	var money: Array = []
	for i in int(main.sim.get_player_count()):
		players.append([
			int(roundf(float(main.sim.get_x(i)))),
			int(roundf(float(main.sim.get_y(i)))),
			int(roundf(float(main.sim.get_z(i)))),
			int(main.sim.get_health(i)),
			int(main.sim.get_mag(i)),
			1 if bool(main.sim.is_alive(i)) else 0,
		])
		money.append(int(main.sim.get_money(i)))
	samples.append(JSON.stringify({
		"tick": tick,
		"phase": int(main.sim.get_round_phase()),
		"round": int(main.sim.get_round_number()),
		"phase_ticks": int(main.sim.get_round_ticks()),
		"score_t": int(main.sim.get_score_t()),
		"score_ct": int(main.sim.get_score_ct()),
		"alive_t": int(main.sim.get_alive_count(1)),
		"alive_ct": int(main.sim.get_alive_count(2)),
		"bomb": [int(main.sim.get_bomb_planted()), int(main.sim.get_bomb_timer())],
		"p": players,
		"money": money,
	}))


func _wait_until(step: Dictionary) -> void:
	var field := str(step.get("field", "phase"))
	var expect := int(step.get("equals", 0))
	var max_ticks := int(step.get("max_ticks", 1000))
	var auto := bool(step.get("auto", true))
	for i in max_ticks:
		if _read_int(field) == expect:
			return
		await _advance(auto, 1)
	failures.append("wait_until 超时：%s != %d（%d tick）" % [field, expect, max_ticks])


func _read_int(field: String) -> int:
	match field:
		"phase":
			return int(main.sim.get_round_phase())
		"round":
			return int(main.sim.get_round_number())
		"phase_ticks":
			return int(main.sim.get_round_ticks())
		"score_t":
			return int(main.sim.get_score_t())
		"score_ct":
			return int(main.sim.get_score_ct())
		"alive_t":
			return int(main.sim.get_alive_count(1))
		"alive_ct":
			return int(main.sim.get_alive_count(2))
		"bomb_planted":
			return int(main.sim.get_bomb_planted())
		"bomb_timer":
			return int(main.sim.get_bomb_timer())
	failures.append("wait_until 未知字段: " + field)
	return -1


func _capture(at: int) -> void:
	main._update_view()
	main._update_hud()
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = root.get_texture().get_image()
	for shot_name: Variant in shots[at]:
		var path := out_dir.path_join("%s.png" % str(shot_name))
		image.save_png(path)
		print("TIMELINE_CAPTURE ", path)


func _flush() -> void:
	var wall_ms := (Time.get_ticks_usec() - started_usec) / 1000.0
	var meta := {
		"id": str(spec.get("id", "timeline")),
		"level": level,
		"mode": str(spec.get("mode", "match")),
		"ticks": tick,
		"sample_every": sample_every,
		"samples": samples.size(),
		"transitions": transitions.size(),
		"wall_ms": snappedf(wall_ms, 0.1),
		"ms_per_tick": snappedf(wall_ms / maxf(1.0, float(tick)), 0.0001),
		"player_count": int(main.sim.get_player_count()),
		"round": int(main.sim.get_round_number()),
		"phase": int(main.sim.get_round_phase()),
		"score_t": int(main.sim.get_score_t()),
		"score_ct": int(main.sim.get_score_ct()),
		"failures": failures,
	}
	_write("timeline.jsonl", samples)
	_write("transitions.jsonl", transitions)
	_write("marks.jsonl", marks)
	_write("meta.json", PackedStringArray([JSON.stringify(meta)]))
	print("TIMELINE_META ", JSON.stringify(meta))
	if failures.is_empty():
		print("TIMELINE PASS")
		quit(0)
	else:
		for failure in failures:
			print("TIMELINE_FAIL ", failure)
		quit(1)


func _write(name: String, lines: PackedStringArray) -> void:
	var file := FileAccess.open(out_dir.path_join(name), FileAccess.WRITE)
	if file == null:
		push_error("无法写入 " + name)
		return
	for line in lines:
		file.store_line(line)
	file.close()
