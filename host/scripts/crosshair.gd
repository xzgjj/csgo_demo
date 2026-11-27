class_name Crosshair
extends Control
## 准星（SPEC-02）：四段线 + 中心点，间距由内核"当前实际扩散（度）"驱动。
## 数据只读 sim（get_effective_inaccuracy），本节点不参与规则；截图验收读 gap 属性。

const BASE_GAP := 5.0          # 静止时四段线离中心的最小间距（像素）
const PIXELS_PER_DEGREE := 9.0 # 扩散每增加 1° 张开多少像素
const LINE_LENGTH := 7.0
const THICKNESS := 2.0
const DOT_RADIUS := 1.5
const COLOR := Color(0.55, 1.0, 0.65)

var gap := BASE_GAP
var spread_degrees := 0.0


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)


## 由 main 每帧调用：degrees = 内核 EffectiveInaccuracy（含 base/姿态/连发累计）。
func set_spread(degrees: float) -> void:
	spread_degrees = maxf(0.0, degrees)
	gap = BASE_GAP + spread_degrees * PIXELS_PER_DEGREE
	queue_redraw()


func _draw() -> void:
	var center := size * 0.5
	var inner := gap
	var outer := gap + LINE_LENGTH
	# 上/下/左/右四段（留出中心缺口，扩散越大缺口越大）。
	draw_line(center + Vector2(0, -inner), center + Vector2(0, -outer), COLOR, THICKNESS)
	draw_line(center + Vector2(0, inner), center + Vector2(0, outer), COLOR, THICKNESS)
	draw_line(center + Vector2(-inner, 0), center + Vector2(-outer, 0), COLOR, THICKNESS)
	draw_line(center + Vector2(inner, 0), center + Vector2(outer, 0), COLOR, THICKNESS)
	draw_circle(center, DOT_RADIUS, COLOR)
