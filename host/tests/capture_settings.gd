extends SceneTree
## 设置页视觉采集（SPEC-05 SET-07）：非编辑器窗口化运行，逐页截图并打印清单。
## 用法：godot --path host --script res://tests/capture_settings.gd -- <out_dir>
## 产出：settings_<page>.png 共 8 张 + SETTINGS_CAPTURE 行（供 watch_capture 校验新鲜度）。

const DEFAULT_DIR := "res://captures/settings"
const EXPECTED_PAGES := 8

var main: Node
var out_dir: String
var pages: Array[String] = []
var shots: int = 0


func _initialize() -> void:
	_boot(OS.get_cmdline_user_args())


func _boot(args: PackedStringArray) -> void:
	out_dir = args[0] if args.size() > 0 else ProjectSettings.globalize_path(DEFAULT_DIR)
	main = load("res://main.tscn").instantiate()
	root.add_child(main)
	DirAccess.make_dir_recursive_absolute(out_dir)
	_flow.call_deferred()


func _flow() -> void:
	main.account.stats = {"kills": 0, "deaths": 0, "matches": 9, "wins": 9}
	main.account.nickname = "Recruit"
	main.account.loadout = {"primary": "awp", "secondary": "glock", "knife": "knife"}
	main.set_physics_process(false)
	await process_frame
	main.show_settings_page()
	await process_frame
	for page_id: String in main.settings_page.store.registry.page_ids():
		main.settings_page.select_page(page_id)
		for i in 3:
			await process_frame
		await RenderingServer.frame_post_draw
		var path := out_dir.path_join("settings_%s.png" % page_id)
		var image := root.get_viewport().get_texture().get_image()
		image.save_png(path)
		shots += 1
		print("SETTINGS_CAPTURE page=%s path=%s size=%dx%d" % [page_id, path, image.get_width(), image.get_height()])
	print("SETTINGS_CAPTURE_TOTAL shots=%d pages=%d" % [shots, main.settings_page.store.registry.page_ids().size()])
	quit(0 if shots == EXPECTED_PAGES else 1)
