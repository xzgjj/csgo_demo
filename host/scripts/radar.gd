class_name RadarPanel
extends Control
## 模块 4：2D 雷达/小地图 + 右上角队友存活条（表现层，只读 sim）。
## - M 键本地开关（_unhandled_input，不依赖 main.gd）；_process 自刷新（读 main.sim/main.playing/main.yaw）。
## - 地图轮廓读 host/scenes/de_ash_boxes.json：Godot 坐标（x=-sim_y, y=sim_z, z=-sim_x）
##   投影成 sim x/y → 雷达局部坐标；headless 不依赖 _draw，断言用 data 接口。
## - 自己 = 白色箭头（朝向取玩家 yaw）；队友 = 队伍色点（T 橙 / CT 蓝）。
## - 敌人只在「暴露」时显示：优先内核 is_visible(viewer,target)；该绑定未就绪时一律不显示
##   （exposure_source()=="pending"），不崩溃、不假装可见。
## - 右上角存活条：固定 5 盏灯 + 存活数（按 sim.get_team 归队，灯数与 get_alive_count 一致）。
## 接入（main.gd 由 Lead 接线，共 3 行）：
##   var radar := RadarPanel.new(); hud.add_child(radar); radar.setup(self)

const BOXES_PATH := "res://scenes/de_ash_boxes.json"
## sim 平面 x/y 的默认范围（de_ash 4096 地图 + 外沿）；盒子清单缺失时回退。
const DEFAULT_WORLD_RECT := Rect2(-2080.0, -2080.0, 4160.0, 4160.0)
const RADAR_RECT := Rect2(20.0, 132.0, 240.0, 240.0)
const SQUAD_SIZE := 5
const SQUAD_ORIGIN := Vector2(1050.0, 14.0)
const SQUAD_BAR_SIZE := Vector2(210.0, 44.0)
const YAW_UNSET := 1.0e30

const BG_COLOR := Color(0.03, 0.04, 0.06, 0.78)
const BORDER_COLOR := Color(0.45, 0.52, 0.62, 0.95)
const WALL_COLOR := Color(0.46, 0.50, 0.56, 0.62)
const PROP_COLOR := Color(0.34, 0.38, 0.44, 0.55)
const STAIR_COLOR := Color(0.52, 0.47, 0.40, 0.55)
const SITE_COLOR := Color(0.78, 0.30, 0.24, 0.50)
const SPAWN_COLOR := Color(0.30, 0.52, 0.86, 0.42)
const OWN_COLOR := Color(1.0, 1.0, 1.0)
const OWN_OUTLINE_COLOR := Color(0.05, 0.06, 0.08, 0.95)
const ENEMY_COLOR := Color(0.96, 0.28, 0.24)
const TEAM_T_COLOR := Color(0.93, 0.62, 0.24)
const TEAM_CT_COLOR := Color(0.42, 0.66, 0.95)
const LAMP_DEAD_COLOR := Color(0.20, 0.22, 0.26, 0.95)
const TEXT_COLOR := Color(0.92, 0.95, 1.0)

## 地面以上但非行走面的盒子（lintel/roof/catwalk）不进 2D 轮廓，避免雷达糊成一片。
const SKIP_KINDS: Array[String] = ["floor", "lintel", "roof", "catwalk"]

var enabled := true
var main_node: Node = null

var _built := false
var _sim: RefCounted = null
var _world_rect := DEFAULT_WORLD_RECT
var _scale := 1.0
var _wall_rects: Array[Rect2] = []
var _wall_colors: Array[Color] = []
var _site_positions: Array[Vector2] = []
var _site_names: Array[String] = []
var _markers: Dictionary = {}
var _marker_enemies: Dictionary = {}
var _lamps: Array[bool] = []
var _team_members := 0
var _viewer_team := 1
var _viewer_yaw := 0.0
var _exposure_source := "pending"
var _squad_title: Label
var _lamp_nodes: Array[ColorRect] = []
var _map_title: Label

func _ready() -> void:
	build()

## 绑定宿主：之后 _process 自动按 main.sim / main.playing / main.yaw 刷新（无需 _update_hud 接线）。
func setup(main_inst: Node) -> void:
	main_node = main_inst
	build()
	_apply_visible()

