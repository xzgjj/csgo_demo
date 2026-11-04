class_name ScopeOverlay
extends Control
## 开镜表现层（只读倍率，不写规则状态）：
## - 倍镜武器（AWP 4x）：圆形镜筒遮罩 + 十字准星 + 倍率文字；
## - 机瞄武器（AK/M4 1.0x）：只显示「机瞄 · 1.0x」提示，不遮画面。
## 遮罩纹理在 _draw 里按屏幕短边等比铺满，保证圆形不变形；headless 下不依赖 GPU 结果做断言。

const MASK_PIXELS := 256
const SCOPE_INNER_RATIO := 0.34  # 镜筒内圈占纹理边长的比例
const RING_RATIO := 0.008
const TEXT_COLOR := Color(0.92, 0.96, 1.0)
const IRON_TEXT_COLOR := Color(0.85, 0.90, 0.95, 0.75)
const WORLD_SCALE := 1.0

var _mask: ImageTexture
var _magnification := 1.0
var _weapon_name := ""
var _label: Label


func build() -> void:
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	visible = false
	_mask = ImageTexture.create_from_image(_make_mask_image())
	_label = Label.new()
	_label.add_theme_font_size_override("font_size", 20)
	_label.add_theme_color_override("font_color", TEXT_COLOR)
	_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	add_child(_label)
	_place_label()


## Label 手动贴到屏幕底部中央（CanvasLayer 子节点不会自动继承尺寸，锚点不可靠）。
func _place_label() -> void:
	if _label == null:
		return
	var viewport_size := get_viewport_rect().size
	_label.size = Vector2(520.0, 26.0)
	_label.position = Vector2((viewport_size.x - 520.0) * 0.5, viewport_size.y - 92.0)


func _notification(what: int) -> void:
	if what == NOTIFICATION_RESIZED:
		_place_label()


## magnification ≤ 1 为机瞄（只提示不遮罩）；> 1 为倍镜（遮罩 + 准星）。
func set_scope(magnification: float, weapon_name: String) -> void:
	_magnification = magnification
	_weapon_name = weapon_name
	if _label != null:
		_label.text = _scope_text()
		_label.add_theme_color_override("font_color",
				TEXT_COLOR if is_scoped() else IRON_TEXT_COLOR)
		_place_label()
	queue_redraw()


func is_scoped() -> bool:
	return _magnification > 1.0


func scope_text() -> String:
	return _scope_text()


func _scope_text() -> String:
	if is_scoped():
		return "%s · %.1fx 开镜" % [_weapon_name, _magnification]
	return "%s · 机瞄 %.1fx" % [_weapon_name, _magnification]


func _draw() -> void:
	if not is_scoped() or _mask == null:
		return
	var viewport_size := get_viewport_rect().size
	var side := maxf(viewport_size.x, viewport_size.y)
	var top_left := (viewport_size - Vector2(side, side)) * 0.5
	draw_texture_rect(_mask, Rect2(top_left, Vector2(side, side)), false)
	var center := viewport_size * 0.5
	var radius := side * 0.5 * SCOPE_INNER_RATIO
	var cross_color := Color(0.03, 0.03, 0.03, 0.9)
	var length := radius * 1.9
	draw_line(center - Vector2(length, 0.0), center + Vector2(length, 0.0), cross_color, 2.0)
	draw_line(center - Vector2(0.0, length), center + Vector2(0.0, length), cross_color, 2.0)
	draw_circle(center, 2.0, cross_color)
	draw_arc(center, radius, 0.0, TAU, 96, Color(0.02, 0.02, 0.02, 0.95), 3.0)


## 生成镜筒遮罩：外圈黑色不透明、内圈透明（圆形），边缘一圈细黑环。
func _make_mask_image() -> Image:
	var image := Image.create(MASK_PIXELS, MASK_PIXELS, false, Image.FORMAT_RGBA8)
	var center := (MASK_PIXELS - 1) * 0.5
	var inner := MASK_PIXELS * 0.5 * SCOPE_INNER_RATIO
	for y in MASK_PIXELS:
		for x in MASK_PIXELS:
			var distance := Vector2(float(x) - center, float(y) - center).length()
			var alpha := 1.0 if distance > inner else 0.0
			image.set_pixel(x, y, Color(0.0, 0.0, 0.0, alpha))
	return image
