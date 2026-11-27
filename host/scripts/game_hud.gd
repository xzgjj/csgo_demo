class_name GameHud
extends CanvasLayer
## 对局 HUD：回合阶段/倒计时、金钱、护甲（+头盔）、武器弹药、C4 状态、击杀滚动信息。
## 数据只读 sim；击杀信息按「击杀数增量 + 当帧死亡」累积，不依赖内核未暴露的事件明细。

const TICKS_PER_SECOND := 64.0
const FEED_MAX_LINES := 5
const PHASE_NAMES: Array[String] = ["冻结期", "进行中", "回合结束", "比赛结束"]
const TEXT_COLOR := Color(0.95, 0.97, 1.0)
const COLOR_DEFAULT := Color(0.55, 1.0, 0.65)

var phase_label: Label
var timer_label: Label
var money_label: Label
var health_label: Label
var armor_label: Label
var weapon_label: Label
var c4_label: Label
var feed_label: Label
var stats_label: Label
var crosshair: Control
var hitmarker: Control
var kill_total := 0
var death_total := 0
var _feed: Array[String] = []
var _kill_seen := 0
var _hit_seen := 0


func build() -> void:
	layer = 0
	phase_label = _make_label(Vector2(20, 14), 22)
	timer_label = _make_label(Vector2(20, 42), 30)
	money_label = _make_label(Vector2(20, 84), 20)
	health_label = _make_label(Vector2(20, 628), 24)
	armor_label = _make_label(Vector2(20, 658), 18)
	stats_label = _make_label(Vector2(20, 604), 16)
	weapon_label = _make_label(Vector2(1020, 620), 20)
	weapon_label.custom_minimum_size = Vector2(240, 24)
	weapon_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	c4_label = _make_label(Vector2(1020, 592), 18)
	c4_label.custom_minimum_size = Vector2(240, 24)
	c4_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	feed_label = _make_label(Vector2(880, 84), 16)
	feed_label.custom_minimum_size = Vector2(380, 120)
	feed_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	feed_label.add_theme_color_override("font_color", Color(0.95, 0.85, 0.70))
	# 准星/命中标记用自绘 Control（SPEC-02），位置由锚点铺满视口。
	crosshair = Crosshair.new()
	crosshair.name = "Crosshair"
	add_child(crosshair)
	hitmarker = Hitmarker.new()
	hitmarker.name = "Hitmarker"
	add_child(hitmarker)


## 命中标记：headshot / through_wall 来自内核 DamageApplied 的 flags。
func show_hit(headshot: bool, through_wall: bool) -> void:
	if hitmarker != null:
		hitmarker.call("flash", headshot, through_wall)


## 准星张合：degrees 为内核实际扩散（含 base 与姿态）。
func set_crosshair_spread(degrees: float) -> void:
	if crosshair != null:
		crosshair.call("set_spread", degrees)


## 战斗反馈聚合（SPEC-02/04）：准星扩散 + 命中标记；main 每帧只调这一处。
func update_combat(sim: RefCounted) -> void:
	if sim == null:
		return
	if sim.has_method("get_effective_inaccuracy"):
		set_crosshair_spread(float(sim.get_effective_inaccuracy(0)))
	if not sim.has_method("get_damage_count_by"):
		return
	var damage_now: int = int(sim.get_damage_count_by(0))
	if damage_now > _hit_seen:
		var hit_flags := 0
		if sim.has_method("get_last_hit_flags"):
			hit_flags = int(sim.get_last_hit_flags(0))
		show_hit((hit_flags & 1) != 0, (hit_flags & 2) != 0)
	_hit_seen = damage_now


func refresh(sim: RefCounted, weapon_name: String) -> void:
	if sim == null:
		return
	var phase: int = sim.get_round_phase()
	phase_label.text = "回合阶段：%s" % _phase_name(phase)
	timer_label.text = _timer_text(phase, float(sim.get_round_ticks()) / TICKS_PER_SECOND)
	money_label.text = "$%d" % sim.get_money(0)
	health_label.text = "HP %d" % sim.get_health(0)
	armor_label.text = "护甲 %d · 头盔 %s" % [sim.get_armor(0), _helmet_text(sim)]
	var reloading: bool = sim.has_method("is_reloading") and bool(sim.is_reloading(0))
	weapon_label.text = "%s  %d/%d%s" % [weapon_name, sim.get_mag(0), sim.get_reserve(0),
			" 换弹中…" if reloading else ""]
	c4_label.text = _c4_text(sim)
	stats_label.text = "击杀 %d · 阵亡 %d · 命中事件 %d" % [
		kill_total, death_total, sim.get_total_damage_events()]
	_track_feed(sim)


