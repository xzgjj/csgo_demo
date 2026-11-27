class_name BuyMenu
extends CanvasLayer
## 购买菜单（SPEC-06 BUY-07）：目录来自内核（价格/限购/阵营单一事实源），
## 列表模式 = 左侧分类 + 右侧商品按钮；轮盘模式 = 6 扇区环形选择。两种模式共用同一批按钮回调。
## 只发信号（purchase_requested/refund_requested/rebuy_requested），不扣钱、不改内核状态。

signal purchase_requested(item_id: String, price: int)
signal refund_requested(item_id: String)
signal rebuy_requested

const UNAFFORDABLE_COLOR := Color(0.52, 0.54, 0.58)
const TAB_COLOR := Color(0.82, 0.86, 0.92)
const TAB_ACTIVE_COLOR := Color(1.0, 0.85, 0.45)
const FALLBACK := {
	"手枪": [{"id": "glock_18", "name": "Glock-18", "price": 200}],
	"步枪": [{"id": "ak47", "name": "AK-47", "price": 2700}],
}
const LOADOUT_IDS := {"ak47": "ak", "m4a4": "m4", "awp": "awp"}
## 分类名由宿主维护（GDExtension 只回传 category 下标，避免 DLL 字符串编码差异）。
const CATEGORY_NAMES := ["手枪", "冲锋枪", "步枪", "重型", "装备", "投掷物"]

var balance := 0
var can_buy := true
var balance_label: Label
var team_label: Label
var hint_label: Label
var close_button: Button
var rebuy_button: Button
var refund_button: Button
var mode_button: Button
var wheel: BuyWheel
var list_mode := true
var catalog: Dictionary = {}
var _tabs: HBoxContainer
var _item_box: VBoxContainer
var _tab_buttons: Dictionary = {}
var _item_buttons: Dictionary = {}
var _current := ""
var _message := ""
var _entry_checker := Callable()
const REASON_TEXT := {
	2: "购买窗口已关闭", 3: "不在购买区", 4: "余额不足", 5: "已达限购",
	6: "非本方阵营/未上架", 7: "已持有", 8: "不可退款", 9: "退款次数用尽",
	10: "条目无效", 11: "本回合已退款，禁止再买",
}

func build() -> void:
	layer = 5
	visible = false
	var box := UiFactory.build_card(self, "购买菜单", 560.0, 760.0)
	balance_label = UiFactory.add_label(box, "", 20)
	balance_label.add_theme_color_override("font_color", UiFactory.TEAM_T_COLOR)
	team_label = UiFactory.add_label(box, "", 14)
	team_label.add_theme_color_override("font_color", UiFactory.NEUTRAL_COLOR)
	_tabs = HBoxContainer.new()
	_tabs.add_theme_constant_override("separation", 6)
	box.add_child(_tabs)
	_item_box = VBoxContainer.new()
	_item_box.add_theme_constant_override("separation", 6)
	box.add_child(_item_box)
	hint_label = UiFactory.add_label(box, "", 14)
	hint_label.add_theme_color_override("font_color", UiFactory.NEUTRAL_COLOR)
	hint_label.visible = false
	var footer := HBoxContainer.new()
	footer.add_theme_constant_override("separation", 8)
	box.add_child(footer)
	mode_button = UiFactory.add_button(footer, "切换轮盘/列表 (Tab)")
	mode_button.custom_minimum_size = Vector2(210.0, 40.0)
	mode_button.pressed.connect(toggle_mode)
	rebuy_button = UiFactory.add_button(footer, "重购 (F)")
	rebuy_button.custom_minimum_size = Vector2(150.0, 40.0)
	rebuy_button.pressed.connect(func() -> void: rebuy_requested.emit())
	refund_button = UiFactory.add_button(footer, "退款 (R)")
	refund_button.custom_minimum_size = Vector2(150.0, 40.0)
	refund_button.pressed.connect(_refund_current)
	close_button = UiFactory.add_button(footer, "关闭 (B)")
	close_button.custom_minimum_size = Vector2(150.0, 40.0)
	close_button.pressed.connect(close)
	if catalog.is_empty():
		catalog = FALLBACK.duplicate(true)
	_build_tabs()
	_build_wheel()
	select_category(str(_category_names()[0]))
	_refresh_balance()

