extends "res://tests/scene_test.gd"
## 开始游戏页回归：菜单入口 → 地图/模式选择 → start_requested 参数 → 设置入口。
## 用法：godot --headless --path host --script res://tests/start_game_smoke.gd


func _run() -> void:
	await process_frame
	var menu: Variant = main.menu
	var page: Variant = main.get_node_or_null("StartGamePage")
	if page == null:
		failures.append("StartGamePage 节点未装配")
		finish("START_GAME_SMOKE")
		return
	_check_options(page)
	menu.start_button.pressed.emit()
	await process_frame
	_check("菜单开始按钮应隐藏主菜单", not bool(menu.visible))
	_check("菜单开始按钮应打开开局页", bool(page.visible))
	var picked: Array = []
	page.start_requested.connect(func(map_id: String, mode_id: String) -> void:
		picked.append([map_id, mode_id]))
	page.map_option.select(page.map_option.item_count - 1)
	page.mode_option.select(1)
	page._on_play()
	await process_frame
	_check("开局页应发出带地图/模式的请求", picked == [["de_dust2", "training"]])
	_check("请求后开局页应关闭", not bool(page.visible))
	menu.visible = true
	menu.settings_button.pressed.emit()
	await process_frame
	_check("设置按钮应打开设置页", bool(main.settings_page.visible))
	finish("START_GAME_SMOKE")


func _check_options(page: Variant) -> void:
	var maps: Array[String] = []
	for i in page.map_option.item_count:
		maps.append(page.map_option.get_item_text(i))
	_check("地图选项应包含 de_ash/de_dust2", maps == ["de_ash", "de_dust2"])


func _check(label: String, ok: bool) -> void:
	if not ok:
		failures.append(label)
