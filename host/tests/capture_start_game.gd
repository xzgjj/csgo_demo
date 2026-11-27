extends SceneTree
## 主菜单 / 开局页视觉采集：菜单 → 点开始 → 开局页。
## 用法：godot --path host --script res://tests/capture_start_game.gd -- <out_dir>

var main: Node
var out_dir: String


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	out_dir = args[0] if args.size() > 0 else ProjectSettings.globalize_path("res://captures/start_game")
	DirAccess.make_dir_recursive_absolute(out_dir)
	main = load("res://main.tscn").instantiate()
	root.add_child(main)
	_flow.call_deferred()


func _flow() -> void:
	main.set_physics_process(false)
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	_save("menu.png")
	main.menu.start_button.pressed.emit()
	await process_frame
	await RenderingServer.frame_post_draw
	_save("start_game.png")
	quit(0)


func _save(name: String) -> void:
	var image: Image = root.get_texture().get_image()
	var path := out_dir.path_join(name)
	image.save_png(path)
	print("CAPTURE ", path, " ", image.get_width(), "x", image.get_height())
