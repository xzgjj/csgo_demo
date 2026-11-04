extends SceneTree
## 截图采集脚本：启动主场景 → 主菜单截图 → 进入游戏并推进内核 → 游戏内截图。
## 运行：godot --path host --script res://tests/capture.gd

var main: Node
var out_dir: String

func _initialize() -> void:
	out_dir = ProjectSettings.globalize_path("res://captures")
	DirAccess.make_dir_recursive_absolute(out_dir)
	main = load("res://main.tscn").instantiate()
	root.add_child(main)
	_capture_flow.call_deferred()

func _capture_flow() -> void:
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	_save("menu.png")

	main._start_play()
	main.sim.set_move(0, 0.0, 0.0, 1.0, 0.0, false, false)
	for i in 40:
		main.sim.tick()
	main.sim.fire(0, 0.0, 0.0)
	main.sim.tick()
	main._update_view()
	main._update_hud()
	await process_frame
	await RenderingServer.frame_post_draw
	_save("gameplay.png")
	quit(0)

func _save(name: String) -> void:
	var image: Image = root.get_texture().get_image()
	image.save_png(out_dir.path_join(name))
	print("CAPTURE ", out_dir.path_join(name), " ", image.get_width(), "x", image.get_height())
