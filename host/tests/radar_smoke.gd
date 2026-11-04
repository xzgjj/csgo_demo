extends SceneTree
## 模块 4 headless 契约测试：2D 雷达/小地图 + 右上角队友存活条（RadarPanel）。
## 用法：& godot --headless --path host --script res://tests/radar_smoke.gd
## 每个用例三动作齐全：准备（sim/输入）→ 执行（tick/移动/开关）→ 断言。
## 敌人暴露：内核 is_visible(viewer,target) 尚未落地 → 用契约替身 FakeSim 验证
## 「暴露前不显示 / 暴露后显示」；真实 sim 上只断言「无内核接口时一律不显示（pending）」。
## ⑤ 接线契约：不修改 main.gd，只在测试里模拟 Lead 的 3 行接线，验证真实 5v5 主场景下自刷新/M/隐藏。

const RadarScript = preload("res://scripts/radar.gd")

var checks := 0
var failures: Array[String] = []
var skipped := 0
var _capture_dir := ""


## 契约替身：与 RecoilSim 同名只读接口 + is_visible，用来验证雷达的暴露显示规则。
class FakeSim:
	extends RefCounted
	var teams: Array[int] = []
	var alive: Array[bool] = []
	var xs: Array[float] = []
	var ys: Array[float] = []
	var exposed: Array[bool] = []

	func get_player_count() -> int:
		return teams.size()

	func get_team(index: int) -> int:
		return teams[index]

	func is_alive(index: int) -> bool:
		return alive[index]

	func get_x(index: int) -> float:
		return xs[index]

	func get_y(index: int) -> float:
		return ys[index]

	func get_alive_count(team: int) -> int:
		var total := 0
		for index in teams.size():
			if teams[index] == team and alive[index]:
				total += 1
		return total

	func is_visible(_viewer: int, target: int) -> bool:
		return exposed[target]


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() >= 2 and args[0] == "--capture":
		_capture_dir = args[1]
	_run.call_deferred()


func _run() -> void:
	await process_frame
	var radar: Control = RadarScript.new()
	radar.name = "Radar"
	root.add_child(radar)
	radar.build()
	await process_frame
	_test_map_projection(radar)
	await _test_self_and_teammate_move(radar)
	await _test_squad_lamps(radar)
	_test_enemy_exposure(radar)
	await _test_toggle(radar)
	var probe: RefCounted = RecoilSim.new()
	if probe.has_method("is_visible"):
		_test_kernel_los(radar)
	else:
		print("RADAR_LOS skipped：内核尚无 is_visible")
	if not _capture_dir.is_empty():
		await _capture_standalone(radar)
	# 接线用例前释放独立面板实例，避免两个雷达同屏干扰 M 开关与截图（radar_off 会关不掉）。
	radar.queue_free()
	await process_frame
	await _test_main_wiring()
	print("RADAR_SMOKE checks=%d failures=%d skipped=%d" % [checks, failures.size(), skipped])
	if failures.is_empty():
		print("RADAR_SMOKE PASS")
		quit(0)
		return
	for item in failures:
		print("RADAR_FAIL ", item)
	quit(1)


## ①-a 准备：读 de_ash_boxes.json 投影；执行：坐标方向映射 + 中心点；断言：轮廓有效且落在雷达框内。
func _test_map_projection(radar: Control) -> void:
	var world: Rect2 = radar.world_rect()
	_expect(world.size.x > 1000.0 and world.size.y > 1000.0,
			"雷达世界范围应来自地图盒子（实际 %s）" % str(world.size))
	_expect(radar.map_wall_count() > 0, "地图轮廓盒数应 > 0")
	var rects: Array = radar.map_rects()
	_expect(rects.size() == radar.map_wall_count(), "轮廓盒数组长度应与计数一致")
	var bounds: Rect2 = radar.map_rect()
	var inside := true
	for item: Rect2 in rects:
		if not bounds.encloses(item):
			inside = false
			break
	_expect(inside, "所有轮廓盒都应落在雷达绘制框内（比例尺/投影一致）")
	var center := world.position + world.size * 0.5
	var mapped: Vector2 = radar.world_to_radar(center.x, center.y)
	_expect(mapped.distance_to(bounds.position + bounds.size * 0.5) < 0.5,
			"世界中心应映射到雷达中心，实际 %s" % str(mapped))
	var origin: Vector2 = radar.world_to_radar(0.0, 0.0)
	var plus_y: Vector2 = radar.world_to_radar(0.0, 200.0)
	var plus_x: Vector2 = radar.world_to_radar(200.0, 0.0)
	_expect(plus_y.x > origin.x + 0.5, "sim +y 应映射到雷达 +x（右）")
	_expect(plus_x.y < origin.y - 0.5, "sim +x 应映射到雷达 -y（上）")


