class_name StartGamePage
extends CanvasLayer
## 开局页：地图 / 模式选择 → 开始；主菜单“开始游戏”入口。
## 只收集用户选择并发出 start_requested，不直接改 main 状态（由 HostPages 接线）。

signal start_requested(map_id: String, mode_id: String)

const MAP_MODES := {
	"match": "5v5 匹配",
	"training": "本地训练（2 人灰盒）",
}

var map_option: OptionButton
var mode_option: OptionButton
var play_button: Button
var back_button: Button
var summary_label: Label
var map_desc: Label
var box: VBoxContainer


func build() -> void:
	layer = 3
	var root := UiFactory.build_card(self, "开始游戏", 350.0, 640.0)
	box = root
	map_desc = UiFactory.add_label(root, "选择地图与模式；地图列表由注册表提供。", 14)
	map_desc.add_theme_color_override("font_color", UiFactory.NEUTRAL_COLOR)
	map_option = _add_choice_row("地图", _map_options(), _on_map_changed)
	mode_option = _add_choice_row("模式", Array(MAP_MODES.values()), _on_mode_changed)
	summary_label = UiFactory.add_label(root, "", 15)
	play_button = UiFactory.add_button(root, "进入对局")
	back_button = UiFactory.add_button(root, "返回")
	play_button.pressed.connect(_on_play)
	back_button.pressed.connect(close)
	_refresh_summary()


func _map_options() -> Array:
	var options: Array = []
	for item: Dictionary in MapLoader.registry().get("maps", []):
		options.append(str(item.get("id", "")))
	if options.is_empty():
		options.append(MapLoader.default_map_id())
	return options


func _add_choice_row(caption: String, options: Array, on_change: Callable) -> OptionButton:
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 12)
	var label := Label.new()
	label.text = caption
	label.add_theme_font_size_override("font_size", 16)
	label.custom_minimum_size = Vector2(80, 0)
	row.add_child(label)
	var picker := OptionButton.new()
	picker.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	for text: Variant in options:
		picker.add_item(str(text))
	picker.item_selected.connect(on_change)
	row.add_child(picker)
	box.add_child(row)
	picker.select(0)
	return picker


func open() -> void:
	visible = true
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	_refresh_summary()


func close() -> void:
	visible = false


func selected_map() -> String:
	if map_option == null or map_option.item_count == 0:
		return MapLoader.default_map_id()
	return map_option.get_item_text(map_option.selected)


func selected_mode() -> String:
	if mode_option == null or mode_option.item_count == 0:
		return "match"
	var keys: Array = MAP_MODES.keys()
	return str(keys[clampi(mode_option.selected, 0, keys.size() - 1)])


func _on_map_changed(_index: int) -> void:
	_refresh_summary()


func _on_mode_changed(_index: int) -> void:
	_refresh_summary()


func _refresh_summary() -> void:
	if summary_label == null:
		return
	summary_label.text = "地图 %s · 模式 %s · 点击“进入对局”开始" % [
		selected_map(), MAP_MODES.get(selected_mode(), "5v5 匹配")]


func _on_play() -> void:
	visible = false
	start_requested.emit(selected_map(), selected_mode())


func _unhandled_input(event: InputEvent) -> void:
	if not visible:
		return
	if event.is_action_pressed("ui_cancel"):
		get_viewport().set_input_as_handled()
		close()
