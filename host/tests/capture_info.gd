extends SceneTree
## DEP-02 信息层视觉采集：四层全开 / 全关，各 1 张（1280×720）。
## 用法：godot --path host --script res://tests/capture_info.gd -- <out_dir>

const DEFAULT_DIR := "res://captures/info"
const EXPECTED := 2

var main: Node
var info: InfoOverlay
var out_dir: String
var shots := 0


func _initialize() -> void:
	_start_capture(OS.get_cmdline_user_args())


func _start_capture(args: PackedStringArray) -> void:
	out_dir = args[0] if args.size() > 0 else ProjectSettings.globalize_path(DEFAULT_DIR)
	main = load("res://main.tscn").instantiate()
	root.add_child(main)
	DirAccess.make_dir_recursive_absolute(out_dir)
	_flow.call_deferred()


func _flow() -> void:
	main.call("_start_match")
	await physics_frame
	info = main.get_node("InfoOverlay")
	main.sim.fire(4, 90.0, 0.0)
	main.sim.tick()
	await process_frame
	await process_frame
	info.push_damage(55.0)
	info.push_sound(640.0)
	await process_frame
	await _shot("info_layers_on")
	for key in ["hit_direction", "killfeed", "radar", "sound_visual"]:
		main.settings.set_value("gameplay.info." + key, false)
	await physics_frame
	await _shot("info_layers_off")
	print("INFO_CAPTURE_TOTAL shots=%d expected=%d" % [shots, EXPECTED])
	quit(0 if shots == EXPECTED else 1)


func _shot(name: String) -> void:
	await RenderingServer.frame_post_draw
	var path := out_dir.path_join("%s.png" % name)
	var image := root.get_viewport().get_texture().get_image()
	image.save_png(path)
	shots += 1
	print("INFO_CAPTURE page=%s path=%s size=%dx%d" % [name, path, image.get_width(),
			image.get_height()])

