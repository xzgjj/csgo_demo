extends SceneTree
## 冒烟（headless 必须退出 0）：
## 1) 内核：移动 / 开火 / 上一 tick 与累计事件计数；
## 2) de_ash：主场景盒子数 > 0、包点/出生锚点 2 个、缺清单时回退灰盒；
## 3) 页面：主菜单 → 对局 →（Esc）暂停 → 设置 → 返回；Tab 比分板；回合/比赛结算；
## 4) 设置持久化：写 user://settings.cfg → 新实例读回、越界钳制、页面改动实时生效。

var failures: Array[String] = []
var main: Node


func _initialize() -> void:
	_sim_checks()
	_page_checks.call_deferred()


func _sim_checks() -> void:
	var sim = RecoilSim.new()
	sim.reset()
	var start_x: float = sim.get_x(0)
	sim.set_move(0, 0.0, 0.0, 1.0, 0.0, false, false)
	for i in 64:
		sim.tick()
	var moved: float = sim.get_x(0) - start_x
	sim.fire(0, 0.0, 0.0)
	sim.tick()
	var damage_events: int = sim.get_last_damage_events()
	var fire_events: int = sim.get_last_fire_events()
	var target_hp: int = sim.get_health(1)
	for i in 5:
		sim.tick()
	var total_damage: int = sim.get_total_damage_events()
	var total_fire: int = sim.get_total_fire_events()
	print("SMOKE sim moved=%.2f hp0=%d mag=%d hp1=%d dmg=%d fire=%d total_dmg=%d total_fire=%d" % [
		moved, sim.get_health(0), sim.get_mag(0), target_hp,
		damage_events, fire_events, total_damage, total_fire])
	_expect(moved > 100.0, "前进 64 tick 应位移 > 100，实际 %.2f" % moved)
	_expect(target_hp == 72, "靶子 HP 应为 72，实际 %d" % target_hp)
	_expect(damage_events == 1 and fire_events == 1, "上一 tick 事件计数应为 1/1")
	_expect(total_damage == 1 and total_fire == 1, "累计事件计数应为 1/1")


func _page_checks() -> void:
	main = load("res://main.tscn").instantiate()
	root.add_child(main)
	await process_frame
	_check_de_ash()
	_check_bindings()
	_check_menu_and_pause()
	_check_settings_persistence()
	_check_c4_hud()
	_check_scoreboard_and_results()
	_check_kill_feed()
	_check_match_end_flow()
	print("SMOKE pages failures=%d" % failures.size())
	for failure: String in failures:
		print("SMOKE FAIL ", failure)
	quit(0 if failures.is_empty() else 1)


func _check_de_ash() -> void:
	_expect(main.world != null, "世界节点应存在")
	_expect(main.world.object_count > 0, "de_ash 对象数应 > 0，实际 %d" % main.world.object_count)
	_expect(main.world.source == "de_ash", "应加载 de_ash，实际 %s" % main.world.source)
	_expect(main.world.site_anchors.size() == 2, "包点锚点应为 2")
	_expect(main.world.spawn_anchors.size() == 2, "出生锚点应为 2")
	var fallback := DeAshWorld.new()
	var loaded: bool = fallback.build("res://scenes/missing_boxes.json")
	_expect(not loaded, "清单缺失时 build() 应返回 false")
	_expect(fallback.source == "fallback" and fallback.object_count > 0, "应回退到灰盒竞技场")
	fallback.free()


func _check_bindings() -> void:
	_expect(main.sim.has_method("plant_bomb") and main.sim.has_method("defuse_bomb"),
			"宿主应绑定 plant_bomb/defuse_bomb")
	_expect(main.sim.has_method("get_bomb_planted") and main.sim.has_method("get_bomb_timer"),
			"宿主应绑定 C4 查询")
	_expect(main.sim.has_method("get_helmet") and main.sim.has_method("get_team")
			and main.sim.has_method("get_player_count"), "宿主应绑定头盔/队伍/玩家数")
	_expect(main.sim.get_player_count() == 2, "demo 玩家数应为 2，实际 %d"
			% main.sim.get_player_count())
	_expect(main.sim.get_team(0) == 1 and main.sim.get_team(1) == 2, "队伍应为 P0=T / P1=CT")
	_expect(main.sim.get_helmet(0) == 0, "初始不应有头盔")


