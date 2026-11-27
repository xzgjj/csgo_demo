extends "res://tests/scene_test.gd"
## DEP-02 信息层：雷达/击杀信息/受击方向各自开关，互不牵连（声源读数已移除）。
## 用法：godot --headless --path host --script res://tests/info_layers_smoke.gd

var _info: InfoOverlay


func _run() -> void:
	main.call("_start_match")
	await physics_frame
	_info = main.get_node("InfoOverlay")
	var layers: Dictionary = main.hud.info_layer_enabled(main.settings)
	expect(layers.keys().size() == 3, "信息层应为 3 个，实际 %d" % layers.keys().size())
	expect(not layers.has("sound_visual"), "声源读数层应已移除")
	expect(bool(layers["radar"]) and bool(layers["killfeed"]) and bool(layers["hit_direction"]),
			"雷达/击杀/受击方向默认应开启")
	_info.push_damage(45.0)
	expect(_info.hit_arc_active(), "受击方向弧应可显示")
	main.settings.set_value("gameplay.info.hit_direction", false, false)
	await physics_frame
	expect(not _info.hit_arc_active(), "关闭受击方向后弧线应消失")
	main.settings.set_value("gameplay.info.killfeed", false, false)
	await physics_frame
	expect(not bool(main.hud.feed_label.visible), "关闭击杀信息后 feed 应隐藏")
	main.settings.set_value("gameplay.info.radar", false, false)
	await physics_frame
	expect(not bool(main.radar.visible), "关闭雷达后雷达应隐藏")
	main.settings.set_value("gameplay.info.hit_direction", true, false)
	main.settings.set_value("gameplay.info.killfeed", true, false)
	main.settings.set_value("gameplay.info.radar", true, false)
	await physics_frame
	await process_frame
	expect(bool(main.radar.enabled), "重新开启雷达应恢复 enabled")
	finish("INFO_LAYERS_SMOKE")
