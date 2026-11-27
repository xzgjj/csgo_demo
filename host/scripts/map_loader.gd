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
## 返回 -1 表示“清单被内核拒绝”（非法盒/超容量），调用方必须拒绝开局而不是继续。
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
	var hp := PackedInt32Array()
	for item: Dictionary in data.get("boxes", []):
		if not bool(item.get("collision", true)):
			continue
		flat.append_array(_flatten_box(item))
		hp.append(int(item.get("hp", 0)))
	var expected := flat.size() / 8
	var written: int = sim.set_map_boxes(flat)
	sim.set_map_wall_hp(hp)
	_apply_frame(sim, data)
	var rejected: int = sim.get_map_rejected()
	if rejected > 0:
		push_error("地图盒被拒绝 %d 个（首个错误码 %d，检查尺寸/旋转/材质/容量）" %
				[rejected, sim.get_map_first_error()])
		return -1
	if written != expected:
		push_error("碰撞盒注入不完整：期望 %d，实际 %d（检查尺寸/旋转/材质/容量）" % [expected, written])
		return -1
	return written


## layout bounds/spawns 注入；缺字段时保持内核默认（de_ash ±2048）。
static func _apply_frame(sim: RefCounted, data: Dictionary) -> void:
	var bounds: Dictionary = data.get("bounds", {})
	var spawns: Dictionary = data.get("spawns", {})
	if bounds.is_empty() or spawns.is_empty():
		return
	var lo: Array = bounds.get("min", [])
	var hi: Array = bounds.get("max", [])
	var t: Dictionary = spawns.get("T", {})
	var ct: Dictionary = spawns.get("CT", {})
	var tc: Array = t.get("center", [])
	var cc: Array = ct.get("center", [])
	if lo.size() < 2 or hi.size() < 2 or tc.size() < 2 or cc.size() < 2:
		return
	sim.set_map_frame(float(lo[0]), float(lo[1]), float(hi[0]), float(hi[1]),
			float(tc[0]), float(tc[1]), 0.0, float(cc[0]), float(cc[1]), 0.0)


## 命令行地图（`--map=<id>`，Godot 用户参数）；未指定时返回注册表默认图。
static func cli_map_id() -> String:
	for arg in OS.get_cmdline_user_args():
		var text := str(arg)
		if text.begins_with("--map="):
			var id := text.trim_prefix("--map=").strip_edges()
			if not id.is_empty():
				return id
	return default_map_id()


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


## 开局前装图：注入盒/HP/bounds/出生 + 包点，再 begin_match；失败返回 false（调用方拒绝开局）。
static func start_match(sim: RefCounted, map_id: String) -> bool:
	var written := apply_map(sim, map_id)
	if written < 0:
		push_error("地图 %s 注入失败，拒绝开局（见计数/错误码）" % map_id)
		return false
	var sites := site_centers(map_id)
	if sites.size() >= 2:
		sim.set_sites(sites[0].x, sites[0].y, sites[1].x, sites[1].y)
	else:
		sim.set_sites(-1200.0, -900.0, 1200.0, 900.0)
	sim.begin_match()
	return true


## 包点中心（sim 坐标）；按 boxes 的 kind=site 顺序返回，不足 2 个返回空。
static func site_centers(map_id: String) -> Array[Vector2]:
	var result: Array[Vector2] = []
	var path := boxes_path_for(map_id)
	if path.is_empty():
		return result
	var text := FileAccess.get_file_as_string(path)
	var data: Variant = JSON.parse_string(text)
	if typeof(data) != TYPE_DICTIONARY:
		return result
	for item: Dictionary in data.get("boxes", []):
		if str(item.get("kind", "")) != "site":
			continue
		var center: Array = item.get("center", [])
		if center.size() < 3:
			continue
		result.append(Vector2(-float(center[2]), -float(center[0])))
	return result


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
		"road":
			# 路面按泥土判定穿透/弹孔，见 assets/materials/README.md。
			return 4
		"fabric":
			return 5
		_:
			return 0
