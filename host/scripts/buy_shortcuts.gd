class_name BuyShortcuts
extends RefCounted
## 购买菜单快捷键（SPEC-06 BUY-06）：抽成独立模块，main 只做转发，保持单文件在 CI 行数预算内。
## 1-6 分类、Tab 轮盘/列表、F 重购、R 退款、B 关闭。

const CATEGORY_KEY_BASE := int(KEY_1)


static func handle(event: InputEvent, menu: Node, purchase: RefCounted) -> bool:
	if not (event is InputEventKey) or not event.pressed or event.echo:
		return false
	if menu == null or not menu.is_open():
		return false
	match event.keycode:
		KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6:
			menu.select_category_index(int(event.keycode) - CATEGORY_KEY_BASE)
		KEY_TAB:
			menu.toggle_mode()
		KEY_F:
			purchase.on_rebuy()
		KEY_R:
			menu.request_refund_current()
		KEY_B:
			menu.close()
		_:
			return false
	return true


static func toggle(owner: Node) -> void:
	if owner.purchase == null:
		return
	owner.purchase.toggle()
	owner.call("_sync_mouse_mode")