func build() -> void:
	if _built:
		return
	_built = true
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	_load_map()
	_build_squad_bar()
	_map_title = Label.new()
	_map_title.text = "雷达 [M]"
	_map_title.position = RADAR_RECT.position + Vector2(6.0, -22.0)
	_map_title.add_theme_font_size_override("font_size", 15)
	_map_title.add_theme_color_override("font_color", TEXT_COLOR)
	_map_title.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_map_title)

func toggle() -> void:
	set_open(not enabled)

func set_open(value: bool) -> void:
	enabled = value
	_apply_visible()

func is_open() -> bool:
	return enabled

## 数据入口：重算自己/队友/敌人点位与存活条；yaw 省略时沿用上次（_process 会喂 main.yaw）。
func refresh(sim_ref: RefCounted, viewer_yaw_degrees: float = YAW_UNSET) -> void:
	if not _built:
		build()
	_sim = sim_ref
	_markers.clear()
	_marker_enemies.clear()
	if sim_ref == null:
		queue_redraw()
		return
	if viewer_yaw_degrees < YAW_UNSET * 0.5:
		_viewer_yaw = viewer_yaw_degrees
	var count: int = int(sim_ref.get_player_count())
	_viewer_team = int(sim_ref.get_team(0))
	for index in count:
		var team: int = int(sim_ref.get_team(index))
		if not bool(sim_ref.is_alive(index)):
			continue
		if index != 0 and team != _viewer_team and not is_exposed(0, index):
			continue
		_markers[index] = world_to_radar(float(sim_ref.get_x(index)), float(sim_ref.get_y(index)))
		_marker_enemies[index] = index != 0 and team != _viewer_team
	_refresh_squad(sim_ref)
	queue_redraw()

## 敌人暴露判定：有内核绑定走 is_visible；未就绪时一律 false（不写假显示）。
func is_exposed(viewer: int, target: int) -> bool:
	if _sim == null:
		_exposure_source = "pending"
		return false
	if _sim.has_method("is_visible"):
		_exposure_source = "kernel"
		return int(_sim.call("is_visible", viewer, target)) != 0
	_exposure_source = "pending"
	return false

func exposure_source() -> String:
	return _exposure_source

func world_rect() -> Rect2:
	return _world_rect

func map_rect() -> Rect2:
	return RADAR_RECT

func map_wall_count() -> int:
	return _wall_rects.size()

func map_rects() -> Array[Rect2]:
	return _wall_rects.duplicate()

func has_marker(index: int) -> bool:
	return _markers.has(index)

func marker_position(index: int) -> Vector2:
	return _markers.get(index, Vector2.ZERO)

func marker_positions() -> Dictionary:
	return _markers.duplicate()

func visible_enemies() -> Array[int]:
	var enemies: Array[int] = []
	for key in _markers.keys():
		if bool(_marker_enemies.get(key, false)):
			enemies.append(int(key))
	return enemies

func own_heading_degrees() -> float:
	return wrapf(_viewer_yaw - 90.0, -180.0, 180.0)

func squad_lamps() -> Array[bool]:
	return _lamps.duplicate()

func squad_size() -> int:
	return _team_members

func squad_alive_count() -> int:
	var total := 0
	for state in _lamps:
		if state:
			total += 1
	return total

