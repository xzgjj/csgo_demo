class_name DeAshWorld
extends Node3D
## demo 世界：优先按 host/scenes/de_ash_boxes.json 生成 de_ash（墙 / 车道 / 包点 / 出生 / 道具，
## 材质由 MaterialLib 按 kind 选择）；JSON 缺失或非法时回退到与内核 ±800 对齐的 1600 灰盒。
## 包点/出生锚点直接取盒子清单里 kind=site/spawn 的中心，保证与布局数据一致。

const DEFAULT_BOXES_PATH := "res://scenes/de_ash_boxes.json"
const FALLBACK_HALF := 800.0
const FALLBACK_WALL_HEIGHT := 200.0
const FALLBACK_WALL_THICKNESS := 32.0
const SITE_LABEL_COLOR := Color(0.95, 0.45, 0.35)
const SPAWN_LABEL_COLOR := Color(0.45, 0.72, 0.98)

var source := "none"
var object_count := 0
var spawn_anchors: Array[Vector3] = []
var site_anchors: Array[Vector3] = []
var blockout: BlockoutBuilder
var _fallback_root: Node3D


func build(boxes_path: String = DEFAULT_BOXES_PATH) -> bool:
	_build_environment()
	if not FileAccess.file_exists(boxes_path):
		push_warning("de_ash 盒子清单缺失，使用灰盒 fallback: " + boxes_path)
		_build_fallback_arena()
		return false
	blockout = BlockoutBuilder.new()
	blockout.name = "DeAshBlockout"
	add_child(blockout)
	if not blockout.build(boxes_path):
		remove_child(blockout)
		blockout.free()
		blockout = null
		push_warning("de_ash 盒子清单非法，使用灰盒 fallback: " + boxes_path)
		_build_fallback_arena()
		return false
	source = "de_ash"
	object_count = blockout.box_count
	for anchor: Vector3 in blockout.spawn_anchors:
		spawn_anchors.append(anchor)
	for anchor: Vector3 in blockout.site_anchors:
		site_anchors.append(anchor)
	_add_anchor_labels()
	return true


func _build_environment() -> void:
	var environment := Environment.new()
	environment.background_mode = Environment.BG_SKY
	environment.sky = _make_sky()
	environment.ambient_light_source = Environment.AMBIENT_SOURCE_SKY
	environment.ambient_light_energy = 0.60
	environment.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	var world_env := WorldEnvironment.new()
	world_env.name = "WorldEnvironment"
	world_env.environment = environment
	add_child(world_env)
	var sun := DirectionalLight3D.new()
	sun.name = "Sun"
	sun.rotation_degrees = Vector3(-50.0, -35.0, 0.0)
	sun.light_energy = 1.6
	sun.shadow_enabled = true
	add_child(sun)


func _make_sky() -> Sky:
	var sky_material := ProceduralSkyMaterial.new()
	sky_material.sky_top_color = Color(0.15, 0.20, 0.30)
	sky_material.sky_horizon_color = Color(0.55, 0.60, 0.68)
	sky_material.ground_bottom_color = Color(0.08, 0.09, 0.10)
	sky_material.ground_horizon_color = Color(0.30, 0.32, 0.35)
	var sky := Sky.new()
	sky.sky_material = sky_material
	return sky


func _build_fallback_arena() -> void:
	source = "fallback"
	spawn_anchors.clear()
	site_anchors.clear()
	_fallback_root = Node3D.new()
	_fallback_root.name = "FallbackArena"
	add_child(_fallback_root)
	var concrete := MaterialLib.get_material("concrete")
	var floor_width := FALLBACK_HALF * 2.0
	_add_fallback_box(Vector3(floor_width, 16.0, floor_width), Vector3(0.0, -8.0, 0.0), concrete)
	var wall_center := FALLBACK_HALF + FALLBACK_WALL_THICKNESS * 0.5
	var half_height := FALLBACK_WALL_HEIGHT * 0.5
	var spans: Array = [
		[Vector3(FALLBACK_WALL_THICKNESS, FALLBACK_WALL_HEIGHT, floor_width),
			Vector3(wall_center, half_height, 0.0)],
		[Vector3(FALLBACK_WALL_THICKNESS, FALLBACK_WALL_HEIGHT, floor_width),
			Vector3(-wall_center, half_height, 0.0)],
		[Vector3(floor_width, FALLBACK_WALL_HEIGHT, FALLBACK_WALL_THICKNESS),
			Vector3(0.0, half_height, wall_center)],
		[Vector3(floor_width, FALLBACK_WALL_HEIGHT, FALLBACK_WALL_THICKNESS),
			Vector3(0.0, half_height, -wall_center)],
	]
	for span: Array in spans:
		_add_fallback_box(span[0], span[1], concrete)
	object_count = 1 + spans.size()


func _add_fallback_box(size: Vector3, at: Vector3, material: StandardMaterial3D) -> void:
	var mesh_instance := MeshInstance3D.new()
	var box_mesh := BoxMesh.new()
	box_mesh.size = size
	box_mesh.material = material
	mesh_instance.mesh = box_mesh
	mesh_instance.position = at
	_fallback_root.add_child(mesh_instance)


func _add_anchor_labels() -> void:
	for index in site_anchors.size():
		var site_label := _make_anchor_label("包点 %s" % _site_name(index), SITE_LABEL_COLOR)
		site_label.position = site_anchors[index] + Vector3(0.0, 130.0, 0.0)
	for index in spawn_anchors.size():
		var spawn_label := _make_anchor_label("%s 出生点" % _team_name(index), SPAWN_LABEL_COLOR)
		spawn_label.position = spawn_anchors[index] + Vector3(0.0, 130.0, 0.0)


func _make_anchor_label(text: String, color: Color) -> Label3D:
	var label := Label3D.new()
	label.text = text
	label.font_size = 96
	label.pixel_size = 0.02
	label.modulate = color
	label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	label.no_depth_test = true
	add_child(label)
	return label


func _site_name(index: int) -> String:
	if index == 0:
		return "A"
	if index == 1:
		return "B"
	return "S%d" % index


func _team_name(index: int) -> String:
	if index == 0:
		return "T"
	if index == 1:
		return "CT"
	return "P%d" % index