## 由 main 在内核就绪后注入目录（id → price/limit/sides/category）。
func set_catalog(entries: Array) -> void:
	var by_category: Dictionary = {}
	var names: Array[String] = []
	for raw: Dictionary in entries:
		var category_index := int(raw.get("category", 0))
		var cat := "其他"
		if category_index >= 0 and category_index < CATEGORY_NAMES.size():
			cat = str(CATEGORY_NAMES[category_index])
		if not by_category.has(cat):
			by_category[cat] = []
			names.append(cat)
		by_category[cat].append({
			"id": str(raw.get("id", "")), "name": _display_name(str(raw.get("id", ""))),
			"price": int(raw.get("price", 0)), "index": int(raw.get("index", 0)),
			"limit": int(raw.get("limit", 1)), "sides": int(raw.get("sides", 3)),
		})
	# 固定 6 扇区/标签顺序；空分类（如冲锋枪尚无商品）保留，显示“暂无商品”。
	var ordered: Dictionary = {}
	for name in CATEGORY_NAMES:
		ordered[name] = by_category.get(name, [])
	catalog = ordered
	var all_names: Array[String] = []
	for name in CATEGORY_NAMES:
		all_names.append(name)
	_rebuild_tabs(all_names)
	_rebuild_wheel(all_names)
	if not _current.is_empty():
		select_category(_current)
	else:
		select_category(str(_category_names()[0]))
	if not _current.is_empty():
		select_category(_current)
	else:
		select_category(str(_category_names()[0]))

func open(money: int) -> void:
	visible = true
	_message = ""
	set_balance(money)
	if _current.is_empty():
		select_category(str(_category_names()[0]))

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

## 团队经济条（SPEC-06 UI 顶部）；由 purchase.refresh 每帧同步。
func set_team_economy(team_t: int, team_ct: int) -> void:
	if team_label == null:
		return
	team_label.text = "团队经济  T $%d   CT $%d" % [maxi(team_t, 0), maxi(team_ct, 0)]

func set_can_buy(value: bool) -> void:
	if can_buy == value:
		return
	can_buy = value
	_refresh_items_state()
	_refresh_hint()

func show_message(text: String) -> void:
	_message = text
	_refresh_hint()

func message_text() -> String:
	return _message

func category_names() -> Array[String]:
	return _category_names()

func current_category() -> String:
	return _current

func select_category(category: String) -> void:
	if not catalog.has(category):
		return
	_current = category
	_rebuild_items()
	_refresh_tabs()

## 数字键 1-6 选分类（快捷键入口；越界忽略）。
func select_category_index(index: int) -> void:
	var names := _category_names()
	if index < 0 or index >= names.size():
		return
	select_category(names[index])

## R 键退款：当前分类第一件可退款物品（失败给提示，不静默）。
func request_refund_current() -> void:
	_refund_current()

func item_ids() -> Array[String]:
	var ids: Array[String] = []
	for item: Dictionary in catalog.get(_current, []):
		ids.append(str(item["id"]))
	return ids

func price_of(item_id: String) -> int:
	for category in catalog:
		for item: Dictionary in catalog[category]:
			if str(item["id"]) == item_id:
				return int(item["price"])
	return -1

func item_name(item_id: String) -> String:
	for category in catalog:
		for item: Dictionary in catalog[category]:
			if str(item["id"]) == item_id:
				return str(item["name"])
	return ""

func buyable(item_id: String) -> bool:
	if not can_buy:
		return false
	var price := price_of(item_id)
	if price < 0 or price > balance:
		return false
	return entry_reason(item_id) == 0

## 内核给出的禁用原因（0=可买；无 checker 时只按价格判断）。
func entry_reason(item_id: String) -> int:
	if not _entry_checker.is_valid():
		return 0
	var index := entry_index_of(item_id)
	if index < 0:
		return 10
	return int(_entry_checker.call(0, index))

## 由 main 注入内核校验函数（player, entry -> BuyResult 原因码）。
func set_entry_checker(checker: Callable) -> void:
	_entry_checker = checker
	_refresh_items_state()

func is_throwable(_item_id: String) -> bool:
	return false

func loadout_id_of(item_id: String) -> String:
	return str(LOADOUT_IDS.get(item_id, ""))

## 内核 catalog 下标（PurchaseFlow 用它下单；找不到返回 -1）。
func entry_index_of(item_id: String) -> int:
	for category in catalog:
		for item: Dictionary in catalog[category]:
			if str(item["id"]) == item_id:
				return int(item.get("index", -1))
	return -1

func buy_item(item_id: String) -> bool:
	if not buyable(item_id):
		return false
	purchase_requested.emit(item_id, price_of(item_id))
	return true