## de_ash 盒子清单 → sim x/y 世界范围 + 雷达局部矩形（_draw 直接画，不依赖 GPU 才能算出的数据）。
func _load_map() -> void:
	_world_rect = DEFAULT_WORLD_RECT
	_wall_rects.clear()
	_wall_colors.clear()
	_site_positions.clear()
	_site_names.clear()
	var text := FileAccess.get_file_as_string(BOXES_PATH)
	if text.is_empty():
		_add_fallback_outline()
		return
	var data: Variant = JSON.parse_string(text)
	if typeof(data) != TYPE_DICTIONARY:
		_add_fallback_outline()
		return
	var boxes: Array = (data as Dictionary).get("boxes", [])
	if boxes.is_empty():
		_add_fallback_outline()
		return
	var min_sim := Vector2(INF, INF)
	var max_sim := Vector2(-INF, -INF)
	for item: Dictionary in boxes:
		var extent := _box_sim_extent(item)
		min_sim.x = minf(min_sim.x, extent.position.x)
		min_sim.y = minf(min_sim.y, extent.position.y)
		max_sim.x = maxf(max_sim.x, extent.end.x)
		max_sim.y = maxf(max_sim.y, extent.end.y)
	if min_sim.x < max_sim.x and min_sim.y < max_sim.y:
		_world_rect = Rect2(min_sim, max_sim - min_sim)
	_scale = minf(RADAR_RECT.size.x / _world_rect.size.x,
			RADAR_RECT.size.y / _world_rect.size.y)
	for item: Dictionary in boxes:
		var color := _kind_color(str(item.get("kind", "solid")))
		if color.a <= 0.0:
			continue
		var extent := _box_sim_extent(item)
		var a := world_to_radar(extent.position.x, extent.position.y)
		var b := world_to_radar(extent.end.x, extent.end.y)
		var rect := Rect2(Vector2(minf(a.x, b.x), minf(a.y, b.y)),
				Vector2(absf(b.x - a.x), absf(b.y - a.y)))
		_wall_rects.append(rect)
		_wall_colors.append(color)
		if str(item.get("kind", "")) == "site":
			_site_positions.append(rect.position + rect.size * 0.5)
			_site_names.append(String.chr(65 + _site_names.size()))
	if _wall_rects.is_empty():
		_add_fallback_outline()

## 一个盒子在 sim x/y 平面的范围：Godot 中心/尺寸 → sim（x=-gz, y=-gx）。
func _box_sim_extent(item: Dictionary) -> Rect2:
	var center: Array = item["center"]
	var size: Array = item["size"]
	var gx := float(center[0])
	var gz := float(center[2])
	var sx := float(size[0])
	var sz := float(size[2])
	return Rect2(Vector2(-gz - sz * 0.5, -gx - sx * 0.5), Vector2(sz, sx))

func _kind_color(kind: String) -> Color:
	if kind in SKIP_KINDS:
		return Color(0.0, 0.0, 0.0, 0.0)
	if kind == "site":
		return SITE_COLOR
	if kind == "spawn":
		return SPAWN_COLOR
	if kind == "prop":
		return PROP_COLOR
	if kind == "stairs" or kind == "ramp":
		return STAIR_COLOR
	return WALL_COLOR

func _add_fallback_outline() -> void:
	_wall_rects.append(RADAR_RECT.grow(-3.0))
	_wall_colors.append(WALL_COLOR)

## sim x/y → 雷达局部坐标：+sim x 向上（-screen y），+sim y 向右（+screen x）。
func world_to_radar(sim_x: float, sim_y: float) -> Vector2:
	var world_center := _world_rect.position + _world_rect.size * 0.5
	var map_center := RADAR_RECT.position + RADAR_RECT.size * 0.5
	return map_center + Vector2((sim_y - world_center.y) * _scale,
			-(sim_x - world_center.x) * _scale)

func _refresh_squad(sim_ref: RefCounted) -> void:
	_lamps.clear()
	var count: int = int(sim_ref.get_player_count())
	for index in count:
		if int(sim_ref.get_team(index)) == _viewer_team:
			_lamps.append(bool(sim_ref.is_alive(index)))
	_team_members = _lamps.size()
	while _lamps.size() < SQUAD_SIZE:
		_lamps.append(false)
	_squad_title.text = "%s 存活 %d/%d" % [_team_name(_viewer_team), squad_alive_count(),
			_team_members]
	_squad_title.add_theme_color_override("font_color", _team_color(_viewer_team))
	for index in _lamp_nodes.size():
		var lamp := _lamp_nodes[index]
		var in_team := index < _team_members
		lamp.visible = in_team
		lamp.color = LAMP_DEAD_COLOR
		if in_team and _lamps[index]:
			lamp.color = _team_color(_viewer_team)

