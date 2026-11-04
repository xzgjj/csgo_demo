extends SceneTree
## 模块 10 headless 契约测试：AccountStore（schema 持久化/损坏回退/原子写）+ LoadoutPage（选择即持久化）
## + BuyMenu 前端骨架（分类/价格/余额灰显/purchase_requested 信号）。
## 用法：& godot --headless --path host --script res://tests/account_smoke.gd
## 存档隔离：全部用 user://account_smoke_test.json，不读写真实 user://account.json；用例前后清理。

const AccountStoreScript = preload("res://scripts/account_store.gd")
const LoadoutPageScript = preload("res://scripts/loadout_page.gd")
const BuyMenuScript = preload("res://scripts/buy_menu.gd")
const Support = preload("res://tests/smoke_support.gd")

const TEST_PATH := "user://account_smoke_test.json"
const TEST_TMP := "user://account_smoke_test.json.tmp"

var checks := 0
var failures: Array[String] = []
var _purchase_log: Array = []
var _capture_dir := ""


func _initialize() -> void:
	_capture_dir = Support.capture_dir_from_args()
	_run.call_deferred()


func _run() -> void:
	await process_frame
	_test_default_path_unchanged()
	_test_missing_file_defaults()
	_test_corrupted_json_fallback()
	_test_bad_types_sanitized()
	_test_persist_roundtrip()
	_test_atomic_write()
	await _test_loadout_page()
	await _test_buy_menu()
	if not _capture_dir.is_empty():
		await _capture_pages()
	_remove(TEST_PATH)
	_remove(TEST_TMP)
	print("ACCOUNT_SMOKE checks=%d failures=%d" % [checks, failures.size()])
	if failures.is_empty():
		print("ACCOUNT_SMOKE PASS")
		quit(0)
		return
	for item in failures:
		print("ACCOUNT_FAIL ", item)
	quit(1)


## 准备：新实例默认路径；执行：读 path 常量（不写盘）；断言：默认存档路径未被测试污染。
func _test_default_path_unchanged() -> void:
	var store: RefCounted = AccountStoreScript.new()
	_expect(str(store.path) == "user://account.json",
			"默认存档路径应为 user://account.json，实际 %s" % str(store.path))
	_expect(int(store.stats["kills"]) == 0 and int(store.stats["deaths"]) == 0
			and int(store.stats["matches"]) == 0 and int(store.stats["wins"]) == 0,
			"新账号统计应全为 0")


## 准备：删除测试存档；执行：load_account；断言：返回 false 且回退默认值（不崩溃）。
func _test_missing_file_defaults() -> void:
	_remove(TEST_PATH)
	_remove(TEST_TMP)
	var store: RefCounted = AccountStoreScript.new(TEST_PATH)
	var loaded: bool = store.load_account()
	_expect(not loaded, "存档缺失时 load_account 应返回 false")
	_expect(str(store.nickname) == "Recruit", "缺失存档时昵称应回退 Recruit，实际 %s"
			% str(store.nickname))
	_expect(str(store.loadout["primary"]) == "ak", "主武器默认应为 ak")
	_expect(str(store.loadout["secondary"]) == "usp", "副武器默认应为 usp")
	_expect(int(store.stats["kills"]) == 0 and int(store.stats["deaths"]) == 0
			and int(store.stats["matches"]) == 0 and int(store.stats["wins"]) == 0,
			"缺失存档时统计应全为 0")


## 准备：写入坏 JSON / 顶层非对象；执行：load_account；断言：回退默认且不崩溃。
func _test_corrupted_json_fallback() -> void:
	_write_raw(TEST_PATH, "{ this is not json")
	var store: RefCounted = AccountStoreScript.new(TEST_PATH)
	_expect(not store.load_account(), "坏 JSON 时 load_account 应返回 false")
	_expect(str(store.nickname) == "Recruit" and str(store.loadout["primary"]) == "ak",
			"坏 JSON 时应回退默认值")
	_write_raw(TEST_PATH, "[1, 2, 3]")
	var store2: RefCounted = AccountStoreScript.new(TEST_PATH)
	_expect(not store2.load_account(), "顶层非 Dictionary 时 load_account 应返回 false")
	_expect(int(store2.stats["matches"]) == 0, "顶层非 Dictionary 时统计应回退 0")


