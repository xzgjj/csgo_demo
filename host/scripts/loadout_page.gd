class_name LoadoutPage
extends CanvasLayer
## 物品栏 / 装备页：昵称 + 主武器（AK/M4/AWP）+ 副武器（USP/Glock）+ 生涯统计。
## 视觉对齐 CS:GO 购买界面与单机装备菜单：暗色面板 + 金色选中描边 + 武器剪影卡，
## 而不是一排纯文字按钮。选择即通过 AccountStore 持久化，页面只读 store 数据。

const PRIMARY_WEAPONS := [
	{"id": "ak", "name": "AK-47", "meta": "步枪 · 30 发 · $2700", "icon": "res://textures/icons/weapon_ak47.png"},
	{"id": "m4", "name": "M4A4", "meta": "步枪 · 30 发 · $3100", "icon": "res://textures/icons/weapon_m4a4.png"},
	{"id": "awp", "name": "AWP", "meta": "狙击 · 10 发 · $4750", "icon": "res://textures/icons/weapon_awp.png"},
]
const SECONDARY_WEAPONS := [
	{"id": "usp", "name": "USP-S", "meta": "手枪 · 12 发 · $200", "icon": "res://textures/icons/weapon_usp.png"},
	{"id": "glock", "name": "Glock-18", "meta": "手枪 · 20 发 · $200", "icon": "res://textures/icons/weapon_glock.png"},
]
const CARD_WIDTH := 356.0
const CARD_HEIGHT := 92.0

var account: RefCounted
var nickname_label: Label
var stats_label: Label
var primary_buttons: Array[Button] = []
var secondary_buttons: Array[Button] = []
var back_button: Button

var _dim: ColorRect
var _panel: PanelContainer


func build(account_store: RefCounted) -> void:
	account = account_store
	layer = 6
	visible = false
	_dim = ColorRect.new()
	_dim.color = Color(0.01, 0.015, 0.025, 0.62)
	_dim.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	_dim.mouse_filter = Control.MOUSE_FILTER_STOP
	add_child(_dim)

	var center := CenterContainer.new()
	center.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(center)
	_panel = PanelContainer.new()
	_panel.custom_minimum_size = Vector2(824.0, 604.0)
	_panel.add_theme_stylebox_override("panel", _panel_style())
	center.add_child(_panel)

	var margin := MarginContainer.new()
	for side in ["left", "right", "top", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, 32)
	_panel.add_child(margin)
	var box := VBoxContainer.new()
	box.add_theme_constant_override("separation", 16)
	margin.add_child(box)

	box.add_child(_build_header())
	box.add_child(_build_rule())

	var body := HBoxContainer.new()
	body.add_theme_constant_override("separation", 26)
	body.size_flags_vertical = Control.SIZE_EXPAND_FILL
	box.add_child(body)
	var primary_col := _build_column("主武器", "PRIMARY")
	primary_buttons = _build_card_list(primary_col, PRIMARY_WEAPONS, true)
	body.add_child(primary_col)
	var secondary_col := _build_column("副武器", "SECONDARY")
	secondary_buttons = _build_card_list(secondary_col, SECONDARY_WEAPONS, false)
	body.add_child(secondary_col)
	secondary_col.add_child(_make_spacer())
	secondary_col.add_child(_build_stats_panel())

	box.add_child(_build_rule())
	box.add_child(_build_footer())
	_refresh()


func open() -> void:
	visible = true
	_refresh()


func close() -> void:
	visible = false


func is_open() -> bool:
	return visible


func select_primary(weapon_id: String) -> void:
	if account == null:
		return
	account.set_primary(weapon_id)
	_refresh()


func select_secondary(weapon_id: String) -> void:
	if account == null:
		return
	account.set_secondary(weapon_id)
	_refresh()


