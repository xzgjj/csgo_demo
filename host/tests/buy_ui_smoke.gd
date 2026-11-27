extends "res://tests/scene_test.gd"
## SPEC-06 BUY-07/BUY-06：购买菜单目录/阵营灰显/退款/快捷键/轮盘双模式（宿主 UI 回归）。
## 用法：godot --headless --path host --script res://tests/buy_ui_smoke.gd

const Shortcuts = preload("res://scripts/buy_shortcuts.gd")
const REASON_NOT_ALLOWED := 6
const REASON_REFUNDED := 11


func _run() -> void:
	main.call("_start_match")
	for i in 3:
		await physics_frame
	main._toggle_buy()
	await process_frame
	_check_catalog()
	_check_disable_reasons()
	_check_modes()
	await _check_shortcuts()
	await _check_refund()
	main._toggle_buy()
	finish("BUY_UI_SMOKE")


func _check_catalog() -> void:
	var cats: Array = main.buy_menu.category_names()
	expect(cats.size() == 6, "购买分类应为 6，实际 %d" % cats.size())
	expect(str(cats[0]) == "手枪" and str(cats[5]) == "投掷物", "分类顺序应为 手枪..投掷物")
	var total := 0
	for name in cats:
		main.buy_menu.select_category(str(name))
		total += main.buy_menu.item_ids().size()
	expect(total == 11, "目录条目应为 11，实际 %d" % total)
	main.buy_menu.select_category("冲锋枪")
	expect(main.buy_menu.item_ids().is_empty(), "冲锋枪空分类不应有商品")
	main.buy_menu.select_category("步枪")
	expect(main.buy_menu.price_of("ak47") == 2700, "AK-47 目录价应为 2700")
	expect(main.buy_menu.price_of("m4a4") == 3100, "M4A4 目录价应为 3100")


func _check_disable_reasons() -> void:
	main.buy_menu.select_category("步枪")
	expect(int(main.buy_menu.entry_reason("m4a4")) == REASON_NOT_ALLOWED,
			"T 方购买 M4A4 应提示非本方阵营")
	expect(not main.buy_menu.buyable("m4a4"), "T 方 M4A4 应灰显")
	expect(bool(main.buy_menu.button_of("m4a4").disabled), "M4A4 按钮应为禁用态")
	main.buy_menu.select_category("重型")
	expect(main.buy_menu.buyable("awp"), "T 方 AWP 应可买")
	main.buy_menu.select_category("手枪")
	expect(int(main.buy_menu.entry_reason("glock_18")) == 7, "默认持有 Glock 应提示已持有")


func _check_modes() -> void:
	expect(main.buy_menu.list_mode, "默认应为列表模式")
	main.buy_menu.toggle_mode()
	expect(not main.buy_menu.list_mode, "Tab 应切到轮盘模式")
	expect(bool(main.buy_menu.wheel.visible), "轮盘应可见")
	expect(main.buy_menu.wheel.get_child_count() == 6, "轮盘应有 6 个扇区按钮")
	main.buy_menu.toggle_mode()
	expect(main.buy_menu.list_mode and not main.buy_menu.wheel.visible, "再按 Tab 应回到列表")


func _check_shortcuts() -> void:
	expect(_send_key(KEY_6), "数字键 6 应被购买快捷键消费")
	await process_frame
	expect(main.buy_menu.current_category() == "投掷物", "数字键 6 应选中投掷物")
	expect(_send_key(KEY_1), "数字键 1 应被购买快捷键消费")
	await process_frame
	expect(main.buy_menu.current_category() == "手枪", "数字键 1 应选中手枪")
	_send_key(KEY_F)
	await process_frame
	_send_key(KEY_B)
	await process_frame
	expect(not main.buy_menu.is_open(), "B 应关闭购买菜单")
	main._toggle_buy()
	await process_frame


func _send_key(code: Key) -> bool:
	var event := InputEventKey.new()
	event.keycode = code
	event.physical_keycode = code
	event.pressed = true
	var got := bool(Shortcuts.handle(event, main.buy_menu, main.purchase))
	return got


func _check_refund() -> void:
	main.buy_menu.select_category("重型")
	var before: int = int(main.sim.get_money(0))
	expect(bool(main.purchase.on_purchase("awp", 4750)), "冻结期买 AWP 应成功")
	await physics_frame
	await physics_frame
	expect(int(main.sim.get_money(0)) == before - 4750, "买 AWP 应扣 4750")
	expect(_send_key(KEY_R), "R 应被购买快捷键消费")
	await physics_frame
	await physics_frame
	expect(int(main.sim.get_money(0)) == before, "退款应全额返还 4750")
	expect(int(main.buy_menu.entry_reason("awp")) == REASON_REFUNDED,
			"退款后本回合应禁止再买同一物品")
	expect(main.buy_menu.button_of("awp").tooltip_text.contains("已退款"),
			"AWP 按钮应显示已退款原因，实际 %s" % main.buy_menu.button_of("awp").tooltip_text)
