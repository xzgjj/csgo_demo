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

	# 导航：T 出生 → A 包点 必须能出路径（nav 网格 ±2048，本图全部落在范围内）
	var nav_count: int = sim.nav_path_points(-420.0, -1900.0, 0.0, 1470.0, 1400.0, 128.0)
	print("PROBE nav_points=%d" % nav_count)
	_expect(nav_count >= 2, "nav 路径点数 %d 不足" % nav_count)

	# tick 预算：先跳过冻结期（约 5s=300 tick），再测 1000 tick 稳态耗时
	for i in 300:
		sim.tick_auto()
	var t0: int = Time.get_ticks_usec()
	for i in 1000:
		sim.tick_auto()
	var avg_ms: float = float(Time.get_ticks_usec() - t0) / 1000.0 / 1000.0
	print("PROBE tick_avg_ms=%.3f" % avg_ms)
	_expect(avg_ms < 8.0, "tick 均值 %.2fms 超过 8ms 预算" % avg_ms)
	_quit()


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






