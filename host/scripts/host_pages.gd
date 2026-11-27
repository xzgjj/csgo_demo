class_name HostPages
extends RefCounted
const LoadoutPageScript = preload("res://scripts/loadout_page.gd")
const RadarPanelScript = preload("res://scripts/radar.gd")
const ScopeControllerScript = preload("res://scripts/scope_controller.gd")
const BuyMenuScript = preload("res://scripts/buy_menu.gd")
const PurchaseFlowScript = preload("res://scripts/purchase_flow.gd")
## 宿主页面装配：由 main.gd 调用，创建 HUD / 主菜单 / 暂停 / 设置 / 结算 / 比分板与致盲层。
## 只做节点与信号连线，不持有规则状态（main 仍是对外入口）。


static func build(main: Node) -> void:
	var hud := GameHud.new()
	main.add_child(hud)
	hud.build()
	main.set("hud", hud)
	var radar := RadarPanelScript.new()
	hud.add_child(radar)
	radar.setup(main)
	radar.use_map(str(main.get("map_id")))
	main.set("radar", radar)
	var scope := ScopeControllerScript.new()
	var scope_layer := CanvasLayer.new()
	scope_layer.name = "ScopeLayer"
	scope_layer.layer = 1
	main.add_child(scope_layer)
	scope.build(scope_layer)
	main.set("scope", scope)

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

	var loadout_page := LoadoutPageScript.new()
	main.add_child(loadout_page)
	loadout_page.build(main.get("account"))
	main.set("loadout_page", loadout_page)
	menu.loadout_button.pressed.connect(Callable(main, "show_loadout_page"))
	loadout_page.back_button.pressed.connect(Callable(main, "close_loadout_page"))

	var buy_menu := BuyMenuScript.new()
	main.add_child(buy_menu)
	buy_menu.build()
	main.set("buy_menu", buy_menu)

	var settings_page := SettingsPage.new()
	main.add_child(settings_page)
	settings_page.build(main.get("settings"))
	main.set("settings_page", settings_page)
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

	var info := InfoOverlay.new()
	info.name = "InfoOverlay"
	main.add_child(info)
	info.setup(main.get("sim"), main.get("settings"))


## 是否有会抢鼠标的页面打开（主菜单/暂停/设置/结算/比分板/购买菜单/物品栏）。
static func page_open(main: Node) -> bool:
	var pages: Array[bool] = [main.menu.visible, main.pause_page.visible,
			main.settings_page.visible, main.result_page.visible, main.scoreboard.visible,
			main.buy_menu != null and main.buy_menu.is_open(),
			main.loadout_page != null and main.loadout_page.visible]
	return pages.has(true)
