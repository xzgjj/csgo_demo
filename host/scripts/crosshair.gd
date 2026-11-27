class_name Crosshair
extends Control
## 准星（SPEC-02/05）：四段线 + 中心点，间距由内核"当前实际扩散（度）"驱动；
## 样式/颜色/粗细/静态间隙/中心点由设置项注入（gameplay.crosshair.*，默认值保持旧观感）。
## 数据只读 sim（get_effective_inaccuracy），本节点不参与规则；截图验收读 gap 属性。

const BASE_GAP := 5.0          # 静止时四段线离中心的最小间距（像素）
const PIXELS_PER_DEGREE := 9.0 # 扩散每增加 1° 张开多少像素
const LINE_LENGTH := 7.0
const THICKNESS := 2.0
const DOT_RADIUS := 1.5
const COLOR := Color(0.55, 1.0, 0.65)

var gap := BASE_GAP
var spread_degrees := 0.0
var base_gap := BASE_GAP
var line_length := LINE_LENGTH
var thickness := THICKNESS
var dot_radius := DOT_RADIUS
var color := COLOR
var dot_enabled := true
var style := "classic"
var dynamic := true


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)


## 设置注入（SPEC-05 target=hud）：未知字段保持当前值。
func configure(config: Dictionary) -> void:
	base_gap = float(config.get("gap", base_gap))
	thickness = float(config.get("thickness", thickness))
	var new_color: Variant = config.get("color", color)
	if new_color is Color:
		color = new_color
	dot_enabled = bool(config.get("dot", dot_enabled))
	style = str(config.get("style", style))
	dynamic = bool(config.get("dynamic", dynamic))
	if not dynamic:
		gap = base_gap
	queue_redraw()


## 由 main 每帧调用：degrees = 内核 EffectiveInaccuracy（含 base/姿态/连发累计）。
func set_spread(degrees: float) -> void:
	spread_degrees = maxf(0.0, degrees)
	gap = base_gap + (spread_degrees * PIXELS_PER_DEGREE if dynamic else 0.0)
	queue_redraw()


func _draw() -> void:
	var center := size * 0.5
	var inner := gap
	var arm := line_length * (1.6 if style == "cross" else 1.0)
	var outer := gap + arm
	# 上/下/左/右四段（留出中心缺口，扩散越大缺口越大）。
	_draw_line(center + Vector2(0, -inner), center + Vector2(0, -outer))
	_draw_line(center + Vector2(0, inner), center + Vector2(0, outer))
	_draw_line(center + Vector2(-inner, 0), center + Vector2(-outer, 0))
	_draw_line(center + Vector2(inner, 0), center + Vector2(outer, 0))
	if dot_enabled or style == "dot":
		draw_circle(center, maxf(dot_radius, thickness * 0.5) if style == "dot" else dot_radius, color)


func _draw_line(from: Vector2, to: Vector2) -> void:
	if style == "dot":
		return
	if style == "tshape" and from.y > to.y:
		return
	draw_line(from, to, color, thickness)