## ①-b 准备：5v5 过冻结期并摆位；执行：自己前进 32 tick + 队友瞬移；断言：点位随坐标变化。
func _test_self_and_teammate_move(radar: Control) -> void:
	var sim: RefCounted = RecoilSim.new()
	sim.reset()
	sim.begin_match()
	for i in 200:
		sim.tick()
	sim.set_player_position(0, 0.0, 0.0, 0.0)
	sim.set_player_position(1, 100.0, 0.0, 0.0)
	sim.tick()
	radar.refresh(sim, 0.0)
	_expect(radar.has_marker(0), "自己应始终出现在雷达上")
	_expect(radar.has_marker(1), "存活的 T 队友应出现在雷达上")
	var own_before: Vector2 = radar.marker_position(0)
	var mate_before: Vector2 = radar.marker_position(1)
	# 执行：自己沿 +sim x 前进；只应由 tick 推动内核位移。
	sim.set_move(0, 0.0, 0.0, 1.0, 0.0, false, false)
	for i in 32:
		sim.tick()
	radar.refresh(sim, 0.0)
	var own_after: Vector2 = radar.marker_position(0)
	_expect(own_after.distance_to(own_before) > 0.5,
			"自己移动后雷达点应变化：%s -> %s" % [str(own_before), str(own_after)])
	_expect(own_after.y < own_before.y - 0.5,
			"朝 +sim x 前进时雷达点应向上移动：%s -> %s" % [str(own_before), str(own_after)])
	# 执行：队友瞬移到 +sim y 400；断言：雷达点向右移动。
	sim.set_player_position(1, 100.0, 400.0, 0.0)
	sim.tick()
	radar.refresh(sim, 0.0)
	var mate_after: Vector2 = radar.marker_position(1)
	_expect(mate_after.distance_to(mate_before) > 0.5,
			"队友移动后雷达点应变化：%s -> %s" % [str(mate_before), str(mate_after)])
	_expect(mate_after.x > mate_before.x + 0.5,
			"队友朝 +sim y 移动时雷达点应向右：%s -> %s" % [str(mate_before), str(mate_after)])
	# 朝向：箭头角度应随 yaw 旋转 90°。
	radar.refresh(sim, 0.0)
	var heading_0: float = radar.own_heading_degrees()
	radar.refresh(sim, 90.0)
	var heading_90: float = radar.own_heading_degrees()
	var delta := wrapf(heading_90 - heading_0, -180.0, 180.0)
	_expect(absf(delta - 90.0) < 0.5, "自己箭头朝向应随 yaw 旋转 90°，实际 %.2f" % delta)


