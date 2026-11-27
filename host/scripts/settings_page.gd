class_name SettingsPage
extends CanvasLayer
## 设置页（SPEC-05 §5.4.4）：左分类栏（8 页）+ 右侧分组滚动列表 + 顶部搜索 + 底部操作条。
## 数据全部来自 SettingsRegistry；控件工厂在 SettingsWidgets，本文件只管导航/搜索/页脚。
## 改动即时写回 SettingsStore（去抖 0.5s），store.changed → main 的 SettingsApplier 热应用。

signal changed

const NAV_WIDTH := 190.0
const PAGE_TITLE_SIZE := 24
const GROUP_TITLE_SIZE := 17

var store: SettingsStore
var widgets: SettingsWidgets
var fov_slider: HSlider
var sensitivity_slider: HSlider
var back_button: Button
var search_box: LineEdit
var status_label: Label
var nav_buttons: Dictionary = {}
var page_nodes: Dictionary = {}
var group_nodes: Array[Dictionary] = []
var _current_page := ""
var _search := ""
var _confirm: ConfirmationDialog
var _panel: PanelContainer
var _scroll: ScrollContainer


func build(settings: SettingsStore) -> void:
	store = settings
	layer = 4
	visible = false
	_build_root()
	_build_nav()
	_build_sections()
	_build_footer()
	_confirm = ConfirmationDialog.new()
	_confirm.dialog_text = "恢复全部设置为默认值？该项不可撤销。"
	_confirm.confirmed.connect(_on_reset_confirmed)
	add_child(_confirm)
	store.changed.connect(_on_store_changed)
	select_page(str(store.registry.page_ids()[0]))
	sync_from_store()


func _build_root() -> void:
	_panel = PanelContainer.new()
	_panel.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	var style := StyleBoxFlat.new()
	style.bg_color = Color(0.06, 0.07, 0.09, 0.97)
	style.set_content_margin_all(18)
	_panel.add_theme_stylebox_override("panel", style)
	add_child(_panel)
	var root := VBoxContainer.new()
	root.add_theme_constant_override("separation", 10)
	_panel.add_child(root)
	var header := HBoxContainer.new()
	header.add_theme_constant_override("separation", 12)
	root.add_child(header)
	var title := Label.new()
	title.text = "设置"
	title.add_theme_font_size_override("font_size", 26)
	header.add_child(title)
	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	header.add_child(spacer)
	search_box = LineEdit.new()
	search_box.placeholder_text = "搜索设置（id 或中文名）"
	search_box.custom_minimum_size = Vector2(280, 32)
	search_box.text_changed.connect(_on_search_changed)
	header.add_child(search_box)
	var body := HBoxContainer.new()
	body.size_flags_vertical = Control.SIZE_EXPAND_FILL
	body.add_theme_constant_override("separation", 12)
	root.add_child(body)
	var nav_panel := PanelContainer.new()
	nav_panel.custom_minimum_size = Vector2(NAV_WIDTH, 0)
	body.add_child(nav_panel)
	var nav := VBoxContainer.new()
	nav.name = "Nav"
	nav.add_theme_constant_override("separation", 6)
	nav_panel.add_child(nav)
	nav_buttons["__root"] = nav
	_scroll = ScrollContainer.new()
	_scroll.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	body.add_child(_scroll)
	var content := VBoxContainer.new()
	content.name = "Content"
	content.add_theme_constant_override("separation", 14)
	content.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_scroll.add_child(content)
	page_nodes["__content"] = content


func _build_nav() -> void:
	var nav: VBoxContainer = nav_buttons["__root"] as VBoxContainer
	for page_id: String in store.registry.page_ids():
		var page := store.registry.page(page_id)
		var button := Button.new()
		button.text = "%s  %s" % [_icon_glyph(str(page.get("icon", ""))), str(page.get("title", page_id))]
		button.custom_minimum_size = Vector2(NAV_WIDTH - 12, 40)
		button.alignment = HORIZONTAL_ALIGNMENT_LEFT
		button.focus_mode = Control.FOCUS_ALL
		button.pressed.connect(select_page.bind(page_id))
		nav.add_child(button)
		nav_buttons[page_id] = button


func _build_sections() -> void:
	var content: VBoxContainer = page_nodes["__content"] as VBoxContainer
	for page_id: String in store.registry.page_ids():
		var page := store.registry.page(page_id)
		var section := VBoxContainer.new()
		section.add_theme_constant_override("separation", 8)
		content.add_child(section)
		var heading := Label.new()
		heading.text = str(page.get("title", page_id))
		heading.add_theme_font_size_override("font_size", PAGE_TITLE_SIZE)
		section.add_child(heading)
		for group: Dictionary in page.get("groups", []):
			var group_box := VBoxContainer.new()
			group_box.add_theme_constant_override("separation", 4)
			section.add_child(group_box)
			var group_title := Label.new()
			group_title.text = str(group.get("title", group.get("id", "")))
			group_title.add_theme_font_size_override("font_size", GROUP_TITLE_SIZE)
			group_title.add_theme_color_override("font_color", UiFactory.NEUTRAL_COLOR)
			group_box.add_child(group_title)
			var items: Array[Dictionary] = []
			for raw_item: Dictionary in group.get("items", []):
				items.append(raw_item)
			if widgets == null:
				widgets = SettingsWidgets.new()
				widgets.setup(store)
			widgets.build(group_box, items)
			var ids: Array[String] = []
			for item: Dictionary in items:
				ids.append(str(item["id"]))
			group_nodes.append({"page": page_id, "id": str(group.get("id", "")),
					"node": group_box, "items": ids})
		page_nodes[page_id] = section
	fov_slider = widgets.control_of("camera.fov") as HSlider
	sensitivity_slider = widgets.control_of("controls.mouse.sensitivity") as HSlider