func refund_item(item_id: String) -> bool:
	if item_id.is_empty() or price_of(item_id) < 0 or not can_buy:
		return false
	refund_requested.emit(item_id)
	return true

func toggle_mode() -> void:
	list_mode = not list_mode
	_item_box.visible = list_mode
	wheel.visible = not list_mode
	if mode_button != null:
		mode_button.text = "切换列表 (Tab)" if not list_mode else "切换轮盘 (Tab)"

func button_of(item_id: String) -> Button:
	var found: Variant = _item_buttons.get(item_id, null)
	return found as Button

func _category_names() -> Array[String]:
	var names: Array[String] = []
	for key in catalog:
		names.append(str(key))
	return names

func _display_name(item_id: String) -> String:
	var names := {"ak47": "AK-47", "m4a4": "M4A4", "awp": "AWP", "usp_s": "USP-S",
		"glock_18": "Glock-18", "kevlar": "凯夫拉护甲", "helmet": "头盔",
		"defuse_kit": "拆弹器", "he": "高爆手雷", "flash": "闪光弹", "smoke": "烟雾弹"}
	return str(names.get(item_id, item_id))

func _build_tabs() -> void:
	_rebuild_tabs(_category_names())

func _rebuild_tabs(names: Array[String]) -> void:
	for child in _tabs.get_children():
		child.queue_free()
	_tab_buttons.clear()
	for name in names:
		var tab := Button.new()
		tab.text = name
		tab.custom_minimum_size = Vector2(88.0, 32.0)
		tab.pressed.connect(select_category.bind(name))
		_tabs.add_child(tab)
		_tab_buttons[name] = tab

func _build_wheel() -> void:
	_rebuild_wheel(_category_names())

func _rebuild_wheel(names: Array[String]) -> void:
	if wheel == null:
		wheel = BuyWheel.new()
		wheel.custom_minimum_size = Vector2(360.0, 300.0)
		wheel.size = Vector2(360.0, 300.0)
		wheel.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
		wheel.visible = false
		_item_box.get_parent().add_child(wheel)
	wheel.rebuild(names, Callable(self, "select_category"))

func _rebuild_items() -> void:
	for child in _item_box.get_children():
		child.queue_free()
	_item_buttons.clear()
	for item: Dictionary in catalog.get(_current, []):
		var item_id := str(item["id"])
		var label := "%s  $%d" % [str(item["name"]), int(item["price"])]
		var button := UiFactory.add_button(_item_box, label)
		button.pressed.connect(_on_item_pressed.bind(item_id))
		_item_buttons[item_id] = button
	if catalog.get(_current, []).is_empty():
		UiFactory.add_label(_item_box, "本分类暂无商品", 14)
	_refresh_items_state()
	_refresh_hint()

func _on_item_pressed(item_id: String) -> void:
	if buy_item(item_id):
		return
	if not can_buy:
		show_message("购买窗口已关闭")
	else:
		show_message(reason_text(entry_reason(item_id)))

func _refund_current() -> void:
	for item: Dictionary in catalog.get(_current, []):
		if refund_item(str(item["id"])):
			return
	show_message("当前分类没有可退款商品")

func _refresh_hint() -> void:
	if hint_label == null:
		return
	var text := ""
	if not can_buy:
		text = "购买窗口已关闭（冻结期可买）"
	elif not _message.is_empty():
		text = _message
	hint_label.text = text
	hint_label.visible = not text.is_empty()

func _refresh_tabs() -> void:
	for name: String in _tab_buttons:
		var tab: Button = _tab_buttons[name]
		tab.modulate = TAB_ACTIVE_COLOR if name == _current else TAB_COLOR

func _refresh_balance() -> void:
	if balance_label == null:
		return
	balance_label.text = "余额 $%d" % balance
	_refresh_items_state()

func _refresh_items_state() -> void:
	for item_id: String in _item_buttons:
		var button: Button = _item_buttons[item_id]
		var affordable := buyable(item_id)
		button.disabled = not affordable
		button.modulate = Color(1.0, 1.0, 1.0) if affordable else UNAFFORDABLE_COLOR
		button.tooltip_text = "" if affordable else reason_text(entry_reason(item_id))

## 原因码 → 玩家可读文案（0 返回空串）。
func reason_text(reason: int) -> String:
	if reason == 0:
		return ""
	return str(REASON_TEXT.get(reason, "不可购买"))