func reset_feed() -> void:
	_feed.clear()
	_kill_seen = 0
	_hit_seen = 0
	kill_total = 0
	death_total = 0
	if feed_label != null:
		feed_label.text = ""
	if stats_label != null:
		stats_label.text = ""


## 设置热应用（SPEC-05 target=hud）：信息层开关、准星、字号、不透明度与整体缩放。
## 默认值与旧观感一致（准星 gap 5 / thickness 2 / 颜色 #8cffa6），不改变既有 golden。
func apply_settings(store: SettingsStore) -> void:
	if store == null or crosshair == null:
		return
	crosshair.call("configure", {
		"gap": float(store.get_value("gameplay.crosshair.gap")),
		"thickness": float(store.get_value("gameplay.crosshair.thickness")),
		"color": Color.from_string(str(store.get_value("gameplay.crosshair.color")), COLOR_DEFAULT),
		"dot": bool(store.get_value("gameplay.crosshair.dot")),
		"style": str(store.get_value("gameplay.crosshair.style")),
		"dynamic": bool(store.get_value("gameplay.crosshair.dynamic")),
	})
	var show_feed := bool(store.get_value("gameplay.info.killfeed")) \
			and bool(store.get_value("ui.notify.kill"))
	feed_label.visible = show_feed
	var font_size := int(store.get_value("ui.hud.font_size"))
	feed_label.add_theme_font_size_override("font_size", font_size)
	stats_label.add_theme_font_size_override("font_size", font_size)
	var opacity := clampf(float(store.get_value("ui.hud.opacity")) / 100.0, 0.0, 1.0)
	_apply_opacity(opacity)
	var hud_scale := float(store.get_value("ui.hud.scale"))
	scale = Vector2(hud_scale, hud_scale)


## CanvasLayer 没有 modulate，逐个子 CanvasItem 调 alpha（准星/命中标记同样跟随）。
func _apply_opacity(alpha: float) -> void:
	for child: Node in get_children():
		if child is CanvasItem:
			(child as CanvasItem).modulate = Color(1.0, 1.0, 1.0, alpha)


func info_layer_enabled(store: SettingsStore) -> Dictionary:
	return {
		"killfeed": bool(store.get_value("gameplay.info.killfeed")),
		"hit_direction": bool(store.get_value("gameplay.info.hit_direction")),
		"radar": bool(store.get_value("gameplay.info.radar")),
	}


func feed_lines() -> Array[String]:
	return _feed.duplicate()


func _timer_text(phase: int, seconds: float) -> String:
	if phase == 2:
		return "下一回合 %s" % _format_time(seconds)
	if phase == 3:
		return "比赛结束"
	return _format_time(seconds)


func _format_time(seconds: float) -> String:
	var total := int(ceilf(maxf(seconds, 0.0)))
	return "%02d:%02d" % [total / 60, total % 60]


func _phase_name(phase: int) -> String:
	if phase >= 0 and phase < PHASE_NAMES.size():
		return PHASE_NAMES[phase]
	return "未知"


func _helmet_text(sim: RefCounted) -> String:
	return "有" if sim.get_helmet(0) != 0 else "无"


func _c4_text(sim: RefCounted) -> String:
	if sim.get_bomb_planted() != 0:
		return "C4 已安放 · %.1fs" % (float(sim.get_bomb_timer()) / TICKS_PER_SECOND)
	return "C4 未安放"


func _track_feed(sim: RefCounted) -> void:
	var total: int = int(sim.get_kill_count())
	while _kill_seen < total:
		var shooter: int = int(sim.get_kill_shooter(_kill_seen))
		var victim: int = int(sim.get_kill_victim(_kill_seen))
		_push_feed("%s 击杀 %s" % [_player_tag(sim, shooter), _player_tag(sim, victim)])
		if shooter == 0:
			kill_total += 1
		if victim == 0:
			death_total += 1
		_kill_seen += 1
	feed_label.text = "\n".join(PackedStringArray(_feed))


func _player_tag(sim: RefCounted, index: int) -> String:
	return "%s·P%d" % [UiFactory.team_name(sim, index), index]


func _push_feed(line: String) -> void:
	_feed.append(line)
	if _feed.size() > FEED_MAX_LINES:
		_feed.pop_front()


func _make_label(at: Vector2, font_size: int) -> Label:
	var label := Label.new()
	label.position = at
	label.add_theme_font_size_override("font_size", font_size)
	label.add_theme_color_override("font_color", TEXT_COLOR)
	add_child(label)
	return label
