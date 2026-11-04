class_name GameHud
extends CanvasLayer
## 对局 HUD：回合阶段/倒计时、金钱、护甲（+头盔）、武器弹药、C4 状态、击杀滚动信息。
## 数据只读 sim；击杀信息按「击杀数增量 + 当帧死亡」累积，不依赖内核未暴露的事件明细。

const TICKS_PER_SECOND := 64.0
const FEED_MAX_LINES := 5
const PHASE_NAMES: Array[String] = ["冻结期", "进行中", "回合结束", "比赛结束"]
const TEXT_COLOR := Color(0.95, 0.97, 1.0)

var phase_label: Label
var timer_label: Label
var money_label: Label
var health_label: Label
var armor_label: Label
var weapon_label: Label
var c4_label: Label
var feed_label: Label
var stats_label: Label
var crosshair: Label
var kill_total := 0
var death_total := 0
var _feed: Array[String] = []
var _prev_alive: Array = []
var _prev_kills: Array = []


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
	crosshair = _make_label(Vector2(632, 344), 26)
	crosshair.text = "+"


func refresh(sim: RefCounted, weapon_name: String) -> void:
	if sim == null:
		return
	var phase: int = sim.get_round_phase()
	phase_label.text = "回合阶段：%s" % _phase_name(phase)
	timer_label.text = _timer_text(phase, float(sim.get_round_ticks()) / TICKS_PER_SECOND)
	money_label.text = "$%d" % sim.get_money(0)
	health_label.text = "HP %d" % sim.get_health(0)
	armor_label.text = "护甲 %d · 头盔 %s" % [sim.get_armor(0), _helmet_text(sim)]
	weapon_label.text = "%s  %d/%d" % [weapon_name, sim.get_mag(0), sim.get_reserve(0)]
	c4_label.text = _c4_text(sim)
	stats_label.text = "击杀 %d · 阵亡 %d · 命中事件 %d" % [
		kill_total, death_total, sim.get_total_damage_events()]
	_track_feed(sim)


func reset_feed() -> void:
	_feed.clear()
	_prev_alive.clear()
	_prev_kills.clear()
	kill_total = 0
	death_total = 0
	if feed_label != null:
		feed_label.text = ""
	if stats_label != null:
		stats_label.text = ""


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
	var count := UiFactory.player_count(sim)
	var alive: Array = []
	var kills: Array = []
	for index in count:
		alive.append(sim.is_alive(index))
		kills.append(sim.get_kills(index))
	while _prev_alive.size() < count:
		_prev_alive.append(true)
		_prev_kills.append(0)
	var deaths: Array = []
	for index in count:
		if bool(_prev_alive[index]) and not bool(alive[index]):
			deaths.append(index)
	death_total += deaths.size()
	for index in count:
		var gained: int = int(kills[index]) - int(_prev_kills[index])
		kill_total += maxi(gained, 0)
		for _hit in maxi(gained, 0):
			if deaths.is_empty():
				_push_feed("%s 击杀" % _player_tag(sim, index))
			else:
				_push_feed("%s 击杀 %s" % [
					_player_tag(sim, index), _player_tag(sim, deaths.pop_front())])
	while not deaths.is_empty():
		_push_feed("%s 阵亡" % _player_tag(sim, deaths.pop_front()))
	_prev_alive = alive
	_prev_kills = kills
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
