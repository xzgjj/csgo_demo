class_name MaterialLib
extends RefCounted
## 6 类原创 PBR 材质（混凝土/金属/木/玻璃/布料/泥土）+ 2 个高亮派生材质。
## 纯程序化 StandardMaterial3D（无外部贴图依赖），按 de_ash blockout 的 kind/prop 复用。
## 参数权威表见 assets/materials/README.md，修改时两处同步。

const BASE_IDS: Array[String] = ["concrete", "metal", "wood", "glass", "fabric", "dirt"]
const HIGHLIGHT_IDS: Array[String] = ["site", "spawn"]

static var _cache: Dictionary = {}


static func all_ids() -> Array[String]:
	var ids: Array[String] = []
	ids.append_array(BASE_IDS)
	ids.append_array(HIGHLIGHT_IDS)
	return ids


static func is_base_id(id: String) -> bool:
	return BASE_IDS.has(id)


static func get_material(id: String) -> StandardMaterial3D:
	if _cache.has(id):
		return _cache[id]
	var material := _create(id)
	_cache[id] = material
	return material


## blockout 盒子 kind → 材质。site/spawn 为高亮派生材质，墙壁/路面为混凝土。
static func material_for_kind(kind: String, size: Vector3 = Vector3.ZERO) -> StandardMaterial3D:
	match kind:
		"floor":
			return get_material("dirt")
		"lane":
			return get_material("concrete")
		"wall":
			return get_material("concrete")
		"site":
			return get_material("site")
		"spawn":
			return get_material("spawn")
		"prop":
			return get_material(prop_material_id(size))
		_:
			return get_material("concrete")


## prop 由几何特征推断材质类别（boxes.json 只有 size/color，无 layout 的 type 字段）：
## 灯具→金属、玻璃窗→玻璃、沙袋墙→布料、木箱→木、竖长件→金属、门板→木、其余→金属。
static func prop_material_id(size: Vector3) -> String:
	var axes: Array[float] = [size.x, size.y, size.z]
	axes.sort()
	var small := axes[0]
	var big := axes[2]
	var height := size.y
	var horizontal_min := minf(size.x, size.z)
	var horizontal_max := maxf(size.x, size.z)
	if big <= 24.0:
		return "metal"
	if small <= 16.0 and big >= 96.0:
		return "glass"
	if small >= 32.0 and big >= 3.5 * small:
		return "fabric"
	if big - small <= big * 0.1:
		return "wood"
	if height >= 1.6 * horizontal_min:
		return "metal"
	if height >= 0.7 * horizontal_max:
		return "wood"
	return "metal"


## layout prop type → 材质（de_ash.layout.json 的 type 字段；Godot 工程外，保留给后续数据接入）。
static func material_for_prop_type(prop_type: String) -> StandardMaterial3D:
	var key := prop_type.to_lower()
	if "crate" in key or "door" in key:
		return get_material("wood")
	if "glass" in key:
		return get_material("glass")
	if "sandbag" in key or "fabric" in key:
		return get_material("fabric")
	if "dirt" in key or "ground" in key:
		return get_material("dirt")
	if "metal" in key or "barrel" in key or "lamp" in key or "sign" in key:
		return get_material("metal")
	return get_material("concrete")


static func _create(id: String) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	match id:
		"concrete":
			_set_pbr(material, Color(0.58, 0.48, 0.36), 0.95, 0.0)
		"metal":
			_set_pbr(material, Color(0.52, 0.54, 0.58), 0.35, 0.90)
		"wood":
			_set_pbr(material, Color(0.42, 0.29, 0.17), 0.80, 0.0)
		"glass":
			_set_pbr(material, Color(0.70, 0.82, 0.88, 0.34), 0.06, 0.10)
			material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		"fabric":
			_set_pbr(material, Color(0.45, 0.40, 0.32), 1.0, 0.0)
		"dirt":
			_set_pbr(material, Color(0.42, 0.34, 0.24), 1.0, 0.0)
		"site":
			_set_pbr(material, Color(0.62, 0.22, 0.18), 0.88, 0.0)
			_set_emission(material, Color(0.85, 0.18, 0.12), 0.9)
		"spawn":
			_set_pbr(material, Color(0.20, 0.38, 0.62), 0.88, 0.0)
			_set_emission(material, Color(0.15, 0.40, 0.85), 0.9)
		_:
			_set_pbr(material, Color(0.30, 0.30, 0.30), 0.9, 0.0)
	return material


static func _set_pbr(material: StandardMaterial3D, color: Color, roughness: float,
		metallic: float) -> void:
	material.albedo_color = color
	material.roughness = roughness
	material.metallic = metallic


static func _set_emission(material: StandardMaterial3D, color: Color, energy: float) -> void:
	material.emission_enabled = true
	material.emission = color
	material.emission_energy_multiplier = energy