## ② 准备：5v5 冻结期（出生点）→ 执行过冻结期到 Live → CT 5 击杀 T 队友 1；
## 断言：冻结期 5 灯全亮、Live 期一致、击杀后 1 灯灭且灯数与 alive_count 一致。
func _test_squad_lamps(radar: Control) -> void:
	var sim: RefCounted = RecoilSim.new()
	sim.reset()
	sim.begin_match()
	# 准备：冻结期/出生点就应显示完整 5 人存活条（模块 1 HUD 项依赖这条）。
	radar.refresh(sim, 0.0)
	var team: int = int(sim.get_team(0))
	var lamps: Array = radar.squad_lamps()
	_expect(lamps.size() == 5, "存活条应固定 5 盏灯，实际 %d" % lamps.size())
	_expect(radar.squad_size() == 5, "5v5 出生点应有 5 名 T 队员，实际 %d" % radar.squad_size())
	_expect(_lit_lamps(lamps) == 5, "冻结期 5 名队友应 5 灯全亮，实际 %d" % _lit_lamps(lamps))
	_expect(radar.squad_alive_count() == 5, "冻结期存活数应为 5，实际 %d" % radar.squad_alive_count())
	_expect(radar.squad_alive_count() == int(sim.get_alive_count(team)),
			"冻结期灯数 %d 应等于 alive_count %d" % [radar.squad_alive_count(),
			int(sim.get_alive_count(team))])
	# 执行：过冻结期到 Live（Fire 在冻结期会被拒）。
	for i in 200:
		sim.tick()
	radar.refresh(sim, 0.0)
	_expect(_lit_lamps(radar.squad_lamps()) == int(sim.get_alive_count(team)),
			"Live 期灯数 %d 应等于 alive_count %d" % [_lit_lamps(radar.squad_lamps()),
			int(sim.get_alive_count(team))])
	# 执行：T 1 站到 (400,0,0)，CT 5 从 (0,0,0) 沿 +sim x 连射（友军不受伤 → 必须由敌方造成）。
	sim.set_player_position(0, -600.0, 600.0, 0.0)
	sim.set_player_position(1, 400.0, 0.0, 0.0)
	sim.set_player_position(5, 0.0, 0.0, 0.0)
	var shots := 0
	while sim.is_alive(1) and shots < 16:
		sim.fire(5, 0.0, 0.0)
		for i in 8:
			sim.tick()
		shots += 1
	_expect(not bool(sim.is_alive(1)), "敌方应能击杀 T 队友 1（实际 %d 枪）" % shots)
	radar.refresh(sim, 0.0)
	lamps = radar.squad_lamps()
	_expect(lamps.size() == 5, "阵亡后存活条仍应固定 5 盏灯")
	_expect(not bool(lamps[1]), "阵亡队友 1 的灯应熄灭")
	_expect(_lit_lamps(lamps) == 4, "击杀 1 人后应剩 4 灯，实际 %d" % _lit_lamps(lamps))
	_expect(radar.squad_alive_count() == 4, "击杀 1 人后存活数应为 4，实际 %d"
			% radar.squad_alive_count())
	_expect(_lit_lamps(lamps) == int(sim.get_alive_count(team)),
			"阵亡后灯数 %d 应等于 alive_count %d" % [_lit_lamps(lamps), int(sim.get_alive_count(team))])
	_expect(not radar.has_marker(1), "阵亡队友不应再画雷达点")


## ③ 准备：替身 sim（含 is_visible）；执行：切换暴露状态；断言：暴露前隐藏 / 暴露后显示。
func _test_enemy_exposure(radar: Control) -> void:
	var fake := FakeSim.new()
	fake.teams = [1, 1, 2]
	fake.alive = [true, true, true]
	fake.xs = [0.0, 100.0, 300.0]
	fake.ys = [0.0, 0.0, 0.0]
	fake.exposed = [false, false, false]
	radar.refresh(fake, 0.0)
	_expect(radar.exposure_source() == "kernel", "有 is_visible 时应走内核可见性，实际 %s"
			% radar.exposure_source())
	_expect(radar.has_marker(0) and radar.has_marker(1), "自己/队友应始终有点")
	_expect(not radar.has_marker(2), "未暴露的敌人不应显示")
	_expect(radar.visible_enemies().is_empty(), "未暴露时敌人集合应为空")
	# 执行：敌人被暴露；断言：显示且进入敌人集合。
	fake.exposed[2] = true
	radar.refresh(fake, 0.0)
	_expect(radar.has_marker(2), "暴露后的敌人应显示")
	_expect(radar.visible_enemies() == [2], "暴露集合应含敌人 2，实际 %s"
			% str(radar.visible_enemies()))
	# 真实内核：is_visible 未落地时，敌人一律不显示（不写假显示）。
	var sim: RefCounted = RecoilSim.new()
	sim.reset()
	sim.begin_match()
	radar.refresh(sim, 0.0)
	if sim.has_method("is_visible"):
		_expect(radar.exposure_source() == "kernel", "真实内核有 is_visible 时应走内核分支")
		for index in range(5, 10):
			_expect(radar.has_marker(index) == radar.is_exposed(0, index),
					"敌人 %d 的显示应与 is_visible 一致" % index)
	else:
		_expect(radar.exposure_source() == "pending",
				"内核缺 is_visible 时来源应为 pending，实际 %s" % radar.exposure_source())
		_expect(radar.visible_enemies().is_empty(), "内核缺 is_visible 时不应显示任何敌人")
		print("RADAR_EXPOSURE pending-kernel（等 kernel-weapons 的 is_visible 落地后自动切换）")


