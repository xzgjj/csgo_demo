class_name HudOverlays
extends RefCounted
## HUD 附加层：阵亡提示 / F3 调试信息（表现层，只读 sim 状态）。


static func make_label(parent: Node, text: String, size: int, color: Color) -> Label:
	var label := Label.new()
	label.text = text
	label.add_theme_font_size_override("font_size", size)
	label.add_theme_color_override("font_color", color)
	label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	label.set_anchors_preset(Control.PRESET_CENTER)
	label.position = Vector2(-360, -40)
	label.custom_minimum_size = Vector2(720, 40)
	label.visible = false
	parent.add_child(label)
	return label


static func make_death_label(parent: Node) -> Label:
	return make_label(parent, "已阵亡 · 等待本回合结束自动复活", 30, Color(1.0, 0.55, 0.5))


static func make_debug_label(parent: Node) -> Label:
	var label := make_label(parent, "", 15, Color(0.75, 0.95, 0.75))
	label.horizontal_alignment = HORIZONTAL_ALIGNMENT_LEFT
	label.set_anchors_preset(Control.PRESET_TOP_RIGHT)
	label.position = Vector2(-420, 12)
	label.custom_minimum_size = Vector2(400, 20)
	return label


static func debug_text(sim: RefCounted, aiming: bool) -> String:
	return "FPS %d · tick %d · pos(%.0f,%.0f,%.0f) · 回合%d %s · 比分%d:%d · T存活%d" % [
			Engine.get_frames_per_second(), sim.get_round_ticks(), sim.get_x(0), sim.get_y(0),
			sim.get_z(0), sim.get_round_number(), "aim" if aiming else "", sim.get_score_t(),
			sim.get_score_ct(), sim.get_alive_count(0)]
