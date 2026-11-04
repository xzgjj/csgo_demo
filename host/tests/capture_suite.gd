extends SceneTree
## 运行时截图套件（非编辑器、窗口化 CLI；供统一验证管线 run_all.py 使用）。
## 用法：godot --path host --script res://tests/capture_suite.gd -- <out_dir>
## 产出：menu/spawn/gameplay/fire/weapon_0..2/scoreboard/pause/settings 共 11 张，
## 并打印 RUNTIME_META（世界来源/GLB 使用情况/武器列表）供断言。

var main: Node
var out_dir: String


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	out_dir = args[0] if args.size() > 0 else ProjectSettings.globalize_path("res://captures/runtime")
	DirAccess.make_dir_recursive_absolute(out_dir)
	main = load("res://main.tscn").instantiate()
	root.add_child(main)
	_flow.call_deferred()


func _flow() -> void:
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	_save("menu.png")

	main._start_match()
	main.yaw = 90.0
	main.pitch = 0.0
	main.viewmodel.set_process(false)
	for i in 5:
		await process_frame
	await _shot("spawn.png")

	main.sim.set_move(0, 90.0, 0.0, 1.0, 0.0, false, false)
	for i in 40:
		main.sim.tick_auto()
	main._update_view()
	main._update_hud()
	await _shot("gameplay.png")

	main.sim.fire(0, 90.0, 0.0)
	main.sim.tick_auto()
	main.viewmodel.kick()
	main._update_view()
	main._update_hud()
	await _shot("fire.png")

	for weapon_index in range(3):
		main._switch_weapon(weapon_index)
		await _shot("weapon_%d.png" % weapon_index)

	print("RUNTIME_META ", JSON.stringify({
		"world": str(main.world.source),
		"glb": main.viewmodel.used_glb,
		"weapons": main.viewmodel.weapon_ids(),
		"player_count": int(main.sim.get_player_count()),
	}))

	main.toggle_scoreboard()
	await _shot("scoreboard.png")
	main.toggle_scoreboard()

	main._show_pause()
	await _shot("pause.png")
	main._resume()

	main.show_settings_page()
	await _shot("settings.png")
	main.close_settings_page()

	quit(0)


func _shot(name: String) -> void:
	main.yaw = 90.0
	main.pitch = 0.0
	main.viewmodel.freeze_pose()
	main._update_view()
	main._update_hud()
	await process_frame
	await process_frame
	await RenderingServer.frame_post_draw
	_save(name)


func _save(name: String) -> void:
	var image: Image = root.get_texture().get_image()
	image.save_png(out_dir.path_join(name))
	print("RUNTIME_CAPTURE ", name, " ", image.get_width(), "x", image.get_height())