## 准备：合法 JSON 但字段类型/取值非法；执行：load_account；断言：清洗为合法值。
func _test_bad_types_sanitized() -> void:
	_write_raw(TEST_PATH, JSON.stringify({
		"schema": "account/1",
		"nickname": 12345,
		"loadout": {"primary": "not_a_gun", "secondary": "deagle"},
		"stats": {"kills": "many", "deaths": -5, "matches": 2.5, "wins": true},
	}))
	var store: RefCounted = AccountStoreScript.new(TEST_PATH)
	_expect(store.load_account(), "合法 JSON（字段异常）应能读入")
	_expect(str(store.nickname) == "Recruit", "非字符串昵称应回退默认，实际 %s" % str(store.nickname))
	_expect(str(store.loadout["primary"]) == "ak", "非法主武器应回退 ak，实际 %s"
			% str(store.loadout["primary"]))
	_expect(str(store.loadout["secondary"]) == "usp", "非法副武器应回退 usp，实际 %s"
			% str(store.loadout["secondary"]))
	_expect(int(store.stats["kills"]) == 0, "非数字击杀应回退 0，实际 %s" % str(store.stats["kills"]))
	_expect(int(store.stats["deaths"]) == 0, "负数阵亡应钳制为 0，实际 %s"
			% str(store.stats["deaths"]))
	_expect(int(store.stats["matches"]) == 2, "浮点比赛数应取整为 2，实际 %s"
			% str(store.stats["matches"]))


## 准备：删除存档、新建 store；执行：改昵称/武器/统计并 reload；断言：字段一致 + 文件可解析 + schema。
func _test_persist_roundtrip() -> void:
	_remove(TEST_PATH)
	_remove(TEST_TMP)
	var store: RefCounted = AccountStoreScript.new(TEST_PATH)
	_expect(store.set_nickname("TestPilot"), "set_nickname 应返回保存成功")
	_expect(store.set_primary("awp"), "set_primary 应返回保存成功")
	_expect(store.set_secondary("glock"), "set_secondary 应返回保存成功")
	_expect(store.record_match(3, 1, true), "record_match 应返回保存成功")
	_expect(store.display_name == "TestPilot", "display_name 应作为 nickname 的兼容别名")
	_expect(int(store.stats["kills"]) == 3 and int(store.stats["deaths"]) == 1
			and int(store.stats["matches"]) == 1 and int(store.stats["wins"]) == 1,
			"内存统计应为 3/1/1/1，实际 %s" % str(store.stats))
	var reloaded: RefCounted = AccountStoreScript.new(TEST_PATH)
	_expect(reloaded.load_account(), "刚写入的存档应能读回")
	_expect(str(reloaded.nickname) == "TestPilot", "昵称未持久化，实际 %s" % str(reloaded.nickname))
	_expect(str(reloaded.loadout["primary"]) == "awp", "主武器未持久化")
	_expect(str(reloaded.loadout["secondary"]) == "glock", "副武器未持久化")
	_expect(int(reloaded.stats["kills"]) == 3 and int(reloaded.stats["deaths"]) == 1
			and int(reloaded.stats["matches"]) == 1 and int(reloaded.stats["wins"]) == 1,
			"统计未持久化，实际 %s" % str(reloaded.stats))
	var raw := _read_raw(TEST_PATH)
	_expect(not raw.is_empty(), "存档文件应存在且非空")
	var parsed: Variant = JSON.parse_string(raw)
	_expect(parsed is Dictionary, "存档应为合法 JSON 对象")
	if parsed is Dictionary:
		_expect(str((parsed as Dictionary).get("schema", "")) == "account/1",
				"存档应带 schema=account/1")


