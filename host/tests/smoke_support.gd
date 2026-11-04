extends RefCounted
## smoke 测试共享 helper（不用 class_name，测试侧 preload 引用，不依赖 .godot 类缓存）。
## 目的：把 account_smoke / buy_smoke 里重复的「--capture 参数解析」和「截图落盘」抽到一处，
## 满足 CI duplicates 门（归一化后 ≥8 行相同块）。


## 解析命令行 `-- --capture <dir>`；没有该参数时返回空串。
static func capture_dir_from_args() -> String:
	var args := OS.get_cmdline_user_args()
	if args.size() >= 2 and args[0] == "--capture":
		return args[1]
	return ""


## 把当前 viewport 存成 PNG（调用方先 await 帧；static 里不 await，保持可当普通函数调用）。
static func save_shot(viewport: Viewport, out_dir: String, shot_name: String, tag: String) -> void:
	DirAccess.make_dir_recursive_absolute(out_dir)
	var image: Image = viewport.get_texture().get_image()
	var path := out_dir.path_join(shot_name + ".png")
	image.save_png(path)
	print(tag, " ", path, " ", image.get_width(), "x", image.get_height())
