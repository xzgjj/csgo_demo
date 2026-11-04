class_name LoadoutPage
extends CanvasLayer
## 物品栏 / 装备页（官方风格简化）：主武器槽位 + 倍镜信息 + 昵称；保存到 AccountStore。

const WEAPONS := [
	{"id": "ak", "name": "AK-47", "scope": 1.0, "price": 2700},
	{"id": "m4", "name": "M4A4", "scope": 1.0, "price": 3100},
	{"id": "awp", "name": "AWP", "scope": 4.0, "price": 4750},
]

var account: RefCounted
var info_label: Label
var back_button: Button
var _buttons: Array[Button] = []


func build(account_store: RefCounted) -> void:
	account = account_store
	layer = 6
	visible = false
	var box := UiFactory.build_card(self, "物品栏 / 装备", 440.0)
	info_label = UiFactory.add_label(box, "", 15)
	for weapon in WEAPONS:
		var scope_text := "%dx" % int(weapon["scope"]) if float(weapon["scope"]) > 1.0 else "机瞄"
		var button := UiFactory.add_button(box, "%s · %s · $%d" % [weapon["name"], scope_text,
				int(weapon["price"])])
		button.pressed.connect(_select_weapon.bind(str(weapon["id"])))
		_buttons.append(button)
	back_button = UiFactory.add_button(box, "返回主菜单")
	_refresh()


func open() -> void:
	visible = true
	_refresh()


func close() -> void:
	visible = false


func _select_weapon(weapon_id: String) -> void:
	if account == null:
		return
	account.set_primary(weapon_id)
	_refresh()


func _refresh() -> void:
	if account == null:
		return
	info_label.text = "昵称：%s   当前主武器：%s   比赛 %d / 击杀 %d / 阵亡 %d" % [
			account.display_name, str(account.loadout["primary"]), int(account.stats["matches"]),
			int(account.stats["kills"]), int(account.stats["deaths"])]
	for button in _buttons:
		button.modulate = Color(1, 1, 1)
	var current := str(account.loadout["primary"])
	for index in WEAPONS.size():
		if str(WEAPONS[index]["id"]) == current:
			_buttons[index].modulate = Color(1.0, 0.85, 0.5)