## 准备：新建 store 保存一次；执行：检查 tmp 残留 + 不可写路径；断言：原子写不残留 tmp / 失败返回 false。
func _test_atomic_write() -> void:
	_remove(TEST_PATH)
	_remove(TEST_TMP)
	var store: RefCounted = AccountStoreScript.new(TEST_PATH)
	store.set_primary("m4")
	_expect(FileAccess.file_exists(TEST_PATH), "保存后应存在目标存档文件")
	_expect(not FileAccess.file_exists(TEST_TMP), "原子写完成后不应残留 .tmp 文件")
	_expect(_read_raw(TEST_PATH).begins_with("{"), "存档内容应为 JSON 对象")
	var broken: RefCounted = AccountStoreScript.new("user://account_smoke_missing_dir/account.json")
	_expect(not broken.save_account(), "目录不存在时 save_account 应返回 false（不崩溃）")


## 准备：store + LoadoutPage；执行：open + 点击主/副武器按钮；断言：UI 文本正确、选择即时持久化。
func _test_loadout_page() -> void:
	_remove(TEST_PATH)
	var store: RefCounted = AccountStoreScript.new(TEST_PATH)
	store.set_nickname("PageTest")
	store.set_primary("m4")
	var page: CanvasLayer = LoadoutPageScript.new()
	root.add_child(page)
	page.build(store)
	page.open()
	await process_frame
	_expect(bool(page.visible), "open() 后物品栏页面应可见")
	_expect(page.nickname_label.text.contains("PageTest"), "物品栏应显示昵称，实际 %s"
			% page.nickname_label.text)
	_expect(page.primary_buttons.size() == 3, "主武器应有 AK/M4/AWP 3 个按钮，实际 %d"
			% page.primary_buttons.size())
	_expect(page.secondary_buttons.size() == 2, "副武器应有 USP/Glock 2 个按钮，实际 %d"
			% page.secondary_buttons.size())
	_expect(page.stats_label.text.contains("比赛 0") and page.stats_label.text.contains("击杀 0"),
			"物品栏应显示统计，实际 %s" % page.stats_label.text)
	# 执行：点击 AWP（第 3 个主武器按钮）→ 选择应写回 store 并落盘。
	page.primary_buttons[2].pressed.emit()
	await process_frame
	_expect(str(store.loadout["primary"]) == "awp", "点击 AWP 应更新主武器，实际 %s"
			% str(store.loadout["primary"]))
	var reloaded: RefCounted = AccountStoreScript.new(TEST_PATH)
	_expect(reloaded.load_account(), "点击后应已落盘")
	_expect(str(reloaded.loadout["primary"]) == "awp", "点击后主武器应持久化为 awp")
	# 执行：点击副武器 Glock（第 2 个）→ 持久化。
	page.secondary_buttons[1].pressed.emit()
	await process_frame
	_expect(str(store.loadout["secondary"]) == "glock", "点击 Glock 应更新副武器")
	var reloaded2: RefCounted = AccountStoreScript.new(TEST_PATH)
	_expect(reloaded2.load_account() and str(reloaded2.loadout["secondary"]) == "glock",
			"副武器选择应持久化")
	page.close()
	await process_frame
	_expect(not bool(page.visible), "close() 后物品栏页面应隐藏")
	page.queue_free()
	await process_frame