func _build_header() -> Control:
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 16)
	var titles := VBoxContainer.new()
	titles.add_theme_constant_override("separation", 2)
	var kicker := _label("LOADOUT / 装备", 13, MenuSkin.GOLD)
	titles.add_child(kicker)
	var title := _label("物品栏", 34, MenuSkin.TEXT)
	title.add_theme_font_size_override("font_size", 34)
	titles.add_child(title)
	titles.add_child(_label("选择主副武器 · 点击即保存", 14, MenuSkin.MUTED))
	row.add_child(titles)
	row.add_child(_make_spacer())
	nickname_label = _label("", 18, MenuSkin.TEXT)
	nickname_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	nickname_label.custom_minimum_size = Vector2(220.0, 0.0)
	row.add_child(nickname_label)
	return row


func _build_column(title: String, english: String) -> VBoxContainer:
	var column := VBoxContainer.new()
	column.add_theme_constant_override("separation", 10)
	column.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var heading := HBoxContainer.new()
	heading.add_theme_constant_override("separation", 8)
	heading.add_child(_label(title, 17, MenuSkin.TEXT))
	heading.add_child(_label(english, 12, MenuSkin.MUTED))
	column.add_child(heading)
	return column


## 一列装备卡；返回按 catalog 顺序创建的按钮数组（测试按顺序点击）。
func _build_card_list(column: VBoxContainer, entries: Array, primary: bool) -> Array[Button]:
	var buttons: Array[Button] = []
	for entry: Dictionary in entries:
		var weapon_id := str(entry["id"])
		var card := _make_card(entry)
		if primary:
			card.pressed.connect(select_primary.bind(weapon_id))
		else:
			card.pressed.connect(select_secondary.bind(weapon_id))
		column.add_child(card)
		buttons.append(card)
	return buttons


func _make_card(entry: Dictionary) -> Button:
	var card := Button.new()
	card.custom_minimum_size = Vector2(CARD_WIDTH, CARD_HEIGHT)
	card.focus_mode = Control.FOCUS_NONE
	card.text = ""
	card.add_theme_stylebox_override("normal", _card_style(false, false))
	card.add_theme_stylebox_override("hover", _card_style(false, true))
	card.add_theme_stylebox_override("pressed", _card_style(false, true))
	card.add_theme_stylebox_override("focus", _card_style(false, false))

	var margin := MarginContainer.new()
	margin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	margin.add_theme_constant_override("margin_left", 14)
	margin.add_theme_constant_override("margin_right", 14)
	margin.add_theme_constant_override("margin_top", 10)
	margin.add_theme_constant_override("margin_bottom", 10)
	margin.mouse_filter = Control.MOUSE_FILTER_IGNORE
	card.add_child(margin)

	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 14)
	row.mouse_filter = Control.MOUSE_FILTER_IGNORE
	margin.add_child(row)
	var icon := TextureRect.new()
	icon.custom_minimum_size = Vector2(132.0, 64.0)
	icon.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	icon.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	icon.texture = load(str(entry["icon"])) as Texture2D
	icon.mouse_filter = Control.MOUSE_FILTER_IGNORE
	row.add_child(icon)
	var texts := VBoxContainer.new()
	texts.add_theme_constant_override("separation", 3)
	texts.alignment = BoxContainer.ALIGNMENT_CENTER
	texts.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	texts.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var name_label := _label(str(entry["name"]), 19, MenuSkin.TEXT)
	name_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_LEFT
	var meta_label := _label(str(entry["meta"]), 13, MenuSkin.MUTED)
	meta_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_LEFT
	texts.add_child(name_label)
	texts.add_child(meta_label)
	row.add_child(texts)
	return card


func _build_stats_panel() -> Control:
	var panel := PanelContainer.new()
	panel.custom_minimum_size = Vector2(CARD_WIDTH, 132.0)
	var style := StyleBoxFlat.new()
	style.bg_color = Color(0.045, 0.055, 0.075, 0.96)
	style.border_color = MenuSkin.LINE
	style.set_border_width_all(1)
	style.set_corner_radius_all(6)
	style.set_content_margin_all(14)
	panel.add_theme_stylebox_override("panel", style)
	var column := VBoxContainer.new()
	column.add_theme_constant_override("separation", 8)
	panel.add_child(column)
	column.add_child(_label("生涯 / CAREER", 12, MenuSkin.GOLD))
	stats_label = _label("", 15, MenuSkin.TEXT)
	stats_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_LEFT
	stats_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	column.add_child(stats_label)
	return panel