func _build_footer() -> void:
	var footer := HBoxContainer.new()
	footer.add_theme_constant_override("separation", 10)
	_panel.get_child(0).add_child(footer)
	var reset_button := _footer_button("恢复默认")
	reset_button.pressed.connect(func() -> void: _confirm.popup_centered())
	footer.add_child(reset_button)
	var undo_button := _footer_button("撤销更改")
	undo_button.pressed.connect(_on_undo)
	footer.add_child(undo_button)
	status_label = Label.new()
	status_label.text = "改动即写 user://settings.cfg（0.5s 去抖）"
	status_label.add_theme_color_override("font_color", UiFactory.NEUTRAL_COLOR)
	status_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	footer.add_child(status_label)
	var apply_button := _footer_button("应用")
	apply_button.pressed.connect(_on_apply)
	footer.add_child(apply_button)
	back_button = _footer_button("关闭")
	back_button.pressed.connect(_on_close)
	footer.add_child(back_button)


func _footer_button(text: String) -> Button:
	var button := Button.new()
	button.text = text
	button.custom_minimum_size = Vector2(120, 40)
	button.focus_mode = Control.FOCUS_ALL
	return button


## 切换分类页（搜索激活时保留搜索结果，不强制切页）。
func select_page(page_id: String) -> void:
	if not page_nodes.has(page_id) or page_id == "__content":
		return
	_current_page = page_id
	if _search.is_empty():
		for key: String in page_nodes.keys():
			if key == "__content":
				continue
			page_nodes[key].visible = key == page_id
	for key: String in nav_buttons.keys():
		if key == "__root":
			continue
		var button: Button = nav_buttons[key] as Button
		button.modulate = Color(1.0, 0.86, 0.45) if key == page_id else Color.WHITE
	if _scroll != null:
		_scroll.scroll_vertical = 0


func current_page() -> String:
	return _current_page


func search_text() -> String:
	return _search


func set_search(text: String) -> void:
	search_box.text = text
	_on_search_changed(text)


func _on_search_changed(text: String) -> void:
	_search = text.strip_edges().to_lower()
	var matched: Array[String] = []
	if not _search.is_empty():
		matched = store.registry.search(_search)
	for entry: Dictionary in group_nodes:
		var node: VBoxContainer = entry["node"] as VBoxContainer
		var any := false
		for item_id: String in entry["items"]:
			var visible_row := _search.is_empty() or matched.has(item_id)
			var row := widgets.row(item_id)
			if not row.is_empty():
				(row["root"] as Control).visible = visible_row
			any = any or visible_row
		node.visible = any
	for page_id: String in page_nodes.keys():
		if page_id == "__content":
			continue
		var section: VBoxContainer = page_nodes[page_id] as VBoxContainer
		section.visible = _search.is_empty() and page_id == _current_page
		if not _search.is_empty():
			section.visible = _page_has_match(section)
	if not _search.is_empty():
		_scroll.scroll_vertical = 0


func _page_has_match(section: VBoxContainer) -> bool:
	for child: Node in section.get_children():
		if child is VBoxContainer and child.visible:
			return true
	return false


func sync_from_store() -> void:
	if store == null or widgets == null:
		return
	store.begin_edit()
	widgets.sync_all()
	_refresh_all_dots()


func _refresh_all_dots() -> void:
	for item_id: String in widgets.rows.keys():
		var row := widgets.row(item_id)
		if not row.is_empty():
			(row["dot"] as Label).visible = store.is_modified(item_id)


## 兼容旧入口：写入并返回是否成功（页面关闭/主菜单返回都会调用）。
func save_settings() -> bool:
	var ok := store.save_settings()
	status_label.text = "已保存 → user://settings.cfg" if ok else "保存失败：user:// 不可写"
	return ok


func _on_apply() -> void:
	save_settings()
	changed.emit()


func _on_close() -> void:
	_leave()
	changed.emit()


## 离开页面：结束按键捕获、落盘（页面本身可见性由 main 决定）。
func _leave() -> void:
	if widgets != null:
		widgets.end_rebind()
	save_settings()


func _on_undo() -> void:
	store.undo()
	_refresh_all_dots()
	status_label.text = "已撤销到打开设置页时的状态"


func _on_reset_confirmed() -> void:
	store.reset_all()
	_refresh_all_dots()
	status_label.text = "已恢复默认值"


## store 被外部写入（快捷键重绑/暂停菜单重置）时同步单行数值与圆点。
func _on_store_changed(item_id: String, _value: Variant, _target: String, _apply: String) -> void:
	if widgets != null:
		widgets.sync_item(item_id)


func _input(event: InputEvent) -> void:
	if not visible or widgets == null:
		return
	if widgets.is_rebinding() and widgets.capture_rebind(event):
		get_viewport().set_input_as_handled()


## 键盘/手柄导航入口（Tab 与方向键由 Godot 焦点系统处理）。
func focus_first() -> void:
	for page_id: String in nav_buttons.keys():
		if page_id == "__root":
			continue
		(nav_buttons[page_id] as Button).grab_focus()
		return


func _icon_glyph(icon: String) -> String:
	var glyphs := {"monitor": "▣", "sparkles": "✦", "speaker": "♪", "target": "◎",
			"keyboard": "⌨", "camera": "▤", "window": "▢", "person": "◉"}
	return str(glyphs.get(icon, "•"))
