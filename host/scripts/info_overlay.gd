class_name InfoOverlay
extends CanvasLayer
## DEP-02 信息层表现：受击方向弧（≤0.8s）。
## 只读 sim/store；规则开关来自 settings_registry `gameplay.info.*`，不写任何状态。
##
## debug1 起移除了“声音来源”画圈 + “声源 N m”文字：它把玩家自己的枪声也当成事件（永远 0 m），
## 数值本身没有对局意义，只会在开火时糊住 HUD。听觉信息回归听声辨位（音频层），不再画读数。

const HIT_LIFETIME := 0.8
const ARC_RADIUS := 96.0

var _canvas: Control
var _sim: RefCounted
var _store: SettingsStore
var _hit_enabled := true
var _hit_angle := 0.0
var _hit_left := 0.0
var _damage_seen := 0


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


## 设置热应用：各信息层独立开/关。
func refresh() -> void:
	if _store == null:
		return
	_hit_enabled = bool(_store.get_value("gameplay.info.hit_direction"))
	_canvas.queue_redraw()


func push_damage(angle_degrees: float) -> void:
	_hit_angle = angle_degrees
	_hit_left = HIT_LIFETIME
	_canvas.queue_redraw()


func hit_arc_active() -> bool:
	return _hit_enabled and _hit_left > 0.0


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
	_canvas.queue_redraw()


func _draw_overlay() -> void:
	if not _hit_enabled or _hit_left <= 0.0:
		return
	var center := _canvas.get_viewport().get_visible_rect().size * 0.5
	var alpha := clampf(_hit_left / HIT_LIFETIME, 0.0, 1.0)
	var angle := deg_to_rad(_hit_angle - 90.0)
	_canvas.draw_arc(center, ARC_RADIUS, angle - 0.32, angle + 0.32, 32,
			Color(1.0, 0.30, 0.25, alpha), 5.0, true)
