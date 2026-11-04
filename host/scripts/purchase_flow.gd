class_name PurchaseFlow
extends RefCounted
## 购买/切武器接线（只做转发）：菜单信号 → 内核 Buy/buy_weapon → 余额刷新 → 账号装备记录 → 手模同步。
## 规则状态全部在内核：这里不判断价格是否够（由菜单灰显 + 内核拒绝双保险），也不写 WorldState。

const BUY_WEAPON_IDS := {"ak47": 1, "m4a4": 2, "awp": 3, "usp_s": 4, "glock_18": 5}
const PRIMARY_WEAPON_IDS: Array[int] = [1, 2, 3]
## 内核武器 id（1=AK,2=M4,3=AWP）→ 手模 id；手枪暂无独立模型，保持当前手模。
const WEAPON_MODELS := ["ak", "m4", "awp"]

var sim: RefCounted
var account
var menu
var viewmodel


func setup(sim_ref: RefCounted, account_ref: Variant, menu_ref: Node, viewmodel_ref: Node) -> void:
	sim = sim_ref
	account = account_ref
	menu = menu_ref
	viewmodel = viewmodel_ref


func toggle() -> void:
	if menu == null:
		return
	menu.toggle(int(sim.get_money(0)))


func is_open() -> bool:
	return menu != null and bool(menu.is_open())


## 余额每帧同步（内核 Buy/buy_weapon 是命令，下一 tick 才结算）。
func refresh() -> void:
	_refresh_balance()


## 菜单点击 → 内核购买；成功返回 true。武器走 buy_weapon（主/副槽由内核决定），装备走 Buy。
func on_purchase(item_id: String, price: int) -> bool:
	if sim == null or int(sim.get_money(0)) < price:
		return false
	var weapon_id: int = int(BUY_WEAPON_IDS.get(item_id, 0))
	var ok := false
	if weapon_id > 0:
		ok = bool(sim.buy_weapon(0, weapon_id))
		if ok:
			_record_loadout(weapon_id)
	else:
		ok = _buy_gear(item_id)
	_refresh_balance()
	return ok


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


func _buy_gear(item_id: String) -> bool:
	var before: int = int(sim.get_money(0))
	if item_id == "kevlar":
		sim.buy_armor(0)
	elif item_id == "kevlar_helmet":
		sim.buy_helmet(0)
	else:
		return false
	return int(sim.get_money(0)) < before


func _refresh_balance() -> void:
	if menu != null:
		menu.set_balance(int(sim.get_money(0)))