func _build_squad_bar() -> void:
	var bar := Control.new()
	bar.name = "SquadBar"
	bar.position = SQUAD_ORIGIN
	bar.size = SQUAD_BAR_SIZE
	bar.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(bar)
	_squad_title = Label.new()
	_squad_title.text = ""
	_squad_title.position = Vector2.ZERO
	_squad_title.size = Vector2(SQUAD_BAR_SIZE.x, 20.0)
	_squad_title.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	_squad_title.add_theme_font_size_override("font_size", 16)
	_squad_title.add_theme_color_override("font_color", TEXT_COLOR)
	_squad_title.mouse_filter = Control.MOUSE_FILTER_IGNORE
	bar.add_child(_squad_title)
	var lamps_width := float(SQUAD_SIZE) * 24.0 - 4.0
	for index in SQUAD_SIZE:
		var lamp := ColorRect.new()
		lamp.name = "Lamp%d" % index
		lamp.position = Vector2(SQUAD_BAR_SIZE.x - lamps_width + float(index) * 24.0, 24.0)
		lamp.size = Vector2(20.0, 14.0)
		lamp.color = LAMP_DEAD_COLOR
		lamp.mouse_filter = Control.MOUSE_FILTER_IGNORE
		bar.add_child(lamp)
		_lamp_nodes.append(lamp)

func _team_color(team: int) -> Color:
	return TEAM_T_COLOR if team == 1 else TEAM_CT_COLOR

func _team_name(team: int) -> String:
	return "T" if team == 1 else "CT"

func _apply_visible() -> void:
	var show := enabled
	if main_node != null:
		show = enabled and bool(main_node.get("playing"))
	visible = show

## 自刷新：main 绑定后每帧读 sim/playing/yaw；不绑 main 时（如测试）不动 visible。
func _process(_delta: float) -> void:
	if main_node == null:
		return
	var sim_ref: Variant = main_node.get("sim")
	_apply_visible()
	if not visible or sim_ref == null:
		return
	refresh(sim_ref as RefCounted, float(main_node.get("yaw")))

func _unhandled_input(event: InputEvent) -> void:
	if not (event is InputEventKey):
		return
	var key := event as InputEventKey
	if not key.pressed or key.echo:
		return
	if key.keycode != KEY_M and key.physical_keycode != KEY_M:
		return
	if main_node != null and not bool(main_node.get("playing")):
		return
	toggle()
	get_viewport().set_input_as_handled()

func _draw() -> void:
	draw_rect(RADAR_RECT, BG_COLOR, true)
	for index in _wall_rects.size():
		draw_rect(_wall_rects[index], _wall_colors[index], true)
	_draw_site_labels()
	draw_rect(RADAR_RECT, BORDER_COLOR, false, 2.0)
	# 先画队友/敌人，最后画自己：白箭头必须在最上层，否则被同队点位盖住。
	var teammate_points: Array[Vector2] = []
	var enemy_points: Array[Vector2] = []
	for key in _markers.keys():
		var index := int(key)
		if index == 0:
			continue
		var pos: Vector2 = _markers[key]
		if bool(_marker_enemies.get(key, false)):
			enemy_points.append(pos)
		else:
			teammate_points.append(pos)
	for pos in teammate_points:
		draw_circle(pos, 3.5, _team_color(_viewer_team))
	for pos in enemy_points:
		draw_circle(pos, 4.0, ENEMY_COLOR)
		draw_arc(pos, 6.5, 0.0, TAU, 14, ENEMY_COLOR, 1.0, true)
	if _markers.has(0):
		var own_pos: Vector2 = _markers[0]
		_draw_own_arrow(own_pos)

func _draw_site_labels() -> void:
	var font := ThemeDB.fallback_font
	if font == null:
		return
	for index in _site_positions.size():
		draw_string(font, _site_positions[index] + Vector2(-5.0, 5.0), _site_names[index],
				HORIZONTAL_ALIGNMENT_LEFT, -1.0, 13, Color(0.95, 0.58, 0.52))

func _draw_own_arrow(pos: Vector2) -> void:
	var direction := Vector2.from_angle(deg_to_rad(own_heading_degrees()))
	var side := direction.orthogonal()
	var tip := pos + direction * 11.0
	var tail_left := pos - direction * 6.0 + side * 7.0
	var tail_right := pos - direction * 6.0 - side * 7.0
	draw_colored_polygon(PackedVector2Array([tip, tail_left, tail_right]), OWN_COLOR)
	draw_polyline(PackedVector2Array([tip, tail_left, tail_right, tip]),
			OWN_OUTLINE_COLOR, 1.5, true)
