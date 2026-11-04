class_name WeaponViewmodel
extends Node3D
## 第一人称武器 viewmodel：优先运行时加载 Blender 程序化生成的 GLB
## （AK-47 / M4A1 / AWP，见 tools/blender/build_weapons.py），加载失败时回退到
## 盒体组合轮廓；开火后以轻微位移+俯仰回落表现后坐力。
## 纯表现层：不读也不写规则状态，武器切换是展示用的本地选择（内核暂无切换 API）。
##
## GLB 坐标约定：+X 右 / +Y 上 / -Z 枪口，单位米，原点在机匣/握把附近，
## 与 Godot 相机空间一致，因此挂到 _pivot 下不需要额外旋转。

const WEAPON_IDS: Array[String] = ["ak", "m4", "awp"]
const DISPLAY_NAMES := {"ak": "AK-47", "m4": "M4A1", "awp": "AWP"}
const MODEL_PATHS := {
	"ak": "res://models/weapons/ak47.glb",
	"m4": "res://models/weapons/m4a4.glb",
	"awp": "res://models/weapons/awp.glb",
}
const MODEL_ROOT_NAMES := {"ak": "ak47", "m4": "m4a4", "awp": "awp"}
const REST_POSITION := Vector3(0.24, -0.26, -0.58)
const REST_ROTATION := Vector3(0.0, -2.0, 0.0)
const VIEWMODEL_SCALE := 0.85

