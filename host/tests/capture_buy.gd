extends SceneTree
## 购买 UI 视觉采集（SPEC-06 BUY-06/07）：6 分类列表 + 轮盘 + 退款禁用态，共 8 张。
## 用法：godot --path host --script res://tests/capture_buy.gd -- <out_dir>
## 产出：buy_list_<idx>.png ×6、buy_wheel.png、buy_refunded.png + BUY_CAPTURE 清单行。

const DEFAULT_DIR := "res://captures/buy"
const CATEGORIES := ["手枪", "冲锋枪", "步枪", "重型", "装备", "投掷物"]
const EXPECTED := 8

var main: Node
var out_dir: String
var shots: int = 0


func _initialize() -> void:
	_boot(OS.get_cmdline_user_args())


func _boot(args: PackedStringArray) -> void:
	out_dir = args[0] if args.size() > 0 else ProjectSettings.globalize_path(DEFAULT_DIR)
	main = load("res://main.tscn").instantiate()
	main.name = "BuyCaptureRoot"
	root.add_child(main)
	DirAccess.make_dir_recursive_absolute(out_dir)
	_flow.call_deferred()


func _shot_path(name: String) -> String:
	return out_dir.path_join("%s.png" % name)


func _flow() -> void:
	main.call("_start_match")
	main.set_physics_process(false)
	await process_frame
	main.call("_toggle_buy")
	await process_frame
	for i in CATEGORIES.size():
		main.buy_menu.select_category(CATEGORIES[i])
		for f in 3:
			await process_frame
		await _shot("buy_list_%d" % (i + 1))
	main.buy_menu.toggle_mode()
	for f in 3:
		await process_frame
	await _shot("buy_wheel")
	main.buy_menu.toggle_mode()
	main.buy_menu.select_category("重型")
	await process_frame
	main.purchase.on_purchase("awp", 4750)
	main.sim.tick()
	main.purchase.on_refund("awp")
	main.sim.tick()
	main.buy_menu.show_message("已退款 AWP（本回合禁止再买）")
	main.purchase.refresh()
	await process_frame
	await _shot("buy_refunded")
	print("BUY_CAPTURE_TOTAL shots=%d expected=%d" % [shots, EXPECTED])
	quit(0 if shots == EXPECTED else 1)


func _shot(name: String) -> void:
	await RenderingServer.frame_post_draw
	var path := out_dir.path_join("%s.png" % name)
	var image := root.get_viewport().get_texture().get_image()
	image.save_png(path)
	shots += 1
	print("BUY_CAPTURE page=%s path=%s size=%dx%d" % [name, path,
			image.get_width(), image.get_height()])
