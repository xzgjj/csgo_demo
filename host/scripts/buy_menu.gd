class_name BuyMenu
extends CanvasLayer
## 购买菜单（前端骨架）：分类（手枪/步枪/狙击/装备/手雷）+ 价格表 + 余额显示 + 买不起灰显。
## 点击只发 purchase_requested(item_id, price) 信号，不扣钱也不改内核状态；
## 内核 buy_weapon/购买命令绑定落地后由 main 把信号接到真实购买（模块 9 的后续）。
## 价格对齐 content/defs/weapons.json（AK 2700 / M4A4 3100 / AWP 4750 / 手枪 200），
## 装备与手雷取 CS:GO 参考值；内核价格 getter 落地后改为从内核读。

signal purchase_requested(item_id: String, price: int)

const CATEGORIES: Array[String] = ["手枪", "步枪", "狙击", "装备", "手雷"]
const CATALOG := {
	"手枪": [
		{"id": "usp_s", "name": "USP-S", "price": 200},
		{"id": "glock_18", "name": "Glock-18", "price": 200},
	],
	"步枪": [
		{"id": "ak47", "name": "AK-47", "price": 2700},
		{"id": "m4a4", "name": "M4A4", "price": 3100},
	],
	"狙击": [
		{"id": "awp", "name": "AWP", "price": 4750},
	],
	"装备": [
		{"id": "kevlar", "name": "凯夫拉护甲", "price": 650},
		{"id": "kevlar_helmet", "name": "护甲 + 头盔", "price": 1000},
	],
	"手雷": [
		{"id": "he", "name": "高爆手雷", "price": 300, "throwable": true},
		{"id": "flash", "name": "闪光弹", "price": 200, "throwable": true},
		{"id": "smoke", "name": "烟雾弹", "price": 300, "throwable": true},
	],
}
const TAB_COLOR := Color(0.82, 0.86, 0.92)
const TAB_ACTIVE_COLOR := Color(1.0, 0.85, 0.45)
const UNAFFORDABLE_COLOR := Color(0.52, 0.54, 0.58)
## 内核武器 id → AccountStore.loadout 的表现层 id（购买成功后写装备用）。
const LOADOUT_IDS := {"ak47": "ak", "m4a4": "m4", "awp": "awp"}

var balance := 0
## 购买窗口：仅内核 Freeze 阶段可买（由 PurchaseFlow 每帧同步）。
var can_buy := true
var balance_label: Label
var hint_label: Label
var close_button: Button
var _tab_row: HBoxContainer
var _item_box: VBoxContainer
var _tab_buttons: Dictionary = {}
var _item_buttons: Dictionary = {}
var _current := ""
var _message := ""


func build() -> void:
	layer = 5
	visible = false
	var box := UiFactory.build_card(self, "购买菜单", 600.0)
	balance_label = UiFactory.add_label(box, "", 20)
	balance_label.add_theme_color_override("font_color", UiFactory.TEAM_T_COLOR)
	_tab_row = HBoxContainer.new()
	_tab_row.alignment = BoxContainer.ALIGNMENT_CENTER
	_tab_row.add_theme_constant_override("separation", 8)
	box.add_child(_tab_row)
	for category in CATEGORIES:
		var tab := Button.new()
		tab.text = category
		tab.custom_minimum_size = Vector2(96.0, 34.0)
		tab.add_theme_font_size_override("font_size", 15)
		tab.pressed.connect(select_category.bind(category))
		_tab_row.add_child(tab)
		_tab_buttons[category] = tab
	_item_box = VBoxContainer.new()
	_item_box.add_theme_constant_override("separation", 8)
	box.add_child(_item_box)
	hint_label = UiFactory.add_label(box, "", 14)
	hint_label.add_theme_color_override("font_color", UiFactory.NEUTRAL_COLOR)
	hint_label.visible = false
	close_button = UiFactory.add_button(box, "关闭 (B)")
	close_button.pressed.connect(close)
	select_category(CATEGORIES[0])
	_refresh_balance()


func open(money: int) -> void:
	visible = true
	_message = ""
	set_balance(money)
	if _current.is_empty():
		select_category(CATEGORIES[0])


func close() -> void:
	visible = false


func toggle(money: int) -> void:
	if visible:
		close()
	else:
		open(money)


func is_open() -> bool:
	return visible


func set_balance(money: int) -> void:
	balance = maxi(money, 0)
	_refresh_balance()