## ④ 准备：独立雷达（未绑 main）初始打开；执行：M 键两次 + toggle()；断言：visible 同步切换。
func _test_toggle(radar: Control) -> void:
	radar.set_open(true)
	await process_frame
	_expect(radar.is_open() and bool(radar.visible), "雷达初始应打开且 visible=true")
	_press_key(KEY_M)
	await process_frame
	await process_frame
	_expect(not bool(radar.is_open()), "M 应关闭雷达")
	_expect(not bool(radar.visible), "关闭后 visible 应为 false")
	_press_key(KEY_M)
	await process_frame
	await process_frame
	_expect(bool(radar.is_open()) and bool(radar.visible), "再次 M 应重新打开雷达")
	radar.toggle()
	await process_frame
	_expect(not bool(radar.visible), "toggle() 应同步 visible=false")
	radar.toggle()
	await process_frame
	_expect(bool(radar.visible), "toggle() 应能恢复 visible=true")


## ③-b 真实内核 LOS（is_visible 落地后才跑）：空场对穿 → 可见；中间加墙 → 遮蔽。
func _test_kernel_los(radar: Control) -> void:
	var sim: RefCounted = RecoilSim.new()
	sim.reset()
	sim.set_player_position(0, 0.0, 0.0, 0.0)
	sim.set_player_position(1, 400.0, 0.0, 0.0)
	radar.refresh(sim, 0.0)
	_expect(radar.exposure_source() == "kernel", "有内核 is_visible 时暴露来源应为 kernel，实际 %s"
			% radar.exposure_source())
	_expect(radar.is_exposed(0, 1), "空场对穿时敌人应可见（内核 HasLineOfSight）")
	_expect(radar.has_marker(1), "空场对穿时敌人应显示在雷达上")
	# 执行：在两人之间放一道 sim AABB 墙（x 190..210，y -100..100，z 0..200）。
	sim.set_map_boxes(PackedFloat32Array([190.0, -100.0, 0.0, 210.0, 100.0, 200.0]))
	radar.refresh(sim, 0.0)
	_expect(not radar.is_exposed(0, 1), "中间有墙时敌人不应可见")
	_expect(not radar.has_marker(1), "被墙遮挡的敌人不应显示在雷达上")