## 每把枪的盒体表（GLB 加载失败时的回退路径）：
## [尺寸, 相对偏移(枪口朝 -Z), 材质键, 旋转角度]。偏移 0 为机匣中心。
const PART_LAYOUTS := {
	"ak": [
		[Vector3(0.060, 0.070, 0.340), Vector3(0, 0, 0), "metal", Vector3.ZERO],
		[Vector3(0.055, 0.020, 0.200), Vector3(0, 0.045, -0.04), "metal", Vector3.ZERO],
		[Vector3(0.026, 0.026, 0.260), Vector3(0, 0.012, -0.31), "metal", Vector3.ZERO],
		[Vector3(0.020, 0.020, 0.140), Vector3(0, 0.050, -0.20), "metal", Vector3.ZERO],
		[Vector3(0.052, 0.052, 0.200), Vector3(0, 0.008, -0.22), "wood", Vector3.ZERO],
		[Vector3(0.038, 0.030, 0.160), Vector3(0, 0.048, -0.22), "wood", Vector3.ZERO],
		[Vector3(0.048, 0.085, 0.240), Vector3(0, -0.015, 0.26), "wood", Vector3(-6, 0, 0)],
		[Vector3(0.038, 0.100, 0.050), Vector3(0, -0.075, 0.10), "polymer", Vector3(-14, 0, 0)],
		[Vector3(0.045, 0.075, 0.090), Vector3(0, -0.075, -0.02), "metal", Vector3(10, 0, 0)],
		[Vector3(0.045, 0.075, 0.090), Vector3(0, -0.135, 0.005), "metal", Vector3(22, 0, 0)],
		[Vector3(0.045, 0.070, 0.080), Vector3(0, -0.185, 0.045), "metal", Vector3(34, 0, 0)],
		[Vector3(0.014, 0.045, 0.014), Vector3(0, 0.055, -0.43), "metal", Vector3.ZERO],
		[Vector3(0.045, 0.010, 0.020), Vector3(0, 0.078, -0.43), "metal", Vector3.ZERO],
		[Vector3(0.035, 0.014, 0.020), Vector3(0, 0.052, 0.02), "metal", Vector3.ZERO],
		[Vector3(0.036, 0.036, 0.045), Vector3(0, 0.012, -0.455), "metal", Vector3.ZERO],
	],
	"m4": [
		[Vector3(0.055, 0.065, 0.300), Vector3(0, 0, 0), "metal", Vector3.ZERO],
		[Vector3(0.030, 0.018, 0.260), Vector3(0, 0.045, -0.03), "metal", Vector3.ZERO],
		[Vector3(0.024, 0.030, 0.100), Vector3(0, 0.065, 0.02), "metal", Vector3.ZERO],
		[Vector3(0.022, 0.022, 0.280), Vector3(0, 0.005, -0.30), "metal", Vector3.ZERO],
		[Vector3(0.058, 0.058, 0.200), Vector3(0, 0.005, -0.20), "polymer", Vector3.ZERO],
		[Vector3(0.045, 0.075, 0.200), Vector3(0, -0.010, 0.24), "polymer", Vector3.ZERO],
		[Vector3(0.032, 0.032, 0.090), Vector3(0, 0.005, 0.14), "metal", Vector3.ZERO],
		[Vector3(0.045, 0.150, 0.060), Vector3(0, -0.100, -0.02), "polymer", Vector3(6, 0, 0)],
		[Vector3(0.040, 0.090, 0.045), Vector3(0, -0.070, 0.10), "polymer", Vector3(-12, 0, 0)],
		[Vector3(0.020, 0.050, 0.020), Vector3(0, 0.045, -0.40), "metal", Vector3.ZERO],
		[Vector3(0.030, 0.012, 0.020), Vector3(0, 0.058, 0.02), "metal", Vector3.ZERO],
		[Vector3(0.030, 0.030, 0.040), Vector3(0, 0.005, -0.45), "metal", Vector3.ZERO],
	],
	"awp": [
		[Vector3(0.032, 0.032, 0.520), Vector3(0, 0.012, -0.42), "metal", Vector3.ZERO],
		[Vector3(0.065, 0.075, 0.300), Vector3(0, 0, 0.02), "metal", Vector3.ZERO],
		[Vector3(0.050, 0.050, 0.260), Vector3(0, 0.105, -0.02), "metal", Vector3.ZERO],
		[Vector3(0.055, 0.055, 0.012), Vector3(0, 0.105, -0.155), "glass", Vector3.ZERO],
		[Vector3(0.050, 0.050, 0.012), Vector3(0, 0.105, 0.115), "glass", Vector3.ZERO],
		[Vector3(0.022, 0.035, 0.020), Vector3(0, 0.068, -0.10), "metal", Vector3.ZERO],
		[Vector3(0.022, 0.035, 0.020), Vector3(0, 0.068, 0.06), "metal", Vector3.ZERO],
		[Vector3(0.075, 0.018, 0.018), Vector3(0.062, 0.020, 0.055), "metal", Vector3.ZERO],
		[Vector3(0.052, 0.100, 0.300), Vector3(0, -0.012, 0.36), "polymer", Vector3.ZERO],
		[Vector3(0.040, 0.030, 0.120), Vector3(0, 0.050, 0.31), "polymer", Vector3.ZERO],
		[Vector3(0.040, 0.100, 0.050), Vector3(0, -0.085, 0.14), "polymer", Vector3(-10, 0, 0)],
		[Vector3(0.050, 0.080, 0.110), Vector3(0, -0.085, -0.01), "metal", Vector3.ZERO],
		[Vector3(0.038, 0.038, 0.050), Vector3(0, 0.012, -0.70), "metal", Vector3.ZERO],
	],
}

var current_id := "ak"
var _pivot: Node3D
var _models: Dictionary = {}
var _kick := 0.0
var _metal: StandardMaterial3D
var _wood: StandardMaterial3D
var _polymer: StandardMaterial3D


## 由 main.gd 在相机加入场景后调用；建立 viewmodel 层级并选中默认武器。
func attach(camera: Camera3D) -> void:
	_metal = MaterialLib.get_material("metal")
	_wood = MaterialLib.get_material("wood")
	_polymer = _make_polymer()
	_pivot = Node3D.new()
	_pivot.name = "WeaponPivot"
	_pivot.position = REST_POSITION
	_pivot.scale = Vector3.ONE * VIEWMODEL_SCALE
	camera.add_child(_pivot)
	for weapon_id in WEAPON_IDS:
		var model := _build_model(weapon_id)
		model.visible = false
		_pivot.add_child(model)
		_models[weapon_id] = model
	select_weapon(current_id)


