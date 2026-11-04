extends "res://tests/scene_test.gd"
## 宿主 UI 回归（模块 1/8/9/10）：出生点队友存活条、AWP 4x/机瞄、购买菜单、物品栏账号。
## 从 input_smoke 拆出（单文件 ≤400 行 CI 预算）；由 tools/verify/run_scenarios.py 的输入契约一起跑。
## 用法：godot --headless --path host --script res://tests/host_ui_smoke.gd


func _run() -> void:
	await process_frame
	main.call("_start_play")
	await process_frame
	await _check_spawn_hud()
	await _check_scope()
	await _check_buy_menu()
	await _check_match_economy_and_weapons()
	await _check_mouse_lock()
	await _check_buy_window()
	finish("HOST_UI_SMOKE")


func _check_spawn_hud() -> void:
	main.call("_start_match")
	for i in 3:
		await physics_frame
	if not bool(main.radar.visible):
		failures.append("出生点雷达/队友存活条未显示")
	var lamps: Array[bool] = main.radar.squad_lamps()
	var alive_expected: int = int(main.sim.get_alive_count(1))
	if lamps.size() != 5:
		failures.append("队友存活灯应为 5 盏，实际 %d" % lamps.size())
	var alive_lamps := 0
	for lit: bool in lamps:
		if lit:
			alive_lamps += 1
	if alive_lamps != alive_expected:
		failures.append("存活灯数与 alive_count(1) 不一致：灯 %d / 内核 %d" % [
			alive_lamps, alive_expected])
	if int(main.radar.squad_alive_count()) != alive_expected:
		failures.append("存活数字与 alive_count(1) 不一致：%d / %d" % [
			int(main.radar.squad_alive_count()), alive_expected])
	if alive_expected != 5:
		failures.append("出生点队友应 5 人存活，实际 %d" % alive_expected)


## 模块 8：AWP 4x 开镜 overlay + HUD 倍率；AK 机瞄只有提示不遮罩。
func _check_scope() -> void:
	main.call("_start_match")
	await process_frame
	main.playing = false
	main.aiming = true
	var base_fov: float = float(main.settings.fov)
	# ① 内核口径：初始 AK = 机瞄 1.0x（倍率由内核武器表决定，手模不能伪造）
	var kernel_mag: float = float(main.weapon_magnification())
	if absf(kernel_mag - 1.0) > 0.01:
		failures.append("AK 内核倍率应为 1.0，实际 %.2f" % kernel_mag)
	# ② 表现层契约：倍率 4.0 → 镜筒遮罩 + FOV=base/4 + HUD 4.0x
	main.scope.sync(true, 4.0, "AWP")
	main.camera.fov = main.scope.target_fov(base_fov)
	main._update_view()
	main._update_hud()
	await process_frame
	if not bool(main.scope.overlay.visible):
		failures.append("AWP 开镜 overlay 未显示")
	if not bool(main.scope.is_scoped()):
		failures.append("AWP 开镜未判定为倍镜")
	if not str(main.scope.overlay.scope_text()).contains("4.0x"):
		failures.append("HUD 倍率文字不含 4.0x：%s" % str(main.scope.overlay.scope_text()))
	if bool(main.hud.crosshair.visible):
		failures.append("开镜时准星应隐藏")
	if absf(float(main.camera.fov) - base_fov / 4.0) > 0.5:
		failures.append("AWP 开镜 FOV 应为 base/4：%.1f / base %.1f" % [
			float(main.camera.fov), base_fov])
	# ③ 机瞄 1.0 → 不遮罩 + FOV=base/1.15
	main.scope.sync(true, 1.0, "AK-47")
	main._update_view()
	await process_frame
	if bool(main.scope.is_scoped()):
		failures.append("AK 机瞄被误判为倍镜")
	if absf(main.scope.target_fov(base_fov) - base_fov / 1.15) > 0.5:
		failures.append("AK 机瞄 FOV 应为 base/1.15：%.1f" % main.scope.target_fov(base_fov))
	if not str(main.scope.overlay.scope_text()).contains("机瞄"):
		failures.append("HUD 机瞄文字异常：%s" % str(main.scope.overlay.scope_text()))
	main.aiming = false
	main.playing = true
	main.scope.sync(false, 1.0, "AK-47")
	main.camera.fov = base_fov