## ⑤ 准备：真实 main.tscn + 5v5；执行：按 Lead 接线方式（hud.add_child + setup(self)）挂雷达；
## 断言：自刷新可见/有点位、M 开关、回主菜单后隐藏（不改 main.gd，只模拟接线）。
func _test_main_wiring() -> void:
	var main: Node = load("res://main.tscn").instantiate()
	root.add_child(main)
	await process_frame
	if not main.has_method("_start_match") or main.get("hud") == null:
		# main.gd 依赖的 class_name（ScopeController/MapLoader/RadarPanel…）未进 .godot 类缓存时
		# 整个脚本解析失败；此处显式报 SKIP（计入 skipped），不伪装成通过。
		print("RADAR_WIRING SKIP：main.tscn 脚本未加载（main.gd 解析失败/类缓存未刷新），接线用例本轮跳过")
		skipped += 1
		main.queue_free()
		return
	main.call("_start_match")
	await process_frame
	# 优先用 host_pages 已接线实例 main.radar（Lead 的接线方式）；未接线时按报告里的 3 行手动挂。
	var radar: Control = main.get("radar") as Control
	if radar == null:
		radar = RadarScript.new()
		radar.name = "RadarWired"
		main.hud.add_child(radar)
		radar.setup(main)
		print("RADAR_WIRING 手动挂载（main.radar 不存在）")
	else:
		print("RADAR_WIRING 使用 host_pages 已接线实例 main.radar")
		_expect(radar.get_parent() == main.hud, "main.radar 应挂在 hud 下（host_pages 接线）")
	await process_frame
	await process_frame
	_expect(bool(radar.visible), "对局中雷达应可见（_process 自刷新）")
	_expect(radar.has_marker(0), "接线后自己应有雷达点")
	_expect(radar.marker_positions().size() >= 1, "接线后雷达应有点位数据")
	_expect(radar.squad_size() == 5, "5v5 存活条应有 5 名队员，实际 %d" % radar.squad_size())
	_expect(radar.squad_alive_count() == int(main.sim.get_alive_count(int(main.sim.get_team(0)))),
			"对局中存活条计数 %d 应等于 alive_count %d" % [radar.squad_alive_count(),
			int(main.sim.get_alive_count(int(main.sim.get_team(0))))])
	# 截图建议机位（Lead 接 golden）：radar_on / radar_off 两帧；用 --capture <dir> 窗口化跑。
	if not _capture_dir.is_empty():
		# 把 T 队友铺到中区，截图能看出队友点位与白箭头的层次（否则全叠在出生点）。
		main.sim.set_player_position(1, 300.0, -200.0, 0.0)
		main.sim.set_player_position(2, 260.0, -260.0, 0.0)
		main.sim.set_player_position(3, -260.0, -150.0, 0.0)
		await process_frame
		await process_frame
		await _capture("radar_on")
		radar.set_open(false)
		await _capture("radar_off")
		radar.set_open(true)
	_press_key(KEY_M)
	await process_frame
	await process_frame
	_expect(not bool(radar.visible), "真实对局中 M 应关闭雷达")
	_press_key(KEY_M)
	await process_frame
	await process_frame
	_expect(bool(radar.visible), "真实对局中再次 M 应打开雷达")
	main.call("_show_menu")
	await process_frame
	await process_frame
	_expect(not bool(radar.visible), "返回主菜单后雷达应隐藏")


## 独立面板截图（不依赖 main.gd）：5v5 真实 sim + 固定 yaw，供设计核对白箭头/点位/灯条。
func _capture_standalone(radar: Control) -> void:
	var sim: RefCounted = RecoilSim.new()
	sim.reset()
	sim.begin_match()
	for i in 200:
		sim.tick()
	sim.set_player_position(0, 0.0, 0.0, 0.0)
	sim.set_player_position(1, 300.0, -200.0, 0.0)
	sim.set_player_position(2, 260.0, -260.0, 0.0)
	radar.refresh(sim, 90.0)
	radar.set_open(true)
	await _capture("radar_panel_only")


func _capture(shot_name: String) -> void:
	DirAccess.make_dir_recursive_absolute(_capture_dir)
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = root.get_texture().get_image()
	var path := _capture_dir.path_join(shot_name + ".png")
	image.save_png(path)
	print("RADAR_CAPTURE ", path, " ", image.get_width(), "x", image.get_height())


func _press_key(code: Key) -> void:
	var event := InputEventKey.new()
	event.physical_keycode = code
	event.keycode = code
	event.pressed = true
	Input.parse_input_event(event)


func _lit_lamps(lamps: Array) -> int:
	var total := 0
	for state: bool in lamps:
		if state:
			total += 1
	return total


func _expect(condition: bool, message: String) -> void:
	checks += 1
	if not condition:
		failures.append(message)