func select_weapon(weapon_id: String) -> void:
	if not _models.has(weapon_id):
		return
	current_id = weapon_id
	for key in _models:
		var model: Node3D = _models[key]
		model.visible = key == weapon_id


func display_name() -> String:
	return str(DISPLAY_NAMES.get(current_id, current_id))


func weapon_ids() -> Array[String]:
	return WEAPON_IDS


## 开火后坐力：单次冲量，_process 内指数回落。
func kick(strength: float = 1.0) -> void:
	_kick = minf(1.0, _kick + 0.6 * strength)


func _process(delta: float) -> void:
	if _pivot == null:
		return
	_kick = maxf(0.0, _kick - delta * 7.0)
	var amount := _kick * _kick
	_pivot.position = REST_POSITION + Vector3(0.0, 0.010 * amount, 0.075 * amount)
	_pivot.rotation_degrees = REST_ROTATION + Vector3(7.0 * amount, 1.8 * amount, 0.0)


## 优先 GLB（Blender 程序化生成），失败回退盒体组合。
func _build_model(weapon_id: String) -> Node3D:
	var glb_model := _load_glb_model(weapon_id)
	if glb_model != null:
		return glb_model
	return _build_box_model(weapon_id)


## 运行时用 GLTFDocument 读取 res://models/weapons/*.glb；任何失败返回 null 由调用方回退。
func _load_glb_model(weapon_id: String) -> Node3D:
	var path := str(MODEL_PATHS.get(weapon_id, ""))
	if path.is_empty():
		push_warning("WeaponViewmodel: 未配置 GLB 路径（%s），回退盒体模型" % weapon_id)
		return null
	var document := GLTFDocument.new()
	var state := GLTFState.new()
	var error := document.append_from_file(path, state)
	if error != OK:
		# 源码工程直接读 res:// 文件；导出工程里回退为绝对路径再试一次。
		error = document.append_from_file(ProjectSettings.globalize_path(path), state)
	if error != OK:
		push_warning("WeaponViewmodel: GLB 加载失败 %s（错误 %d），回退盒体模型" % [path, error])
		return null
	var generated := document.generate_scene(state)
	if not generated is Node3D:
		push_warning("WeaponViewmodel: GLB 场景无效 %s，回退盒体模型" % path)
		return null
	var model := generated as Node3D
	model.name = str(MODEL_ROOT_NAMES.get(weapon_id, weapon_id))
	for mesh_instance in _mesh_instances(model):
		mesh_instance.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	return model


func _mesh_instances(node: Node) -> Array[MeshInstance3D]:
	var instances: Array[MeshInstance3D] = []
	for child in node.get_children():
		if child is MeshInstance3D:
			instances.append(child as MeshInstance3D)
		instances.append_array(_mesh_instances(child))
	return instances


func _build_box_model(weapon_id: String) -> Node3D:
	var model := Node3D.new()
	model.name = "Weapon_" + weapon_id
	for part: Array in PART_LAYOUTS.get(weapon_id, []):
		_add_part(model, part[0], part[1], str(part[2]), part[3])
	return model


func _add_part(model: Node3D, size: Vector3, offset: Vector3, material_key: String,
		rotation: Vector3) -> void:
	var mesh_instance := MeshInstance3D.new()
	var box_mesh := BoxMesh.new()
	box_mesh.size = size
	box_mesh.material = _material_for(material_key)
	mesh_instance.mesh = box_mesh
	mesh_instance.position = offset
	mesh_instance.rotation_degrees = rotation
	mesh_instance.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	model.add_child(mesh_instance)


func _material_for(material_key: String) -> StandardMaterial3D:
	match material_key:
		"wood":
			return _wood
		"polymer":
			return _polymer
		"glass":
			return MaterialLib.get_material("glass")
		_:
			return _metal


func _make_polymer() -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(0.09, 0.10, 0.11)
	material.roughness = 0.55
	material.metallic = 0.10
	return material
