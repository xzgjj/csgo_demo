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
	await _check_loadout()
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