## 模块 9：购买菜单分类/价格/余额/灰显 + 购买扣钱/装备进内核（800 起始金 → 只买得起手枪）。
func _check_buy_menu() -> void:
	main.call("_start_play")
	await process_frame
	main.purchase.toggle()
	await process_frame
	if not bool(main.buy_menu.is_open()):
		failures.append("B 未打开购买菜单")
	if int(main.buy_menu.balance) != int(main.sim.get_money(0)):
		failures.append("菜单余额与内核不一致：%d / %d" % [
			int(main.buy_menu.balance), int(main.sim.get_money(0))])
	if not bool(main.buy_menu.buyable("glock_18")):
		failures.append("800 起始金应可买 Glock-18")
	if bool(main.buy_menu.buyable("ak47")):
		failures.append("800 起始金不应可买 AK-47（应灰显）")
	var money_before: int = int(main.sim.get_money(0))
	if bool(main.buy_menu.buy_item("ak47")):
		failures.append("买不起的 AK-47 不应发出购买请求")
	if int(main.sim.get_money(0)) != money_before:
		failures.append("买不起时余额不应变化")
	if not bool(main.buy_menu.buy_item("glock_18")):
		failures.append("买 Glock-18 应发出购买请求")
	# 内核 Buy 是命令：下一 tick 才结算（真实对局里由 _physics_process 推进）。
	await physics_frame
	await physics_frame
	if int(main.sim.get_money(0)) != money_before - 200:
		failures.append("买 Glock-18 后余额应为 %d，实际 %d" % [
			money_before - 200, int(main.sim.get_money(0))])
	if int(main.buy_menu.balance) != int(main.sim.get_money(0)):
		failures.append("购买后菜单余额未刷新：%d" % int(main.buy_menu.balance))
	main.buy_menu.close()
	await process_frame
	if bool(main.buy_menu.is_open()):
		failures.append("购买菜单未关闭")


func _check_loadout() -> void:
	main.show_loadout_page()
	await process_frame
	if not bool(main.loadout_page.visible):
		failures.append("物品栏页面未打开")
	main.account.set_primary("awp")
	await process_frame
	if str(main.account.loadout["primary"]) != "awp":
		failures.append("物品栏主武器选择未持久化")
	main.close_loadout_page()
	await process_frame
	if bool(main.loadout_page.visible):
		failures.append("物品栏页面未关闭")


## 模块 7 回归：5v5 演示局出生金钱够买步枪/狙击；2 号(M4)/3 号(AWP) 购买后内核/手模/HUD/倍镜一致。
func _check_match_economy_and_weapons() -> void:
	main.call("_start_match")
	for i in 3:
		await physics_frame
	if int(main.sim.get_money(0)) != 16000:
		failures.append("演示局出生金钱应为 16000，实际 %d" % int(main.sim.get_money(0)))
	if not bool(main.purchase.on_purchase("m4a4", 3100)):
		failures.append("买 M4A4 应成功")
	main.sim.tick()
	main._update_view()
	main._update_hud()
	if int(main.sim.get_weapon_id(0)) != 2:
		failures.append("买 M4 后内核武器应为 2，实际 %d" % int(main.sim.get_weapon_id(0)))
	if main.viewmodel.current_id != "m4":
		failures.append("买 M4 后手模应为 m4，实际 %s" % main.viewmodel.current_id)
	if not main.hud.weapon_label.text.contains("M4A4"):
		failures.append("买 M4 后 HUD 应显示 M4A4，实际 %s" % main.hud.weapon_label.text)
	if not bool(main.purchase.on_purchase("awp", 4750)):
		failures.append("买 AWP 应成功")
	main.sim.tick()
	main._update_view()
	main._update_hud()
	if int(main.sim.get_weapon_id(0)) != 3:
		failures.append("买 AWP 后内核武器应为 3，实际 %d" % int(main.sim.get_weapon_id(0)))
	if main.viewmodel.current_id != "awp":
		failures.append("买 AWP 后手模应为 awp，实际 %s" % main.viewmodel.current_id)
	if absf(float(main.weapon_magnification()) - 4.0) > 0.01:
		failures.append("AWP 内核倍率应为 4.0，实际 %.2f" % float(main.weapon_magnification()))


