class_name MapLoader
extends RefCounted
## 地图派生盒子（Godot 坐标）→ 内核 MapDef AABB：碰撞与画面用同一份数据。
## 盒子由 tools/mapgen/gen_blockout.py 从 layout JSON 派生（唯一几何事实源）。
## 换图只改 host/scenes/map_registry.json；默认地图仍由注册表 default 决定（当前 de_ash）。

const BOXES_PATH := "res://scenes/de_ash_boxes.json"
const REGISTRY_PATH := "res://scenes/map_registry.json"


## 默认地图注入（等价 apply_map(default_map_id())）。
static func apply(sim: RefCounted) -> int:
	return apply_map(sim, default_map_id())


## 按 map_id 注入；id 不存在或缺文件时回退默认地图，再失败则返回 0（保持竞技场）。
static func apply_map(sim: RefCounted, map_id: String) -> int:
	var path := boxes_path_for(map_id)
	if path.is_empty():
		push_warning("地图 %s 未注册，回退 %s" % [map_id, default_map_id()])
		path = boxes_path_for(default_map_id())
	if path.is_empty() or not FileAccess.file_exists(path):
		push_warning("地图 %s 缺少盒子清单，保持默认竞技场" % map_id)
		return 0
	return apply_path(sim, path)


## 指定清单注入（换图/测试用；默认路径行为不变）。
static func apply_path(sim: RefCounted, path: String) -> int:
	var text := FileAccess.get_file_as_string(path)
	if text.is_empty():
		push_warning("缺少 %s，保持默认竞技场" % path)
		return 0
	var data: Variant = JSON.parse_string(text)
	if typeof(data) != TYPE_DICTIONARY:
		push_warning("%s 非法，保持默认竞技场" % path)
		return 0
	var flat := PackedFloat32Array()
	for item: Dictionary in data.get("boxes", []):
		if not bool(item.get("collision", true)):
			continue
		flat.append_array(_flatten_box(item))
	var expected := flat.size() / 8
	var written: int = sim.set_map_boxes(flat)
	if written != expected:
		push_error("碰撞盒注入不完整：期望 %d，实际 %d（检查尺寸/旋转/材质/容量）" % [expected, written])
	_inject_layout_points(sim, data)
	return written


## 从盒子清单提取 kind=site / kind=spawn 的中心并注入内核，替换原先写死在 main.gd 的坐标。
## 坐标映射与 _flatten_box 一致：sim x = -gz，sim y = -gx。
## 规则：spawn 按 sim y 升序取 [T, CT]（de_ash/dust2 的 T 出生屋都在 -y 侧）；
##      包点 A 取“距 T 出生点更近”的一个，B 为另一个。缺数据时保持内核默认（不阻断对局）。
static func _inject_layout_points(sim: RefCounted, data: Dictionary) -> void:
	var spawns: Array[Vector2] = []
	var sites: Array[Vector2] = []
	for item: Dictionary in data.get("boxes", []):
		var kind := str(item.get("kind", ""))
		if kind != "site" and kind != "spawn":
			continue
		var center: Array = item.get("center", [])
		if center.size() < 3:
			continue
		var point := Vector2(-float(center[2]), -float(center[0]))
		if kind == "site":
			sites.append(point)
		else:
			spawns.append(point)
	spawns.sort_custom(_sort_by_y)
	if spawns.size() >= 2:
		var spawn_t: Vector2 = spawns[0]
		var spawn_ct: Vector2 = spawns[spawns.size() - 1]
		sim.set_spawn_centers(spawn_t.x, spawn_t.y, spawn_ct.x, spawn_ct.y)
		if sites.size() >= 2:
			var site_a: Vector2 = sites[0]
			var site_b: Vector2 = sites[1]
			if site_b.distance_to(spawn_t) < site_a.distance_to(spawn_t):
				var swap := site_a
				site_a = site_b
				site_b = swap
			sim.set_sites(site_a.x, site_a.y, site_b.x, site_b.y)


static func _sort_by_y(a: Vector2, b: Vector2) -> bool:
	return a.y < b.y


static func registry() -> Dictionary:
	var data: Variant = JSON.parse_string(FileAccess.get_file_as_string(REGISTRY_PATH))
	if typeof(data) != TYPE_DICTIONARY:
		return {}
	return data


static func default_map_id() -> String:
	var id := str(registry().get("default", "de_ash"))
	return id if not id.is_empty() else "de_ash"


## map_id → boxes 路径；未注册返回空串。
static func boxes_path_for(map_id: String) -> String:
	for entry: Dictionary in registry().get("maps", []):
		if str(entry.get("id", "")) == map_id:
			return str(entry.get("boxes", ""))
	return ""


## map_id → glb 路径；未注册返回空串。
static func glb_path_for(map_id: String) -> String:
	for entry: Dictionary in registry().get("maps", []):
		if str(entry.get("id", "")) == map_id:
			return str(entry.get("glb", ""))
	return ""


static func _flatten_box(item: Dictionary) -> PackedFloat32Array:
	var center: Array = item["center"]
	var size: Array = item["size"]
	var gx := float(center[0])
	var gy := float(center[1])
	var gz := float(center[2])
	var sx := float(size[0])
	var sy := float(size[1])
	var sz := float(size[2])
	return PackedFloat32Array([
		-gz - sz * 0.5, -gx - sx * 0.5, gy - sy * 0.5,
		-gz + sz * 0.5, -gx + sx * 0.5, gy + sy * 0.5,
		_surface_id(str(item.get("material", "concrete"))),
		float(item.get("rot_y", 0.0)),
	])


static func _surface_id(material: String) -> int:
	match material:
		"metal":
			return 1
		"wood":
			return 2
		"glass":
			return 3
		"dirt":
			return 4
		"fabric":
			return 5
		_:
			return 0
