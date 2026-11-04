class_name LoadoutPage
extends CanvasLayer
## 物品栏 / 装备页：昵称 + 主武器（AK/M4/AWP）+ 副武器（USP/Glock）+ 生涯统计。
## 选择即通过 AccountStore 持久化（点击 → set_primary/set_secondary → save）；页面只读 store 数据。

const PRIMARY_WEAPONS := [
	{"id": "ak", "name": "AK-47"},
	{"id": "m4", "name": "M4A4"},
	{"id": "awp", "name": "AWP"},
]
const SECONDARY_WEAPONS := [
	{"id": "usp", "name": "USP-S"},
	{"id": "glock", "name": "Glock-18"},
]
const SELECTED_COLOR := Color(1.0, 0.85, 0.45)
const NORMAL_COLOR := Color(1.0, 1.0, 1.0)

var account: RefCounted
var nickname_label: Label
var stats_label: Label
var primary_buttons: Array[Button] = []
var secondary_buttons: Array[Button] = []
var back_button: Button


func build(account_store: RefCounted) -> void:
	account = account_store
	layer = 6
	visible = false
	var box := UiFactory.build_card(self, "物品栏 / 装备", 520.0)
	nickname_label = UiFactory.add_label(box, "", 20)
	nickname_label.add_theme_color_override("font_color", UiFactory.TEAM_T_COLOR)
	UiFactory.add_label(box, "主武器（点击切换，立即保存）", 15)
	primary_buttons = _build_choice_row(box, PRIMARY_WEAPONS, true)
	UiFactory.add_label(box, "副武器（点击切换，立即保存）", 15)
	secondary_buttons = _build_choice_row(box, SECONDARY_WEAPONS, false)
	stats_label = UiFactory.add_label(box, "", 15)
	back_button = UiFactory.add_button(box, "返回主菜单")
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


## 一行装备按钮（主/副武器共用）；返回按 catalog 顺序创建的按钮数组。
func _build_choice_row(box: VBoxContainer, entries: Array, primary: bool) -> Array[Button]:
	var buttons: Array[Button] = []
	var row := HBoxContainer.new()
	row.alignment = BoxContainer.ALIGNMENT_CENTER
	row.add_theme_constant_override("separation", 10)
	box.add_child(row)
	for entry: Dictionary in entries:
		var button := Button.new()
		button.text = str(entry["name"])
		button.custom_minimum_size = Vector2(112.0, 36.0)
		button.add_theme_font_size_override("font_size", 16)
		var weapon_id := str(entry["id"])
		if primary:
			button.pressed.connect(select_primary.bind(weapon_id))
		else:
			button.pressed.connect(select_secondary.bind(weapon_id))
		row.add_child(button)
		buttons.append(button)
	return buttons


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
		buttons[index].modulate = SELECTED_COLOR if selected else NORMAL_COLOR
