extends SceneTree
## SET-06 导航与布局模型：8 页全部可达、全部条目有控件、搜索可过滤、键盘重绑可用。
## 用法：& godot --headless --path host --script res://tests/settings_nav_smoke.gd

const StoreScript = preload("res://scripts/settings_store.gd")
const PageScript = preload("res://scripts/settings_page.gd")

var failures: Array[String] = []
var page: CanvasLayer


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	var store = StoreScript.new("user://settings_nav_test.cfg")
	page = PageScript.new()
	root.add_child(page)
	page.build(store)
	await process_frame
	_check_pages(store)
	_check_rows(store)
	_check_search(store)
	_check_widgets(store)
	_check_rebind(store)
	_check_footer(store)
	_write_nav_csv(store)
	_finish()


func _check_pages(store) -> void:
	var nav_count := 0
	for key: String in page.nav_buttons.keys():
		if key != "__root":
			nav_count += 1
	_expect(nav_count == 8, "左侧分类栏应为 8 项，实际 %d" % nav_count)
	for page_id: String in store.registry.page_ids():
		var button: Button = page.nav_buttons[page_id]
		_expect(button.focus_mode == Control.FOCUS_ALL, "分类按钮 %s 必须可键盘聚焦" % page_id)
		page.select_page(page_id)
		_expect(page.current_page() == page_id, "无法切到页面 %s" % page_id)
	_expect(page.page_nodes.size() == 9, "应有 8 个页面节点 + __content")
	page.focus_first()
	_expect(page.get_viewport().gui_get_focus_owner() != null, "focus_first 后应有焦点归属")


func _check_rows(store) -> void:
	_expect(page.widgets.rows.size() == store.registry.items.size(),
			"控件行数 %d != 注册表项 %d" % [page.widgets.rows.size(), store.registry.items.size()])
	_expect(page.widgets.rows.size() >= 60, "可视条目应 >= 60")
	_expect(page.fov_slider != null and page.sensitivity_slider != null, "兼容滑块别名缺失")


func _check_search(store) -> void:
	page.set_search("准星")
	var matched_visible := false
	for item_id: String in page.widgets.rows.keys():
		var row: Dictionary = page.widgets.row(item_id)
		var visible: bool = (row["root"] as Control).visible
		if item_id == "gameplay.crosshair.color":
			matched_visible = visible
	_expect(matched_visible, "搜索“准星”应显示准星颜色行")
	var hidden := false
	for item_id: String in page.widgets.rows.keys():
		var row: Dictionary = page.widgets.row(item_id)
		if item_id.begins_with("audio.") and not (row["root"] as Control).visible:
			hidden = true
	_expect(hidden, "搜索时应隐藏不匹配行")
	page.set_search("")
	page.select_page("display")


func _check_widgets(store) -> void:
	page.fov_slider.value = 100.0
	_expect(is_equal_approx(float(store.get_value("camera.fov")), 100.0),
			"滑块改动未写回 store")
	var row: Dictionary = page.widgets.row("camera.fov")
	_expect((row["dot"] as Label).visible, "修改过的小圆点应可见")
	store.reset("camera.fov")
	_expect(not (row["dot"] as Label).visible, "恢复默认后小圆点应隐藏")


func _check_rebind(store) -> void:
	page.widgets.begin_rebind("controls.bind.jump")
	var event := InputEventKey.new()
	event.physical_keycode = KEY_T
	event.pressed = true
	_expect(page.widgets.capture_rebind(event), "重绑应消费按键事件")
	_expect(str(store.get_value("controls.bind.jump")) == "T", "重绑后跳跃键应为 T")
	_expect(not page.widgets.is_rebinding(), "捕获后应退出重绑态")
	store.reset("controls.bind.jump")


func _check_footer(store) -> void:
	_expect(page.back_button != null, "缺少关闭按钮")
	_expect(page.search_box != null, "缺少搜索框")
	page.status_label.text = ""
	_expect(page.save_settings(), "保存应成功")
	_expect(not page.status_label.text.is_empty(), "保存后应有状态提示")
	_expect(page.search_text() == "", "搜索框应可清空")


func _expect(condition: bool, message: String) -> void:
	if not condition:
		failures.append(message)


## SET-06 证据：逐个条目记录“是否可键盘聚焦”，写入 log/verify/settings/nav.csv。
func _write_nav_csv(store) -> void:
	var out := ProjectSettings.globalize_path("res://").path_join("../log/verify/settings/nav.csv")
	DirAccess.make_dir_recursive_absolute(out.get_base_dir())
	var handle := FileAccess.open(out, FileAccess.WRITE)
	if handle == null:
		failures.append("无法写入 nav.csv")
		return
	handle.store_line("page,item,type,focusable")
	for page_id: String in store.registry.page_ids():
		page.select_page(page_id)
		for item_id: String in store.registry.item_ids():
			if not item_id.begins_with(page_id + "."):
				continue
			var control: Control = page.widgets.control_of(item_id)
			var focusable: bool = control != null and control.focus_mode != Control.FOCUS_NONE
			handle.store_line("%s,%s,%s,%s" % [page_id, item_id,
					str(store.registry.item(item_id).get("type", "")), focusable])
	handle.close()
	_expect(FileAccess.file_exists(out), "nav.csv 未生成")


func _finish() -> void:
	if failures.is_empty():
		print("SETTINGS_NAV PASS")
		quit(0)
		return
	for item in failures:
		print("SETTINGS_NAV_FAIL %s" % item)
	quit(1)
