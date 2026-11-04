class_name UiFactory
extends RefCounted
## 页面通用控件工厂：统一卡片/按钮/滑条搭建，避免各页面重复同一套布局代码。
## 另提供队伍归属与玩家数推导（宿主内核未暴露 get_team 时按 AssignTeams 规则回退）。

const CARD_WIDTH := 460.0
const BUTTON_WIDTH := 380.0
const PANEL_BACKGROUND := Color(0.09, 0.10, 0.13, 0.92)
const PANEL_BORDER := Color(0.35, 0.40, 0.48, 0.90)
const TEAM_T_COLOR := Color(0.93, 0.62, 0.24)
const TEAM_CT_COLOR := Color(0.42, 0.66, 0.95)
const NEUTRAL_COLOR := Color(0.82, 0.84, 0.88)


static func add_layer(parent: Node, layer_name: String, order: int) -> CanvasLayer:
	var layer := CanvasLayer.new()
	layer.name = layer_name
	layer.layer = order
	parent.add_child(layer)
	return layer


static func build_card(layer: CanvasLayer, title: String, card_height: float) -> VBoxContainer:
	var panel := _make_panel(Vector2(CARD_WIDTH, card_height))
	_centered_root(layer).add_child(panel)
	var box := VBoxContainer.new()
	box.add_theme_constant_override("separation", 12)
	panel.add_child(box)
	var heading := Label.new()
	heading.text = title
	heading.add_theme_font_size_override("font_size", 26)
	heading.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	box.add_child(heading)
	return box


## 不透明深色面板，保证页面在 3D 场景上有足够对比度。
static func _make_panel(panel_size: Vector2) -> PanelContainer:
	var panel := PanelContainer.new()
	panel.custom_minimum_size = panel_size
	var style := StyleBoxFlat.new()
	style.bg_color = PANEL_BACKGROUND
	style.border_color = PANEL_BORDER
	style.set_border_width_all(1)
	style.set_corner_radius_all(6)
	style.set_content_margin_all(14)
	panel.add_theme_stylebox_override("panel", style)
	return panel


## 页面根：全屏 CenterContainer，让卡片在任意窗口尺寸下居中（而不是贴着锚点偏移）。
static func _centered_root(layer: CanvasLayer) -> CenterContainer:
	var center := CenterContainer.new()
	center.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	layer.add_child(center)
	return center


static func add_label(box: Control, text: String, font_size: int,
		alignment: HorizontalAlignment = HORIZONTAL_ALIGNMENT_CENTER) -> Label:
	var label := Label.new()
	label.text = text
	label.add_theme_font_size_override("font_size", font_size)
	label.horizontal_alignment = alignment
	box.add_child(label)
	return label


static func add_button(box: Control, text: String) -> Button:
	var button := Button.new()
	button.text = text
	button.custom_minimum_size = Vector2(BUTTON_WIDTH, 44)
	button.add_theme_font_size_override("font_size", 18)
	var normal := _button_style(Color(0.16, 0.18, 0.22, 0.95), Color(0.38, 0.42, 0.50))
	button.add_theme_stylebox_override("normal", normal)
	button.add_theme_stylebox_override("hover",
			_button_style(Color(0.25, 0.29, 0.35, 0.98), Color(0.55, 0.62, 0.72)))
	button.add_theme_stylebox_override("pressed",
			_button_style(Color(0.34, 0.40, 0.48, 1.0), Color(0.70, 0.78, 0.90)))
	box.add_child(button)
	return button


static func _button_style(background: Color, border: Color) -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color = background
	style.border_color = border
	style.set_border_width_all(1)
	style.set_corner_radius_all(4)
	style.set_content_margin_all(8)
	return style


## 一行滑条：[说明][滑条][数值]，返回 {"slider","caption","value"}。
static func add_slider_row(box: Control, caption: String) -> Dictionary:
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 10)
	var caption_label := Label.new()
	caption_label.text = caption
	caption_label.custom_minimum_size = Vector2(140, 26)
	row.add_child(caption_label)
	var slider := HSlider.new()
	slider.custom_minimum_size = Vector2(250, 26)
	row.add_child(slider)
	var value_label := Label.new()
	value_label.custom_minimum_size = Vector2(64, 26)
	value_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	row.add_child(value_label)
	box.add_child(row)
	return {"slider": slider, "caption": caption_label, "value": value_label}


static func build_panel(layer: CanvasLayer, panel_size: Vector2) -> VBoxContainer:
	var panel := _make_panel(panel_size)
	_centered_root(layer).add_child(panel)
	var box := VBoxContainer.new()
	box.add_theme_constant_override("separation", 8)
	panel.add_child(box)
	return box


## 玩家数 / 队伍归属统一走内核 getter（RecoilSim 已绑定 get_player_count / get_team）。
static func player_count(sim: RefCounted) -> int:
	return int(sim.get_player_count())


static func team_id(sim: RefCounted, index: int) -> int:
	return int(sim.get_team(index))


static func team_name(sim: RefCounted, index: int) -> String:
	return "T" if team_id(sim, index) == 1 else "CT"


static func team_color(sim: RefCounted, index: int) -> Color:
	return TEAM_T_COLOR if team_id(sim, index) == 1 else TEAM_CT_COLOR