func _check_menu_and_pause() -> void:
	_expect(main.menu.visible and not main.pause_page.visible and not main.settings_page.visible,
			"启动应停在主菜单")
	_expect(main.menu.button_texts().size() == 3, "主菜单应有 3 个按钮")
	main._start_play()
	_expect(main.playing and not main.menu.visible and main.hud.visible, "开始游戏应进入对局")
	main._update_hud()
	_expect(main.hud.phase_label.text.contains("冻结期"), "HUD 应显示冻结期")
	_expect(main.hud.timer_label.text.length() == 5, "HUD 倒计时应为 mm:ss，实际 %s"
			% main.hud.timer_label.text)
	_expect(main.hud.money_label.text.begins_with("$"), "HUD 应显示金钱")
	_expect(main.hud.armor_label.text.contains("护甲")
			and main.hud.armor_label.text.contains("头盔 无"), "HUD 应显示护甲/头盔（初始无头盔）")
	_expect(main.hud.c4_label.text == "C4 未安放", "HUD 初始 C4 状态应为未安放")
	main._show_pause()
	_expect(main.paused and main.pause_page.visible, "Esc 应暂停并显示暂停页")
	main.show_settings_page()
	_expect(main.settings_page.visible, "暂停页应能打开设置")
	main.close_settings_page()
	_expect(main.pause_page.visible and not main.settings_page.visible, "设置应返回暂停页")
	main._resume()
	_expect(main.playing and not main.pause_page.visible, "恢复后应回到对局")
	main._show_menu()
	_expect(not main.playing and main.menu.visible, "应能返回主菜单")
	main.show_settings_page()
	_expect(main.settings_page.visible, "主菜单应能打开设置")
	main.close_settings_page()
	_expect(main.menu.visible and not main.settings_page.visible, "设置应返回主菜单")


func _check_settings_persistence() -> void:
	var store := SettingsStore.new()
	store.fov = 100.0
	store.sensitivity = 0.125
	store.volume = 55
	_expect(store.save_settings(), "设置应写入 user://settings.cfg")
	var reloaded := SettingsStore.new()
	_expect(reloaded.load_settings(), "设置应能从 user://settings.cfg 读回")
	_expect(is_equal_approx(reloaded.fov, 100.0), "FOV 持久化失败")
	_expect(is_equal_approx(reloaded.sensitivity, 0.125), "灵敏度持久化失败")
	_expect(reloaded.volume == 55, "音量持久化失败")
	var bad_config := ConfigFile.new()
	bad_config.set_value("player", "fov", 500.0)
	bad_config.set_value("player", "sensitivity", 9.0)
	bad_config.set_value("player", "volume", "loud")
	var clamped := SettingsStore.new()
	clamped.load_from(bad_config)
	_expect(is_equal_approx(clamped.fov, SettingsStore.FOV_MAX), "越界 FOV 应钳制到上限")
	_expect(is_equal_approx(clamped.sensitivity, SettingsStore.SENSITIVITY_MAX), "越界灵敏度应钳制")
	_expect(clamped.volume == 80, "非法音量应回退默认 80，实际 %d" % clamped.volume)
	main.show_settings_page()
	main.settings_page.fov_slider.value = 104.0
	main.settings_page.sensitivity_slider.value = 0.15
	main.close_settings_page()
	_expect(is_equal_approx(main.camera.fov, 104.0), "设置页 FOV 应实时生效")
	_expect(is_equal_approx(main.settings.sensitivity, 0.15), "设置页灵敏度应实时生效")
	var saved := SettingsStore.new()
	saved.load_settings()
	_expect(is_equal_approx(saved.fov, 104.0), "页面保存后文件应为 104")
	store.reset_defaults()
	store.save_settings()


func _check_c4_hud() -> void:
	main._start_play()
	for i in 192:
		main.sim.tick()
	main.sim.plant_bomb(0)
	main.sim.tick()
	_expect(main.sim.get_bomb_planted() == 1, "Live 期应能下包（T 需携带 C4）")
	_expect(main.sim.get_bomb_timer() > 0, "下包后 C4 倒计时应 > 0")
	main._update_hud()
	_expect(main.hud.c4_label.text.contains("C4 已安放"), "HUD C4 应显示已安放")


