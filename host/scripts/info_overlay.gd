class_name InfoOverlay
extends CanvasLayer
## DEP-02 信息层表现：受击方向弧（≤0.8s）+ 声音来源提示（≤1.2s）。
## 只读 sim/store；规则开关来自 settings_registry `gameplay.info.*`，不写任何状态。

const HIT_LIFETIME := 0.8
const SOUND_LIFETIME := 1.2
const ARC_RADIUS := 96.0

var _canvas: Control
var _sim: RefCounted
var _store: SettingsStore
var _hit_enabled := true
var _sound_enabled := true
var _hit_angle := 0.0
var _hit_left := 0.0
var _damage_seen := 0
var _pings: Array[Dictionary] = []
var _origin := Vector2.ZERO


func setup(sim_ref: RefCounted, store: SettingsStore) -> void:
	_sim = sim_ref
	_store = store
	layer = 3
	_canvas = Control.new()
	_canvas.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	_canvas.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_canvas.draw.connect(_draw_overlay)
	add_child(_canvas)
	refresh()


## 设置热应用：四个信息层各自独立开/关。
func refresh() -> void:
	if _store == null:
		return
	_hit_enabled = bool(_store.get_value("gameplay.info.hit_direction"))
	_sound_enabled = bool(_store.get_value("gameplay.info.sound_visual"))
	if not _sound_enabled:
		_pings.clear()
	_canvas.queue_redraw()


func push_damage(angle_degrees: float) -> void:
	_hit_angle = angle_degrees
	_hit_left = HIT_LIFETIME
	_canvas.queue_redraw()


func push_sound(distance_units: float) -> void:
	if _sound_enabled and _pings.size() < 4:
		_pings.append({"left": SOUND_LIFETIME, "distance": distance_units})
		_canvas.queue_redraw()


func hit_arc_active() -> bool:
	return _hit_enabled and _hit_left > 0.0


func ping_count() -> int:
	return _pings.size()


func _process(delta: float) -> void:
	if _sim != null:
		update(delta)


func update(delta: float) -> void:
	_hit_left = maxf(_hit_left - delta, 0.0)
	if _sim != null and _sim.has_method("get_damage_taken"):
		var taken := int(_sim.get_damage_taken(0))
		if taken > _damage_seen:
			_damage_seen = taken
			push_damage(float(_sim.get_last_hit_direction(0)))
	if _sound_enabled and _sim != null and _sim.has_method("sound_events"):
		for row: Dictionary in _sim.sound_events():
			if _pings.size() < 4:
				_pings.append({"left": SOUND_LIFETIME, "distance": float(row["distance"])})
	for ping: Dictionary in _pings:
		ping["left"] = float(ping["left"]) - delta
	while not _pings.is_empty() and float(_pings[0]["left"]) <= 0.0:
		_pings.remove_at(0)
	_canvas.queue_redraw()


func _draw_overlay() -> void:
	var center := _canvas.get_viewport().get_visible_rect().size * 0.5
	if _hit_enabled and _hit_left > 0.0:
		var alpha := clampf(_hit_left / HIT_LIFETIME, 0.0, 1.0)
		var angle := deg_to_rad(_hit_angle - 90.0)
		_canvas.draw_arc(center, ARC_RADIUS, angle - 0.32, angle + 0.32, 32,
				Color(1.0, 0.30, 0.25, alpha), 5.0, true)
	if _sound_enabled:
		for i in _pings.size():
			var ping: Dictionary = _pings[i]
			var y := center.y + 150.0 + float(i) * 26.0
			var radius := 10.0 + float(ping["distance"]) / 80.0
			_canvas.draw_arc(Vector2(center.x - 320.0, y), radius, 0.0, TAU, 24,
					Color(0.45, 0.85, 1.0, 0.7), 2.0, true)
			_canvas.draw_string(ThemeDB.fallback_font, Vector2(center.x - 300.0, y + 5.0),
					"声源 %d m" % int(float(ping["distance"]) / 32.0),
					HORIZONTAL_ALIGNMENT_LEFT, -1, 13, Color(0.75, 0.92, 1.0, 0.85))
