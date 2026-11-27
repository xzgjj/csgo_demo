class_name MaterialLib
extends RefCounted
## 6 类原创 PBR 材质（混凝土/金属/木/玻璃/布料/泥土）+ 2 个高亮派生材质。
## 纯程序化 StandardMaterial3D（无外部贴图依赖），按 de_ash blockout 的 kind/prop 复用。
## 参数权威表见 assets/materials/README.md，修改时两处同步。

const BASE_IDS: Array[String] = ["concrete", "stone", "metal", "wood", "glass", "fabric", "dirt", "road"]
const HIGHLIGHT_IDS: Array[String] = ["site", "spawn"]
const TEXTURE_FILES := {
	"concrete": "res://textures/brick_wall.png",
	"wood": "res://textures/wood_grain.png",
	"metal": "res://textures/metal_scratch.png",
	"fabric": "res://textures/fabric_weave.png",
	"dirt": "res://textures/dirt_noise.png",
}
const TEXTURE_SCALE := 0.01
## 路面尺度单独放宽：1 张 = 256u（4664u 地面约 18 次平铺），避免车辙过密。
const TEXTURE_SCALE_BY_ID := {
	"road": 0.00390625,
	"stone": 0.00390625,
}
## 地图几何的世界尺度：1 张贴图 = 多少单位（1u≈2.54cm）。只有三平面才谈得上“贴图尺度”。
const MAP_TILE_UNITS := {
	"concrete": 128.0,
	"stone": 256.0,
	"wood": 96.0,
	"metal": 128.0,
	"glass": 128.0,
	"fabric": 96.0,
	"dirt": 256.0,
	"road": 256.0,
	"site": 256.0,
	"spawn": 256.0,
}
## 世界坐标三平面：盒子是等比缩放的立方体，局部三平面会把整张贴图压进 1 个像素。
## 角色/武器不适合世界三平面（贴图会随相机移动），因此它们继续走 get_material()。
const WORLD_TRIPLANAR_IDS := ["road", "stone"]
const DUST2_TEXTURE_FILES := {
	"concrete": "res://textures/dust2/plaster_sand_01.png",
	"stone": "res://textures/dust2/sandstone_block_01.png",
	"wood": "res://textures/dust2/wood_plank_01.png",
	"metal": "res://textures/dust2/metal_panel_01.png",
	"glass": "res://textures/dust2/glass_crack_01.png",
	"fabric": "res://textures/dust2/sandbag_01.png",
	"dirt": "res://textures/dust2/dirt_ground_01.png",
	"road": "res://textures/dust2/road_compacted_01.png",
}

static var _cache: Dictionary = {}
static var _map_cache: Dictionary = {}
static var _texture_files: Dictionary = TEXTURE_FILES


## 按图切换贴图集（de_ash 用程序化原版，de_dust2 用 assets/generated/dust2）。
static func use_texture_set(map_id: String) -> void:
	_texture_files = DUST2_TEXTURE_FILES if map_id == "de_dust2" else TEXTURE_FILES
	_cache.clear()
	_map_cache.clear()


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


## 地图几何专用：一律世界三平面 + 每类材质的固定 tile 尺度。
## 为什么单独一份：GLB/blockout 的盒子是「1×1×1 立方体缩放到 32×512×4664」，
## 局部 UV 只有 0..1，贴图被拉成一整片纯色——这就是截图里“什么都没有的大长方形”。
static func map_material(id: String) -> StandardMaterial3D:
	if _map_cache.has(id):
		return _map_cache[id]
	var material := _create(id, true)
	_map_cache[id] = material
	return material


## blockout 盒子 kind → 材质。site/spawn 为高亮派生材质，墙壁/路面为混凝土。
static func material_for_kind(kind: String, size: Vector3 = Vector3.ZERO) -> StandardMaterial3D:
	match kind:
		"floor":
			return map_material("dirt")
		"lane":
			return map_material("concrete")
		"road":
			return map_material("road")
		"wall":
			return map_material("concrete")
		"site":
			return get_material("site")
		"spawn":
			return get_material("spawn")
		"prop":
			return map_material(prop_material_id(size))
		_:
			return map_material("concrete")


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


## 把 GLB 的 mat_* 材质替换为带 6 贴图的 MaterialLib 材质（triplanar，无需 UV）。
## 游戏（de_ash_world）与 6 机位截图（capture_de_ash）共用，保证视觉一致。
static func apply_to_scene(node: Node) -> void:
	if node is MeshInstance3D:
		var mesh_instance := node as MeshInstance3D
		for surface in mesh_instance.get_surface_override_material_count():
			var current := mesh_instance.get_active_material(surface)
			if current is StandardMaterial3D and current.resource_name.begins_with("mat_"):
				var key := current.resource_name.trim_prefix("mat_")
				if is_base_id(key):
					mesh_instance.set_surface_override_material(surface, map_material(key))
	for child in node.get_children():
		apply_to_scene(child)


static func _create(id: String, world_triplanar: bool = false) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	match id:
		"concrete":
			_set_pbr(material, Color(0.58, 0.48, 0.36), 0.95, 0.0)
		"metal":
			_set_pbr(material, Color(0.60, 0.62, 0.66), 0.58, 0.20)
		"wood":
			_set_pbr(material, Color(0.42, 0.29, 0.17), 0.80, 0.0)
		"glass":
			_set_pbr(material, Color(0.70, 0.82, 0.88, 0.34), 0.06, 0.10)
			material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		"fabric":
			_set_pbr(material, Color(0.45, 0.40, 0.32), 1.0, 0.0)
		"dirt":
			_set_pbr(material, Color(0.42, 0.34, 0.24), 1.0, 0.0)
		"road":
			_set_pbr(material, Color(0.50, 0.44, 0.34), 0.92, 0.0)
		"stone":
			_set_pbr(material, Color(0.60, 0.53, 0.42), 0.90, 0.0)
		"site":
			_set_pbr(material, Color(0.62, 0.22, 0.18), 0.88, 0.0)
			_set_emission(material, Color(0.85, 0.18, 0.12), 0.22)
		"spawn":
			_set_pbr(material, Color(0.20, 0.38, 0.62), 0.88, 0.0)
			_set_emission(material, Color(0.15, 0.40, 0.85), 0.22)
		_:
			_set_pbr(material, Color(0.30, 0.30, 0.30), 0.9, 0.0)
	_apply_texture(material, id, world_triplanar)
	return material


static func _apply_texture(material: StandardMaterial3D, id: String,
		world_triplanar: bool = false) -> void:
	var path: String = _texture_files.get(id, "")
	if path.is_empty():
		return
	var image := Image.new()
	if image.load(ProjectSettings.globalize_path(path)) != OK:
		return
	image.generate_mipmaps()
	var texture := ImageTexture.create_from_image(image)
	material.albedo_texture = texture
	material.uv1_triplanar = true
	material.uv1_world_triplanar = world_triplanar
	var scale := float(TEXTURE_SCALE_BY_ID.get(id, TEXTURE_SCALE))
	if world_triplanar:
		scale = 1.0 / float(MAP_TILE_UNITS.get(id, 128.0))
	material.uv1_scale = Vector3(scale, scale, scale)
	material.texture_filter = BaseMaterial3D.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC


static func _set_pbr(material: StandardMaterial3D, color: Color, roughness: float,
		metallic: float) -> void:
	material.albedo_color = color
	material.roughness = roughness
	material.metallic = metallic


static func _set_emission(material: StandardMaterial3D, color: Color, energy: float) -> void:
	material.emission_enabled = true
	material.emission = color
	material.emission_energy_multiplier = energy