## 回归：页面/瞄准不锁死输入；菜单关闭后鼠标模式自愈为抓取（针对"瞄准后转不动视角"报告）。
func _check_mouse_lock() -> void:
	main.call("_start_match")
	for i in 3:
		await physics_frame
	if main._ui_page_open():
		failures.append("对局中不应有页面打开")
	if int(main.want_mouse_mode()) != Input.MOUSE_MODE_CAPTURED:
		failures.append("对局中鼠标应为抓取模式")
	if not bool(main.playing):
		failures.append("对局应处于 playing")
	main.purchase.toggle()
	await physics_frame
	if not main._ui_page_open():
		failures.append("打开购买菜单后 ui_page_open 应为真")
	if int(main.want_mouse_mode()) != Input.MOUSE_MODE_VISIBLE:
		failures.append("菜单打开时鼠标应可见")
	var fires: int = int(main.sim.get_fire_count(0))
	_press_mouse(MOUSE_BUTTON_LEFT)
	for i in 3:
		await physics_frame
	_release_mouse(MOUSE_BUTTON_LEFT)
	if int(main.sim.get_fire_count(0)) != fires:
		failures.append("菜单打开时点击不应开火")
	main.buy_menu.close()
	await physics_frame
	await physics_frame
	if main._ui_page_open():
		failures.append("关闭菜单后 ui_page_open 应为假")
	if int(main.want_mouse_mode()) != Input.MOUSE_MODE_CAPTURED:
		failures.append("关闭菜单后鼠标应自愈为抓取")
	if not bool(main.playing):
		failures.append("关闭菜单后对局应继续")
## 回归（用户报告"点枪没有扣费也没有买下来"）：购买窗口 = 内核 CanBuy（冻结期 + Live 开局 5 秒）。
## 冻结期点击 → 扣钱/换枪/成功提示；Live 宽限内仍可买；宽限过后灰显+提示且点击无副作用。
func _check_buy_window() -> void:
	main.call("_start_match")
	for i in 3:
		await physics_frame
	main._toggle_buy()
	await physics_frame
	if not main.buy_menu.can_buy:
		failures.append("冻结期购买窗口应为打开")
	main.buy_menu.select_category("步枪")
	await physics_frame
	var before: int = int(main.sim.get_money(0))
	var btn: Button = main.buy_menu.button_of("m4a4")
	if btn == null or bool(btn.disabled):
		failures.append("冻结期 M4A4 按钮应可点")
	else:
		btn.pressed.emit()
		for i in 4:
			await physics_frame
	if int(main.sim.get_money(0)) >= before:
		failures.append("冻结期点击购买应扣钱（%d -> %d）" % [before, int(main.sim.get_money(0))])
	if int(main.sim.get_weapon_id(0)) != 2:
		failures.append("冻结期买 M4 后 weapon_id 应为 2，实际 %d" % int(main.sim.get_weapon_id(0)))
	if not main.buy_menu.message_text().contains("已购买"):
		failures.append("购买成功应有可见提示，实际 %s" % main.buy_menu.message_text())
	# Live 开局宽限内（3.75s < 5s）仍可购买
	for i in 240:
		main.sim.tick()
	main._update_view()
	main._update_hud()
	await physics_frame
	if int(main.sim.get_round_phase()) != 1:
		failures.append("应已进入 Live 阶段")
	if not main.buy_menu.can_buy:
		failures.append("Live 开局宽限内仍应可购买")
	main.buy_menu.select_category("狙击")
	await physics_frame
	var btn_awp: Button = main.buy_menu.button_of("awp")
	if btn_awp == null or bool(btn_awp.disabled):
		failures.append("Live 宽限内 AWP 按钮应可点")
	else:
		btn_awp.pressed.emit()
		for i in 4:
			await physics_frame
	if int(main.sim.get_weapon_id(0)) != 3:
		failures.append("Live 宽限内买 AWP 后 weapon_id 应为 3，实际 %d"
				% int(main.sim.get_weapon_id(0)))
	# 宽限过后（Live 已过 1/5 秒阈值）：窗口关闭 → 灰显 + 提示 + 点击无副作用
	for i in 400:
		main.sim.tick()
	main._update_view()
	main._update_hud()
	await physics_frame
	if main.buy_menu.can_buy:
		failures.append("Live 宽限过后购买窗口应关闭")
	var money_late: int = int(main.sim.get_money(0))
	var weapon_late: int = int(main.sim.get_weapon_id(0))
	main.buy_menu.select_category("手枪")
	await physics_frame
	var btn_usp: Button = main.buy_menu.button_of("usp_s")
	if btn_usp != null:
		btn_usp.pressed.emit()
	for i in 4:
		await physics_frame
	if int(main.sim.get_money(0)) != money_late or int(main.sim.get_weapon_id(0)) != weapon_late:
		failures.append("购买窗口关闭后点击不应扣钱/换枪")
	if not main.buy_menu.hint_label.text.contains("冻结期"):
		failures.append("窗口关闭后应提示购买时间已过，实际 %s" % main.buy_menu.hint_label.text)
	main._toggle_buy()
	await physics_frame
