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
	# 截图确定性：冻结物理循环（否则 sim 按墙钟推进，计时/bot 位置逐次漂移），
	# 需要推进对局的地方显式调 sim.tick_auto()。
	main.set_physics_process(false)
	main.yaw = 90.0
	main.pitch = 0.0
	main.viewmodel.set_process(false)
	for i in 5:
		await process_frame
	await _shot("spawn.png")
# 模块 7：副武器（手枪）手模可见切换（bug2 回归的截图证据）
	main.sim.switch_weapon(0, 1)
	main.sim.tick()
	main._update_view()
	main._update_hud()
	await _shot("weapon_pistol.png")
	main.sim.switch_weapon(0, 0)
	main.sim.tick()
	main._update_view()
	main._update_hud()

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

# 三把枪模型展示机位（关闭手模自动同步，避免被内核当前武器覆盖）
	main.purchase.auto_sync = false
	for weapon_index in range(3):
		main.viewmodel.select_weapon(["ak", "m4", "awp"][weapon_index])
		await _shot("weapon_%d.png" % weapon_index)
	main.purchase.auto_sync = true

	# 模块 8：AWP 4x 开镜（镜筒遮罩 + 倍率文字）+ AK 机瞄（只提示不遮罩）。
	main.viewmodel.select_weapon("awp")
	main.playing = false
	main.aiming = true
	main.scope.sync(true, 4.0, main.viewmodel.display_name())
	main.camera.fov = float(main.settings.fov) / 4.0
	await _shot("scope.png")
	main.viewmodel.select_weapon("ak")
	main.scope.sync(true, 1.0, main.viewmodel.display_name())
	main.camera.fov = float(main.settings.fov) / 1.15
	await _shot("iron_sight.png")
	main.aiming = false
	main.playing = true
	main.scope.sync(false, 1.0, main.viewmodel.display_name())

	# 模块 4：雷达开/关两帧（M 开关的产品态）。
	await _shot("radar_on.png")
	main.radar.set_open(false)
	await _shot("radar_off.png")
	main.radar.set_open(true)

	# 模块 5：T bot 持 AK（侧身），镜头在其右侧 → 可见持枪轮廓 + 开火枪口闪光
	main.bot_visuals.set_animating(false)
	for other in range(2, 5):
		main.sim.set_player_position(other, 2600.0, -2600.0, 0.0)
	main.sim.set_player_position(1, 0.0, -1700.0, 0.0)
	main.sim.set_move(1, 180.0, 0.0, 0.0, 0.0, false, false)
	main.sim.set_player_position(0, 0.0, -1630.0, 0.0)
	main.sim.tick()
	main._update_view()
	await _shot("bot_weapon.png", -90.0)
	main.sim.fire(1, 180.0, 0.0)
	main.sim.tick()
	main._update_view()
	main.bot_visuals.advance(0.0)
	await _shot("bot_fire.png", -90.0)

	# 模块 6：CT bot 摆到 T 出生点开阔地击杀 → 倒地 + 原地掉落主武器 → 靠近提示
	var victim := 5
	main.sim.set_player_position(victim, 0.0, -1600.0, 0.0)
	main.sim.set_player_position(0, 0.0, -1600.0, 0.0)
	main.sim.tick()
	for shot in 8:
		if not bool(main.sim.is_alive(victim)):
			break
		main.sim.fire(0, 0.0, 0.0)
		for wait in 8:
			main.sim.tick()
	main._update_view()
	main.sim.set_player_position(0, -100.0, -1760.0, 0.0)
	main.sim.tick()
	main._update_view()
	for step in 12:
		main.bot_visuals.advance(1.0 / 60.0)
	await _shot("drop_weapon.png", rad_to_deg(atan2(160.0, 100.0)))
	main.sim.set_player_position(0, 0.0, -1560.0, 0.0)
	main.sim.tick()
	main._update_view()
	await _shot("pickup_prompt.png", -90.0)
	# 回到 5v5 对局（后续 meta 断言 player_count==10）
	main.call("_start_match")
	main.set_physics_process(false)
	main.bot_visuals.set_animating(false)
	# 模块 9/10：购买菜单（800 起始金 → 手枪可买/步枪灰显）与物品栏页面。
	main.purchase.toggle()
	await _shot("buy_menu.png")
	main.buy_menu.close()
	main.show_loadout_page()
	await _shot("loadout.png")
	main.close_loadout_page()

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


func _shot(name: String, yaw_degrees: float = 90.0) -> void:
	main.yaw = yaw_degrees
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
