extends SceneTree
## de_dust2 灰盒接入冒烟（headless）：只验证「新图能进内核」，不改默认 de_ash。
## 用法：& godot --headless --path host --script res://tests/dust2_probe.gd
## 断言：清单存在 / 盒子数与碰撞数 / set_map_boxes 写入数一致 / 8 tick 不崩 / 出生点可用。

const MapLoader = preload("res://scripts/map_loader.gd")
const PATH := "res://scenes/de_dust2_boxes.json"

var failures: Array[String] = []


func _initialize() -> void:
	var text := FileAccess.get_file_as_string(PATH)
	_expect(not text.is_empty(), "缺少 %s" % PATH)
	if text.is_empty():
		_quit()
		return
	var data: Variant = JSON.parse_string(text)
	_expect(typeof(data) == TYPE_DICTIONARY, "清单不是字典")
	var boxes: Array = data.get("boxes", [])
	var collision := 0
	for item: Dictionary in boxes:
		if bool(item.get("collision", true)):
			collision += 1
	print("PROBE boxes=%d collision=%d" % [boxes.size(), collision])

	var sim = RecoilSim.new()
	sim.reset()
	var written: int = MapLoader.apply_map(sim, "de_dust2")
	print("PROBE registry default=%s dust2_boxes=%s" % [MapLoader.default_map_id(), MapLoader.boxes_path_for("de_dust2")])
	print("PROBE set_map_boxes written=%d" % written)
	_expect(written == collision, "写入 %d != 碰撞盒 %d（容量或非法盒）" % [written, collision])

	sim.begin_match()
	var start_x: float = sim.get_x(0)
	var start_y: float = sim.get_y(0)
	sim.set_move(0, 0.0, 0.0, 1.0, 0.0, false, false)
	for i in 8:
		sim.tick()
	var moved: float = absf(sim.get_x(0) - start_x) + absf(sim.get_y(0) - start_y)
	print("PROBE spawn=(%.0f,%.0f) moved8=%.2f tick_ok" % [start_x, start_y, moved])
	_expect(sim.get_player_count() >= 2, "玩家数应 >= 2，实际 %d" % sim.get_player_count())
	_expect(moved >= 0.0, "tick 后坐标异常")

	# 导航：四组出生↔包点必须都能出路径（围合后不得断路）
	var nav_pairs: Array = [
		["T->A", -420.0, -1900.0, 0.0, 1470.0, 1400.0, 128.0],
		["T->B", -420.0, -1900.0, 0.0, -1500.0, 1771.0, 64.0],
		["CT->A", 380.0, 1420.0, 0.0, 1470.0, 1400.0, 128.0],
		["CT->B", 380.0, 1420.0, 0.0, -1500.0, 1771.0, 64.0],
	]
	for pair: Array in nav_pairs:
		var points: int = sim.nav_path_points(pair[1], pair[2], pair[3], pair[4], pair[5], pair[6])
		print("PROBE nav %s points=%d" % [pair[0], points])
		_expect(points >= 2, "nav %s 路径点数 %d 不足（围合断路）" % [pair[0], points])
	var nav_count: int = sim.nav_path_points(-420.0, -1900.0, 0.0, 1470.0, 1400.0, 128.0)
	print("PROBE nav_points=%d" % nav_count)
	_expect(nav_count >= 2, "nav 路径点数 %d 不足" % nav_count)

	# 可破坏（MAP-10）：14 个 HP>0 的盒；玻璃 20 HP 打碎后墙数减一，非法索引被拒。
	var wall_count: int = sim.get_wall_count()
	var breakables := 0
	var glass_index := -1
	for index in wall_count:
		var hp: int = sim.get_wall_hp(index)
		if hp > 0:
			breakables += 1
		if hp == 20 and glass_index < 0:
			glass_index = index
	print("PROBE breakables=%d glass_index=%d" % [breakables, glass_index])
	_expect(breakables == 14, "可破坏盒应为 14，实际 %d" % breakables)
	_expect(glass_index >= 0, "未找到 20 HP 玻璃盒")
	if glass_index >= 0:
		var result: int = sim.damage_wall(glass_index, 20)
		_expect(result == 2, "玻璃 20HP 应被击碎（Destroyed=2），实际 %d" % result)
		_expect(sim.get_wall_count() == wall_count - 1,
				"击碎后墙数应减 1：%d -> %d" % [wall_count, sim.get_wall_count()])
	_expect(sim.damage_wall(9999, 1) == 3, "非法墙索引应返回 InvalidIndex=3")

	# tick 预算：先跳过冻结期（约 5s=300 tick），再测 1000 tick 稳态耗时与 p99
	for i in 300:
		sim.tick_auto()
	var samples: Array[float] = []
	for i in 1000:
		var tick_start: int = Time.get_ticks_usec()
		sim.tick_auto()
		samples.append(float(Time.get_ticks_usec() - tick_start) / 1000.0)
	var avg_ms := 0.0
	for sample in samples:
		avg_ms += sample
	avg_ms /= samples.size()
	samples.sort()
	var p99_ms: float = samples[int(samples.size() * 0.99)]
	print("PROBE tick_avg_ms=%.3f tick_p99_ms=%.3f" % [avg_ms, p99_ms])
	_expect(avg_ms < 8.0, "tick 均值 %.2fms 超过 8ms 预算" % avg_ms)
	_expect(p99_ms < 8.0, "tick p99 %.2fms 超过 8ms 预算" % p99_ms)
	_write_tick_csv(samples, avg_ms, p99_ms)
	_quit()


func _write_tick_csv(samples: Array[float], avg_ms: float, p99_ms: float) -> void:
	var out_dir := ProjectSettings.globalize_path("res://../log/map")
	DirAccess.make_dir_recursive_absolute(out_dir)
	var file := FileAccess.open(out_dir.path_join("dust2_tick.csv"), FileAccess.WRITE)
	file.store_line("tick,ms")
	for index in samples.size():
		file.store_line("%d,%.3f" % [index, samples[index]])
	file.store_line("avg,%.3f" % avg_ms)
	file.store_line("p99,%.3f" % p99_ms)
	file.close()
	print("PROBE tick_csv=", out_dir.path_join("dust2_tick.csv"))


func _expect(ok: bool, message: String) -> void:
	if not ok:
		failures.append(message)
		print("FAIL ", message)


func _quit() -> void:
	if failures.is_empty():
		print("dust2_probe PASS")
	else:
		print("dust2_probe FAIL ", failures.size())
	quit(0 if failures.is_empty() else 1)






