class_name HostPages
extends RefCounted
## 宿主页面装配：由 main.gd 调用，创建 HUD / 主菜单 / 暂停 / 设置 / 结算 / 比分板与致盲层。
## 只做节点与信号连线，不持有规则状态（main 仍是对外入口）。


static func build(main: Node) -> void:
	var hud := GameHud.new()
	main.add_child(hud)
	hud.build()
	main.set("hud", hud)

	var menu := MainMenuPage.new()
	main.add_child(menu)
	menu.build()
	main.set("menu", menu)
	menu.start_button.pressed.connect(Callable(main, "_start_match"))
	menu.settings_button.pressed.connect(Callable(main, "show_settings_page"))
	menu.quit_button.pressed.connect(Callable(main, "_quit"))

	var pause_page := PausePage.new()
	main.add_child(pause_page)
	pause_page.build()
	main.set("pause_page", pause_page)
	pause_page.resume_button.pressed.connect(Callable(main, "_resume"))
	pause_page.settings_button.pressed.connect(Callable(main, "show_settings_page"))
	pause_page.menu_button.pressed.connect(Callable(main, "_show_menu"))

	var settings_page := SettingsPage.new()
	main.add_child(settings_page)
	settings_page.build(main.get("settings"))
	main.set("settings_page", settings_page)
	settings_page.changed.connect(Callable(main, "_apply_settings"))
	settings_page.back_button.pressed.connect(Callable(main, "close_settings_page"))

	var result_page := ResultPage.new()
	main.add_child(result_page)
	result_page.build()
	main.set("result_page", result_page)
	result_page.restart_button.pressed.connect(Callable(main, "_restart"))
	result_page.menu_button.pressed.connect(Callable(main, "_show_menu"))

	var scoreboard := ScoreboardPanel.new()
	main.add_child(scoreboard)
	scoreboard.build()
	main.set("scoreboard", scoreboard)

	var blind_overlay := ColorRect.new()
	blind_overlay.color = Color(1.0, 1.0, 1.0, 0.0)
	blind_overlay.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	blind_overlay.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var overlay_layer := CanvasLayer.new()
	overlay_layer.name = "BlindOverlay"
	overlay_layer.layer = 1
	main.add_child(overlay_layer)
	overlay_layer.add_child(blind_overlay)
	main.set("blind_overlay", blind_overlay)
