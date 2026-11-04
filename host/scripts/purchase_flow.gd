class_name PurchaseFlow
extends RefCounted
## 购买/切武器接线（只做转发）：菜单信号 → 内核 Buy/buy_weapon → 余额刷新 → 账号装备记录 → 手模同步。
## 规则状态全部在内核：这里不判断价格是否够（由菜单灰显 + 内核拒绝双保险），也不写 WorldState。

const BUY_WEAPON_IDS := {"ak47": 1, "m4a4": 2, "awp": 3, "usp_s": 4, "glock_18": 5}
const PRIMARY_WEAPON_IDS: Array[int] = [1, 2, 3]
## 内核武器 id（1=AK,2=M4,3=AWP,4=USP,5=Glock）→ 手模 id。
const WEAPON_MODELS := ["ak", "m4", "awp", "usp", "glock"]

var sim: RefCounted
var account
var menu
var viewmodel
## 截图/预览用：关闭后不再每帧把手模拉回内核武器（weapon_0..2 三把枪展示机位）。
var auto_sync := true
## 上一笔购买待内核确认（Buy/buy_weapon 是命令，tick 后才结算）。
var _pending: Dictionary = {}


func setup(sim_ref: RefCounted, account_ref: Variant, menu_ref: Node, viewmodel_ref: Node) -> void:
	sim = sim_ref
	account = account_ref
	menu = menu_ref
	viewmodel = viewmodel_ref


func toggle() -> void:
	if menu == null:
		return
	menu.toggle(int(sim.get_money(0)))
	refresh()


func is_open() -> bool:
	return menu != null and bool(menu.is_open())


## 每帧同步：余额 + 购买窗口（内核只允许 Freeze 阶段购买，菜单需要同步灰显/提示）。
func refresh() -> void:
	_refresh_balance()
	if menu != null and menu.has_method("set_can_buy"):
		menu.set_can_buy(_can_buy())


## 菜单点击 → 内核购买；成功返回 true。武器走 buy_weapon（主/副槽由内核决定），装备走 Buy。
func on_purchase(item_id: String, price: int) -> bool:
	if sim == null:
		return false
	if not _can_buy():
		_message("购买时间已过（仅冻结期与开局 5 秒内可购买）")
		return false
	if int(sim.get_money(0)) < price:
		_message("余额不足")
		return false
	var weapon_id: int = int(BUY_WEAPON_IDS.get(item_id, 0))
	var ok := false
	if weapon_id > 0:
		ok = bool(sim.buy_weapon(0, weapon_id))
		if ok:
			_record_loadout(weapon_id)
	else:
		ok = _buy_gear(item_id)
	if ok:
		_pending = {"name": _item_name(item_id), "money": int(sim.get_money(0)),
				"weapon": int(sim.get_weapon_id(0))}
	_refresh_balance()
	return ok


## 内核 tick 后校验上一笔购买：金钱减少或武器变化=成功；否则给出失败原因（不再静默失败）。
func after_tick() -> void:
	if _pending.is_empty():
		return
	var name := str(_pending.get("name", "物品"))
	var money_before := int(_pending.get("money", 0))
	var weapon_before := int(_pending.get("weapon", 0))
	var bought := int(sim.get_money(0)) < money_before or int(sim.get_weapon_id(0)) != weapon_before
	_message("已购买 %s" % name if bought else "购买失败：%s" % name)
	_pending.clear()
	refresh()


func switch_weapon(index: int) -> void:
	if sim == null or viewmodel == null:
		return
	sim.switch_weapon(0, index)
	_sync_viewmodel()


## 手模跟随内核当前武器（视觉预览/无模型时保持不变）。
func sync_viewmodel() -> void:
	_sync_viewmodel()


func _sync_viewmodel() -> void:
	var weapon_id: int = int(sim.get_weapon_id(0))
	var model_index: int = weapon_id - 1
	if model_index >= 0 and model_index < WEAPON_MODELS.size():
		viewmodel.select_weapon(WEAPON_MODELS[model_index])


func _record_loadout(weapon_id: int) -> void:
	if account == null:
		return
	var model_index: int = weapon_id - 1
	if model_index < 0 or model_index >= WEAPON_MODELS.size():
		return
	if weapon_id in PRIMARY_WEAPON_IDS:
		account.set_primary(WEAPON_MODELS[model_index])
	else:
		account.set_secondary(WEAPON_MODELS[model_index])


func _buy_gear(item_id: String) -> bool:
	var before: int = int(sim.get_money(0))
	if item_id == "kevlar":
		sim.buy_armor(0)
	elif item_id == "kevlar_helmet":
		sim.buy_helmet(0)
	else:
		return false
	return int(sim.get_money(0)) < before


## 购买窗口以内核为准（冻结期 + Live 开局宽限）。
func _can_buy() -> bool:
	if sim == null:
		return false
	if sim.has_method("can_buy"):
		return bool(sim.can_buy(0))
	return int(sim.get_round_phase()) == 0


func _message(text: String) -> void:
	if menu != null and menu.has_method("show_message"):
		menu.show_message(text)


func _item_name(item_id: String) -> String:
	if menu != null and menu.has_method("item_name"):
		return str(menu.item_name(item_id))
	return item_id


func _refresh_balance() -> void:
	if menu != null:
		menu.set_balance(int(sim.get_money(0)))
