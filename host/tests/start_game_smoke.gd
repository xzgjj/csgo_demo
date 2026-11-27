extends "res://tests/scene_test.gd"
## 开始游戏页回归：菜单入口 → 固定 de_dust2 / 模式选择 → start_requested 参数 → 设置入口。
## 用法：godot --headless --path host --script res://tests/start_game_smoke.gd


func _run() -> void:
	await process_frame
	var menu: Variant = main.menu
	var page: Variant = main.get_node_or_null("StartGamePage")
	if page == null:
		failures.append("StartGamePage 节点未装配")
		finish("START_GAME_SMOKE")
		return
	_check("默认地图应为 de_dust2", page.selected_map() == "de_dust2")
	_check("主菜单不应展示旧图", not str(menu.status_label.text).contains("de_ash"))
	menu.start_button.pressed.emit()
	await process_frame
	_check("菜单开始按钮应隐藏主菜单", not bool(menu.visible))
	_check("菜单开始按钮应打开部署页", bool(page.visible))
	var picked: Array = []
	page.start_requested.connect(func(map_id: String, mode_id: String) -> void:
		picked.append([map_id, mode_id]))
	page._select_mode("training")
	_check("切换模式后 selected_mode 应为 training", page.selected_mode() == "training")
	page._on_play()
	await process_frame
	_check("部署页应发出固定地图的请求", picked == [["de_dust2", "training"]])
	_check("请求后部署页应关闭", not bool(page.visible))
	menu.visible = true
	menu.settings_button.pressed.emit()
	await process_frame
	_check("设置按钮应打开设置页", bool(main.settings_page.visible))
	finish("START_GAME_SMOKE")


func _check(label: String, ok: bool) -> void:
	if not ok:
		failures.append(label)
