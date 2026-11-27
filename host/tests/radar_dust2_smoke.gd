extends SceneTree
## MAP-07 de_dust2 雷达契约：4600 图幅、≥100 轮廓盒、2 包点、包点落在框内。
## 用法：godot --headless --path host --script res://tests/radar_dust2_smoke.gd
##     加 -- --capture <dir> 输出 radar_dust2.png。

const RadarScript = preload("res://scripts/radar.gd")
const MapLoader = preload("res://scripts/map_loader.gd")
var failures: Array[String] = []
var checks := 0
var _capture_dir := ""


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	for index in args.size():
		var text := str(args[index])
		if text == "--capture" and index + 1 < args.size():
			_capture_dir = str(args[index + 1])
	_run.call_deferred()


func _run() -> void:
	var radar: Control = RadarScript.new()
	root.add_child(radar)
	radar.set_boxes_path("res://scenes/de_dust2_boxes.json")
	radar.build()
	await process_frame
	var world: Rect2 = radar.world_rect()
	_expect(world.size.x > 4300.0 and world.size.x < 4900.0,
			"dust2 雷达世界宽度应约 4600，实际 %.1f" % world.size.x)
	_expect(world.size.y > 4300.0 and world.size.y < 4900.0,
			"dust2 雷达世界高度应约 4600，实际 %.1f" % world.size.y)
	_expect(radar.map_wall_count() >= 100,
			"dust2 轮廓盒数应 >=100（实际 %d），不能被过滤成实心块" % radar.map_wall_count())
	_expect(radar.site_count() == 2, "dust2 雷达应画出 2 个包点，实际 %d" % radar.site_count())
	var bounds: Rect2 = radar.map_rect()
	for pos: Vector2 in radar.site_positions():
		_expect(bounds.has_point(pos), "包点 %s 应落在雷达框内" % str(pos))
	if not _capture_dir.is_empty():
		await _capture(radar)
	print("RADAR_DUST2 checks=%d failures=%d" % [checks, failures.size()])
	if failures.is_empty():
		print("RADAR_DUST2 PASS")
		quit(0)
		return
	for item in failures:
		print("RADAR_FAIL ", item)
	quit(1)


func _capture(radar: Control) -> void:
	var sim: RefCounted = RecoilSim.new()
	sim.reset()
	MapLoader.start_match(sim, "de_dust2")
	sim.set_player_position(0, -565.0, -1835.0, 0.0)
	sim.set_player_position(1, 300.0, 700.0, 0.0)
	radar.refresh(sim, 90.0)
	DirAccess.make_dir_recursive_absolute(_capture_dir)
	await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = root.get_texture().get_image()
	var path := _capture_dir.path_join("radar_dust2.png")
	image.save_png(path)
	print("RADAR_CAPTURE ", path, " ", image.get_width(), "x", image.get_height())


func _expect(ok: bool, message: String) -> void:
	checks += 1
	if not ok:
		failures.append(message)
