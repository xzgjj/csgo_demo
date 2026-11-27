class_name MenuSkin
extends RefCounted
## 主菜单 / 部署页共用视觉：深色左栏、金色强调、英文 kicker 与分级按钮。

const INK := Color(0.035, 0.045, 0.065, 0.94)
const PANEL := Color(0.055, 0.065, 0.085, 0.90)
const GOLD := Color(0.93, 0.63, 0.25)
const TEXT := Color(0.92, 0.93, 0.95)
const MUTED := Color(0.56, 0.62, 0.70)
const LINE := Color(0.28, 0.33, 0.40, 0.85)


static func root(layer: CanvasLayer) -> Control:
	var root := Control.new()
	root.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	root.mouse_filter = Control.MOUSE_FILTER_IGNORE
	layer.add_child(root)
	return root


## 左侧全高导航栏；返回带 52/56 内边距的 VBox。
static func side_panel(root: Control, width: float) -> VBoxContainer:
	var panel := PanelContainer.new()
	panel.set_anchors_and_offsets_preset(Control.PRESET_LEFT_WIDE)
	panel.offset_right = width
	panel.add_theme_stylebox_override("panel", _panel_style(INK, Color(0, 0, 0, 0), 0))
	root.add_child(panel)
	var margin := MarginContainer.new()
	margin.add_theme_constant_override("margin_left", 52)
	margin.add_theme_constant_override("margin_top", 56)
	margin.add_theme_constant_override("margin_right", 42)
	margin.add_theme_constant_override("margin_bottom", 48)
	panel.add_child(margin)
	var box := VBoxContainer.new()
	box.add_theme_constant_override("separation", 14)
	margin.add_child(box)
	return box


## 右下角“当前部署”卡片。
static func corner_card(root: Control, width: float, height: float) -> VBoxContainer:
	var panel := PanelContainer.new()
	panel.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_RIGHT)
	panel.offset_left = -width - 56
	panel.offset_top = -height - 48
	panel.offset_right = -56
	panel.offset_bottom = -48
	panel.add_theme_stylebox_override("panel", _panel_style(PANEL, LINE, 1))
	root.add_child(panel)
	var margin := MarginContainer.new()
	margin.add_theme_constant_override("margin_left", 22)
	margin.add_theme_constant_override("margin_top", 18)
	margin.add_theme_constant_override("margin_right", 22)
	margin.add_theme_constant_override("margin_bottom", 18)
	panel.add_child(margin)
	var box := VBoxContainer.new()
	box.add_theme_constant_override("separation", 6)
	margin.add_child(box)
	return box


static func kicker(parent: Control, text: String) -> Label:
	var label := label(parent, text, 13, MUTED, HORIZONTAL_ALIGNMENT_LEFT)
	return label


static func heading(parent: Control, text: String, size: int = 46) -> Label:
	return label(parent, text, size, TEXT, HORIZONTAL_ALIGNMENT_LEFT)


static func note(parent: Control, text: String) -> Label:
	var item := label(parent, text, 15, MUTED, HORIZONTAL_ALIGNMENT_LEFT)
	item.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	return item


static func label(parent: Control, text: String, size: int, color: Color,
		align: HorizontalAlignment) -> Label:
	var item := Label.new()
	item.text = text
	item.add_theme_font_size_override("font_size", size)
	item.add_theme_color_override("font_color", color)
	item.horizontal_alignment = align
	parent.add_child(item)
	return item


static func rule(parent: Control, inset: int = 0) -> void:
	var line := ColorRect.new()
	line.color = LINE
	line.custom_minimum_size = Vector2(0, 1)
	parent.add_child(line)


static func nav_button(parent: Control, text: String, selected: bool = false) -> Button:
	var button := Button.new()
	button.text = text
	button.custom_minimum_size = Vector2(386, 58)
	button.add_theme_font_size_override("font_size", 20)
	button.alignment = HORIZONTAL_ALIGNMENT_LEFT
	button.add_theme_stylebox_override("hover", _button_style(
			Color(0.15, 0.17, 0.21, 0.95), GOLD, 1))
	button.add_theme_stylebox_override("pressed", _button_style(
			Color(0.22, 0.18, 0.10, 0.95), GOLD, 1))
	parent.add_child(button)
	set_nav_selected(button, selected)
	return button


static func set_nav_selected(button: Button, selected: bool) -> void:
	button.add_theme_stylebox_override("normal", _button_style(
			Color(0.16, 0.13, 0.08, 0.92) if selected else Color(0.10, 0.12, 0.15, 0.85),
			GOLD if selected else Color(0.22, 0.26, 0.32, 0.0), 1 if selected else 0))


static func primary_button(parent: Control, text: String) -> Button:
	var button := Button.new()
	button.text = text
	button.custom_minimum_size = Vector2(386, 54)
	button.add_theme_font_size_override("font_size", 21)
	button.add_theme_stylebox_override("normal", _button_style(Color(0.72, 0.45, 0.14), GOLD, 1))
	button.add_theme_stylebox_override("hover", _button_style(Color(0.86, 0.56, 0.19), GOLD, 1))
	button.add_theme_stylebox_override("pressed", _button_style(Color(0.56, 0.35, 0.10), GOLD, 1))
	parent.add_child(button)
	return button


static func _button_style(bg: Color, border: Color, border_width: int) -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color = bg
	style.border_color = border
	style.set_border_width_all(border_width)
	style.border_width_left = maxi(border_width, 3)
	style.set_corner_radius_all(3)
	style.set_content_margin_all(10)
	style.content_margin_left = 18
	return style


static func _panel_style(bg: Color, border: Color, border_width: int) -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color = bg
	style.border_color = border
	style.set_border_width_all(border_width)
	style.set_corner_radius_all(4)
	return style
