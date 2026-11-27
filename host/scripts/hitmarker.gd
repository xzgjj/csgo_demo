class_name Hitmarker
extends Control
## 命中标记（SPEC-02/04）：白色 X；爆头红色；穿墙加外层描边。
## 由 main 依据内核 DamageApplied 事件（部位/爆头/穿墙）触发，仅表现层。

const LIFE := 0.16
const ARM := 9.0
const THICKNESS := 2.0
const COLOR_BODY := Color(0.92, 0.96, 1.0)
const COLOR_HEAD := Color(1.0, 0.32, 0.28)
const COLOR_WALL := Color(0.75, 0.82, 0.9)

var _life := 0.0
var _headshot := false
var _through_wall := false


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	set_process(true)


func flash(headshot: bool, through_wall: bool) -> void:
	_headshot = headshot
	_through_wall = through_wall
	_life = LIFE
	queue_redraw()


func active() -> bool:
	return _life > 0.0


func _process(delta: float) -> void:
	if _life <= 0.0:
		return
	_life = maxf(0.0, _life - delta)
	queue_redraw()


func _draw() -> void:
	if _life <= 0.0:
		return
	var center := size * 0.5
	var alpha := _life / LIFE
	var color := COLOR_HEAD if _headshot else COLOR_BODY
	color.a = alpha
	var offset := ARM * 0.5
	# 四条 45° 斜线组成 X（向下旋转，外扩 45°）。
	var dirs: Array[Vector2] = [
		Vector2(1.0, 1.0), Vector2(-1.0, 1.0), Vector2(1.0, -1.0), Vector2(-1.0, -1.0),
	]
	for dir in dirs:
		var start := center + dir * offset
		var end := center + dir * (offset + ARM)
		if _through_wall:
			var wall := COLOR_WALL
			wall.a = alpha
			draw_line(start, end, wall, THICKNESS + 2.0)
		draw_line(start, end, color, THICKNESS)