## 准备：BuyMenu + 余额 3000；执行：切分类/买 AK/降余额；断言：分类/价格/灰显/信号。
func _test_buy_menu() -> void:
	var menu: CanvasLayer = BuyMenuScript.new()
	root.add_child(menu)
	menu.build()
	menu.purchase_requested.connect(_on_purchase_requested)
	menu.open(3000)
	await process_frame
	_expect(bool(menu.visible) and bool(menu.is_open()), "open() 后购买菜单应可见")
	var names: Array = menu.category_names()
	_expect(names.size() == 5, "购买菜单应有 5 个分类，实际 %d" % names.size())
	for expected in ["手枪", "步枪", "狙击", "装备", "手雷"]:
		_expect(expected in names, "购买菜单缺分类 %s" % expected)
	menu.select_category("步枪")
	await process_frame
	_expect(menu.item_ids() == ["ak47", "m4a4"], "步枪分类应有 AK-47/M4A4，实际 %s"
			% str(menu.item_ids()))
	_expect(menu.price_of("ak47") == 2700, "AK-47 价格应为 2700，实际 %d" % menu.price_of("ak47"))
	_expect(menu.price_of("awp") == 4750, "AWP 价格应为 4750，实际 %d" % menu.price_of("awp"))
	_expect(menu.buyable("ak47"), "余额 3000 时 AK-47 应可买")
	_expect(not menu.buyable("awp"), "余额 3000 时 AWP（4750）应不可买")
	_expect(menu.loadout_id_of("ak47") == "ak", "购买 AK-47 应能映射到装备 id ak，实际 %s"
			% menu.loadout_id_of("ak47"))
	# 执行：切到狙击分类看买不起的项灰显；再回步枪买 AK → 信号带 id/price。
	menu.select_category("狙击")
	await process_frame
	var awp_button: Button = menu.button_of("awp")
	_expect(awp_button != null and awp_button.disabled, "买不起的 AWP 按钮应 disabled 灰显")
	menu.select_category("步枪")
	await process_frame
	_purchase_log.clear()
	_expect(menu.buy_item("ak47"), "买得起时 buy_item 应返回 true 并 emit")
	_expect(_purchase_log.size() == 1, "应收到 1 次 purchase_requested，实际 %d"
			% _purchase_log.size())
	if _purchase_log.size() == 1:
		var entry: Array = _purchase_log[0]
		_expect(str(entry[0]) == "ak47" and int(entry[1]) == 2700,
				"信号参数应为 (ak47, 2700)，实际 %s" % str(entry))
	# 执行：余额降到 100 → 买不起，不应 emit。
	menu.set_balance(100)
	await process_frame
	_purchase_log.clear()
	_expect(not menu.buy_item("ak47"), "余额不足时 buy_item 应返回 false")
	_expect(_purchase_log.is_empty(), "余额不足时不应发 purchase_requested")
	menu.close()
	await process_frame
	_expect(not bool(menu.visible), "close() 后购买菜单应隐藏")
	menu.queue_free()
	await process_frame


## 截图机位（Lead 接 golden）：loadout / buy_menu 两帧；窗口化跑 `-- --capture <dir>`。
func _capture_pages() -> void:
	var store: RefCounted = AccountStoreScript.new(TEST_PATH)
	store.set_nickname("Recruit")
	store.set_primary("awp")
	store.set_secondary("glock")
	store.record_match(7, 4, true)
	var page: CanvasLayer = LoadoutPageScript.new()
	root.add_child(page)
	page.build(store)
	page.open()
	await _capture("loadout")
	page.queue_free()
	await process_frame
	var menu: CanvasLayer = BuyMenuScript.new()
	root.add_child(menu)
	menu.build()
	menu.open(3000)
	menu.select_category("步枪")
	await _capture("buy_menu")
	menu.queue_free()
	await process_frame


func _capture(shot_name: String) -> void:
	await process_frame
	await RenderingServer.frame_post_draw
	Support.save_shot(root, _capture_dir, shot_name, "ACCOUNT_CAPTURE")


func _on_purchase_requested(item_id: String, price: int) -> void:
	_purchase_log.append([item_id, price])


func _global_path(path: String) -> String:
	return ProjectSettings.globalize_path(path)


func _remove(path: String) -> void:
	if not FileAccess.file_exists(path):
		return
	DirAccess.remove_absolute(_global_path(path))


func _write_raw(path: String, text: String) -> void:
	var file := FileAccess.open(path, FileAccess.WRITE)
	if file == null:
		failures.append("无法写入测试文件 %s" % path)
		return
	file.store_string(text)
	file.close()


func _read_raw(path: String) -> String:
	return FileAccess.get_file_as_string(path)


func _expect(condition: bool, message: String) -> void:
	checks += 1
	if not condition:
		failures.append(message)
