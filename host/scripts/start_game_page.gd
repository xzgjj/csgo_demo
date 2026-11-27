class_name StartGamePage
extends CanvasLayer
## 作战部署页：地图锁定 de_dust2，只选模式；左侧指挥栏 + 右下任务简报。
## 只发 start_requested(map_id, mode_id)，不直接改 main 状态。

signal start_requested(map_id: String, mode_id: String)

const MODES := {
	"match": ["5v5 匹配", "10 人 · 炸弹模式 · 完整经济"],
	"training": ["本地训练", "2 人灰盒 · 练枪 / 走位"],
}

var play_button: Button
var back_button: Button
var mode_buttons: Dictionary = {}
var _map_id := MapLoader.cli_map_id()
var _mode := "match"


func build() -> void:
	layer = 3
	var root := MenuSkin.root(self)
	var box := MenuSkin.side_panel(root, 560.0)
	MenuSkin.kicker(box, "作战部署  /  DEPLOYMENT")
	MenuSkin.heading(box, _map_id.to_upper(), 44)
	MenuSkin.note(box, "新地图 · 爆破 · 本地运行 · 地图已锁定")
	MenuSkin.rule(box)
	MenuSkin.kicker(box, "模式  /  MODE")
	for mode_id: String in MODES:
		var pair: Array = MODES[mode_id]
		var button := MenuSkin.nav_button(box, "%s   /   %s" % [pair[0], pair[1]],
				mode_id == _mode)
		button.custom_minimum_size = Vector2(456, 66)
		button.add_theme_font_size_override("font_size", 17)
		button.pressed.connect(_select_mode.bind(mode_id))
		mode_buttons[mode_id] = button
	var spacer := Control.new()
	spacer.size_flags_vertical = Control.SIZE_EXPAND_FILL
	box.add_child(spacer)
	MenuSkin.note(box, "地图已锁定，本页不提供换图。")
	play_button = MenuSkin.primary_button(box, "进入对局   →")
	back_button = MenuSkin.nav_button(box, "返回指挥中心")
	play_button.pressed.connect(_on_play)
	back_button.pressed.connect(close)
	_build_brief(root)


func _build_brief(root: Control) -> void:
	var card := MenuSkin.corner_card(root, 390.0, 196.0)
	MenuSkin.kicker(card, "任务简报  /  BRIEFING")
	MenuSkin.heading(card, _map_id.to_upper(), 28)
	MenuSkin.note(card, "双包点 · 长 A / 中门 / B 通道")
	MenuSkin.note(card, "10 人 · 购买 25s · 本地确定性内核")
	MenuSkin.note(card, "模式只影响人数与节奏，默认对战地图为新图。")


func open() -> void:
	visible = true
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE


func close() -> void:
	visible = false


func selected_map() -> String:
	return _map_id


func selected_mode() -> String:
	return _mode


func _select_mode(mode_id: String) -> void:
	if not mode_buttons.has(mode_id):
		return
	_mode = mode_id
	for id: String in mode_buttons:
		MenuSkin.set_nav_selected(mode_buttons[id], id == _mode)


func _on_play() -> void:
	visible = false
	start_requested.emit(selected_map(), selected_mode())


func _unhandled_input(event: InputEvent) -> void:
	if not visible:
		return
	if event.is_action_pressed("ui_cancel"):
		get_viewport().set_input_as_handled()
		close()