func _build_footer() -> Control:
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 12)
	row.add_child(_label("点击卡片装备 · 立即写入存档", 13, MenuSkin.MUTED))
	row.add_child(_make_spacer())
	back_button = Button.new()
	back_button.text = "返回主菜单"
	back_button.custom_minimum_size = Vector2(150.0, 40.0)
	back_button.focus_mode = Control.FOCUS_NONE
	back_button.add_theme_font_size_override("font_size", 16)
	back_button.add_theme_stylebox_override("normal", _button_style(Color(0.10, 0.11, 0.15, 0.96), MenuSkin.LINE))
	back_button.add_theme_stylebox_override("hover", _button_style(Color(0.16, 0.17, 0.20, 1.0), MenuSkin.GOLD))
	back_button.add_theme_stylebox_override("pressed", _button_style(Color(0.22, 0.20, 0.14, 1.0), MenuSkin.GOLD))
	row.add_child(back_button)
	return row


func _build_rule() -> ColorRect:
	var rule := ColorRect.new()
	rule.color = MenuSkin.LINE
	rule.custom_minimum_size = Vector2(0.0, 1.0)
	rule.mouse_filter = Control.MOUSE_FILTER_IGNORE
	return rule


func _label(text: String, font_size: int, color: Color) -> Label:
	var label := Label.new()
	label.text = text
	label.add_theme_font_size_override("font_size", font_size)
	label.add_theme_color_override("font_color", color)
	return label


func _make_spacer() -> Control:
	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	spacer.mouse_filter = Control.MOUSE_FILTER_IGNORE
	return spacer


func _panel_style() -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color = MenuSkin.INK
	style.border_color = Color(0.34, 0.30, 0.22, 0.95)
	style.set_border_width_all(1)
	style.set_corner_radius_all(8)
	return style


func _card_style(selected: bool, hovered: bool) -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color = Color(0.13, 0.12, 0.09, 0.98) if selected else Color(0.075, 0.085, 0.11, 0.96)
	if hovered and not selected:
		style.bg_color = Color(0.12, 0.13, 0.17, 1.0)
	style.border_color = MenuSkin.GOLD if selected else (Color(0.42, 0.46, 0.52) if hovered else MenuSkin.LINE)
	style.set_border_width_all(2 if selected else 1)
	style.set_corner_radius_all(6)
	return style


func _button_style(background: Color, border: Color) -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color = background
	style.border_color = border
	style.set_border_width_all(1)
	style.set_corner_radius_all(4)
	style.set_content_margin_all(8)
	return style


func _refresh() -> void:
	if account == null:
		return
	nickname_label.text = "昵称：%s" % str(account.display_name)
	stats_label.text = "比赛 %d · 胜 %d · 击杀 %d · 阵亡 %d · K/D %.2f" % [
		int(account.stats["matches"]), int(account.stats["wins"]), int(account.stats["kills"]),
		int(account.stats["deaths"]), float(account.kd_ratio())]
	_highlight(primary_buttons, PRIMARY_WEAPONS, str(account.loadout["primary"]))
	_highlight(secondary_buttons, SECONDARY_WEAPONS, str(account.loadout["secondary"]))


func _highlight(buttons: Array[Button], entries: Array, current_id: String) -> void:
	for index in buttons.size():
		var selected := index < entries.size() and str(entries[index]["id"]) == current_id
		buttons[index].add_theme_stylebox_override("normal", _card_style(selected, false))
		buttons[index].add_theme_stylebox_override("hover", _card_style(selected, true))
		buttons[index].add_theme_stylebox_override("pressed", _card_style(selected, true))
