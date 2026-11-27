extends "res://tests/scene_test.gd"
## DEP-02 信息四层：声音/雷达/击杀信息/受击方向各自开关，互不牵连。
## 用法：godot --headless --path host --script res://tests/info_layers_smoke.gd

var _info: InfoOverlay


func _run() -> void:
	main.call("_start_match")
	await physics_frame
	_info = main.get_node("InfoOverlay")
	var layers: Dictionary = main.hud.info_layer_enabled(main.settings)
	expect(layers.keys().size() == 4, "信息层应为 4 个，实际 %d" % layers.keys().size())
	expect(layers.values().all(func(v: bool) -> bool: return v), "四层默认应全开")
	_info.push_damage(45.0)
	expect(_info.hit_arc_active(), "受击方向弧应可显示")
	main.settings.set_value("gameplay.info.hit_direction", false)
	await physics_frame
	expect(not _info.hit_arc_active(), "关闭受击方向后弧线应消失")
	main.settings.set_value("gameplay.info.killfeed", false)
	await physics_frame
	expect(not bool(main.hud.feed_label.visible), "关闭击杀信息后 feed 应隐藏")
	main.settings.set_value("gameplay.info.radar", false)
	await physics_frame
	expect(not bool(main.radar.visible), "关闭雷达后雷达应隐藏")
	main.settings.set_value("gameplay.info.sound_visual", false)
	await physics_frame
	expect(_info.ping_count() == 0, "关闭声音可视化后不应保留声源提示")
	main.settings.set_value("gameplay.info.hit_direction", true)
	main.settings.set_value("gameplay.info.killfeed", true)
	main.settings.set_value("gameplay.info.radar", true)
	main.settings.set_value("gameplay.info.sound_visual", true)
	await physics_frame
	await process_frame
	expect(bool(main.radar.enabled), "重新开启雷达应恢复 enabled")
	finish("INFO_LAYERS_SMOKE")