func _check_scoreboard_and_results() -> void:
	main._start_play()
	main.toggle_scoreboard()
	_expect(main.scoreboard.visible, "Tab 应打开比分板")
	main.scoreboard.refresh(main.sim)
	var rows: Array[String] = main.scoreboard.row_texts()
	_expect(rows.size() >= 3, "比分板应有比分 + 2 行玩家")
	_expect(rows[0].contains("T") and rows[0].contains("CT"), "比分板应显示双方比分")
	_expect(rows[1].contains("T") and rows[2].contains("CT"), "比分板应显示双方队伍")
	main.toggle_scoreboard()
	_expect(not main.scoreboard.visible, "再次 Tab 应关闭比分板")
	main.result_page.show_round_result("T", 1, 0, 2, 3400, 4.5)
	_expect(main.result_page.visible, "回合结算页应可见")
	var round_text: String = main.result_page.summary_text()
	_expect(round_text.contains("回合结束") and round_text.contains("1 : 0")
			and round_text.contains("3400"), "回合结算应含胜者/比分/金钱")
	main.result_page.hide_result()
	main.result_page.show_match_result("CT", 2, 3, 5, 7000, 5)
	_expect(main.result_page.mode == "match", "比赛结算模式")
	_expect(main.result_page.summary_text().contains("比赛结束"), "比赛结算应含胜者/比分")
	_expect(main.result_page.restart_button.visible and main.result_page.menu_button.visible,
			"比赛结算应给再来一场/返回主菜单按钮")
	main.result_page.hide_result()
	main._show_menu()


func _check_kill_feed() -> void:
	main._start_play()
	main.sim.reset()
	main.hud.reset_feed()
	main.sim.fire(0, 0.0, 0.6)
	main.sim.tick()
	main._update_hud()
	_expect(main.hud.kill_total == 1, "HUD 击杀累计应为 1，实际 %d" % main.hud.kill_total)
	_expect(main.hud.death_total == 1, "HUD 阵亡累计应为 1，实际 %d" % main.hud.death_total)
	_expect(main.hud.feed_lines().size() >= 1, "HUD 击杀信息应有滚动条目")
	_expect(main.hud.feed_lines()[0].contains("击杀"), "击杀信息应标注击杀")


func _check_match_end_flow() -> void:
	main._start_play()
	for i in 192:
		main.sim.tick()
	main.sim.fire(0, 0.0, 0.6)
	main.sim.tick()
	main._update_view()
	_expect(main.result_page.visible and main.result_page.mode == "round", "击杀后应弹回合结算")
	_expect(main.result_page.title_label.text.contains("回合结束")
			and main.result_page.score_label.text.contains("1 : 0"), "回合结算应含胜者与比分")
	for i in 320:
		main.sim.tick()
	main._update_view()
	_expect(not main.result_page.visible, "回合重置后应收起回合结算")
	for i in 192:
		main.sim.tick()
	# 第二回合：T 出生点回到 (0,-200)，靶子在 (400,0)，按方位角平射 3 枪（36×3 > 100）击杀。
	var aim_yaw: float = rad_to_deg(atan2(200.0, 400.0))
	for shot in 3:
		main.sim.fire(0, aim_yaw, 0.0)
		for wait in 6:
			main.sim.tick()
	main._update_view()
	_expect(main.result_page.mode == "match", "两分后应进入比赛结算")
	_expect(main.result_page.title_label.text.contains("比赛结束")
			and main.result_page.title_label.text.contains("T"), "比赛结算应含最终胜者")
	_expect(main.result_page.score_label.text.contains("2 : 0"), "比赛结算应含最终比分")
	_expect(main.result_page.stats_label.text.contains("击杀 2")
			and main.result_page.stats_label.text.contains("$"), "比赛结算应含击杀与金钱")
	_expect(main.result_page.restart_button.visible and main.result_page.menu_button.visible,
			"比赛结算应可再来一场或返回主菜单")
	_expect(not main.playing, "比赛结算应停住对局")
	main._show_menu()


func _expect(condition: bool, message: String) -> void:
	if not condition:
		failures.append(message)