## 购买窗口开关（Freeze=可买；Live/RoundEnd=灰显并给出原因提示）。
func set_can_buy(value: bool) -> void:
	if can_buy == value:
		return
	can_buy = value
	_refresh_items_state()
	_refresh_hint()


## 菜单内可见提示（购买成功/被拒原因）。
func show_message(text: String) -> void:
	_message = text
	_refresh_hint()


func message_text() -> String:
	return _message


func category_names() -> Array[String]:
	return CATEGORIES.duplicate()


func current_category() -> String:
	return _current


func select_category(category: String) -> void:
	if not (category in CATEGORIES):
		return
	_current = category
	_rebuild_items()
	_refresh_tabs()


## 当前分类的商品 id（按价格表顺序）。
func item_ids() -> Array[String]:
	var ids: Array[String] = []
	if _current.is_empty():
		return ids
	for item: Dictionary in CATALOG[_current]:
		ids.append(str(item["id"]))
	return ids


func price_of(item_id: String) -> int:
	for category in CATEGORIES:
		for item: Dictionary in CATALOG[category]:
			if str(item["id"]) == item_id:
				return int(item["price"])
	return -1


func item_name(item_id: String) -> String:
	for category in CATEGORIES:
		for item: Dictionary in CATALOG[category]:
			if str(item["id"]) == item_id:
				return str(item["name"])
	return ""


func buyable(item_id: String) -> bool:
	if not can_buy:
		return false
	if is_throwable(item_id):
		# 手雷不是“购买”而是对局内投掷（G/F/V），菜单里只做信息展示。
		return false
	var price := price_of(item_id)
	return price >= 0 and price <= balance


func is_throwable(item_id: String) -> bool:
	for category in CATEGORIES:
		for item: Dictionary in CATALOG[category]:
			if str(item["id"]) == item_id:
				return bool(item.get("throwable", false))
	return false


## 内核武器 id → 装备主武器 id（非主武器返回空串）；购买成功后写 AccountStore 用。
func loadout_id_of(item_id: String) -> String:
	return str(LOADOUT_IDS.get(item_id, ""))


## 点击/调用购买：买得起才发 purchase_requested，返回是否已发请求（不扣钱，等内核）。
func buy_item(item_id: String) -> bool:
	if not buyable(item_id):
		return false
	purchase_requested.emit(item_id, price_of(item_id))
	return true


func button_of(item_id: String) -> Button:
	var button: Variant = _item_buttons.get(item_id, null)
	return button as Button


func _rebuild_items() -> void:
	for child in _item_box.get_children():
		child.queue_free()
	_item_buttons.clear()
	if _current.is_empty():
		return
	for item: Dictionary in CATALOG[_current]:
		var item_id := str(item["id"])
		var label := "%s  $%d" % [str(item["name"]), int(item["price"])]
		if bool(item.get("throwable", false)):
			label += "  · 对局内投掷"
		var button := UiFactory.add_button(_item_box, label)
		button.pressed.connect(_on_item_pressed.bind(item_id))
		_item_buttons[item_id] = button
	_refresh_items_state()
	_refresh_hint()


func _on_item_pressed(item_id: String) -> void:
	if not buy_item(item_id):
		if not can_buy:
			show_message("购买时间已过（仅冻结期与开局 5 秒内可购买）")
		elif not is_throwable(item_id) and price_of(item_id) > balance:
			show_message("余额不足：%s 需要 $%d" % [item_name(item_id), price_of(item_id)])


## 提示优先级：购买窗口关闭 > 手动消息 > 手雷说明。
func _refresh_hint() -> void:
	if hint_label == null:
		return
	var text := ""
	if not can_buy:
		text = "购买时间已过（仅冻结期与开局 5 秒内可购买）"
	elif not _message.is_empty():
		text = _message
	elif _current == "手雷":
		text = "手雷不在购买菜单买：对局内按 G / F / V 投掷"
	hint_label.text = text
	hint_label.visible = not text.is_empty()


func _refresh_tabs() -> void:
	for category in CATEGORIES:
		var tab: Button = _tab_buttons[category]
		tab.modulate = TAB_ACTIVE_COLOR if category == _current else TAB_COLOR


func _refresh_balance() -> void:
	if balance_label == null:
		return
	balance_label.text = "余额 $%d" % balance
	_refresh_items_state()


func _refresh_items_state() -> void:
	for item_id in _item_buttons.keys():
		var button: Button = _item_buttons[item_id]
		var affordable := buyable(str(item_id))
		button.disabled = not affordable
		button.modulate = Color(1.0, 1.0, 1.0) if affordable else UNAFFORDABLE_COLOR
