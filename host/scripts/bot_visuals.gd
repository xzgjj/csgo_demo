class_name BotVisuals
extends Node3D
## 模块 5/6 表现层：bot 持枪 / 枪口闪光 / 后坐力 / 朝向；死亡倒地；地面掉落武器 + E 拾取提示。
## 只读内核状态（weapon_id / view_yaw / fire_count / ground_weapon_*），不写规则状态。
## 截图/headless 确定性：`animating=false` 时不由 _process 推进，由调用方 `advance(seconds)` 显式步进。

const WEAPON_GLB := {
	1: "res://models/weapons/ak47.glb",
	2: "res://models/weapons/m4a4.glb",
	3: "res://models/weapons/awp.glb",
}
const WEAPON_NAMES := {1: "AK-47", 2: "M4A4", 3: "AWP", 4: "USP-S", 5: "Glock-18"}
const WEAPON_SCALE := 42.0
const HAND_OFFSET := Vector3(13.0, 42.0, -8.0)
const HAND_AIM_TILT := Vector3(0.0, 0.0, 0.0)
const MUZZLE_OFFSET := Vector3(0.0, 0.0, -34.0)
const FLASH_SECONDS := 0.07
const DEATH_SECONDS := 0.8
const DEATH_ROLL_DEGREES := 90.0
const DEATH_SINK := 20.0
const PICKUP_RADIUS_HINT := 64.0

var animating := true
var bot_nodes: Array[Node3D] = []
var ground_nodes: Array[Node3D] = []
var prompt_layer: CanvasLayer
var prompt: Label

var _prototypes: Dictionary = {}
var _holder_by_bot: Dictionary = {}
var _flash_by_bot: Dictionary = {}
var _recoil_by_bot: Dictionary = {}
var _flash_seconds: Dictionary = {}
var _death_seconds: Dictionary = {}
var _alive_prev: Dictionary = {}
var _fire_prev: Dictionary = {}
var _weapon_ids: Dictionary = {}
var _prompt_text := ""


func _ready() -> void:
	_build_prompt()


func _process(delta: float) -> void:
	if animating:
		_animate(delta)


## 每帧/每步从内核同步：位置、朝向、武器、开火闪光、存活→倒地。
func sync(sim: RefCounted) -> void:
	if sim == null:
		return
	_ensure_bots(sim)
	var count: int = int(sim.get_player_count())
	for index in range(1, count):
		_sync_bot(sim, index, bot_nodes[index - 1])
	_sync_ground(sim)
	_sync_prompt(sim)


## 确定性推进动画（截图/测试用；animating=false 时由调用方驱动）。
func advance(seconds: float) -> void:
	_animate(seconds)


func set_animating(value: bool) -> void:
	animating = value


func flash_visible(index: int) -> bool:
	var flash: Node3D = _flash_by_bot.get(index, null)
	return flash != null and flash.visible


func recoil_amount(index: int) -> float:
	return float(_recoil_by_bot.get(index, 0.0))


func weapon_node(index: int) -> Node3D:
	return _holder_by_bot.get(index, null)


func has_weapon_model(index: int) -> bool:
	var holder: Node3D = _holder_by_bot.get(index, null)
	return holder != null and holder.get_child_count() > 0


func ground_count() -> int:
	return ground_nodes.size()


func prompt_visible() -> bool:
	return prompt != null and prompt.visible


func prompt_text() -> String:
	return _prompt_text


func _ensure_bots(sim: RefCounted) -> void:
	var count: int = int(sim.get_player_count())
	while bot_nodes.size() < count - 1:
		var index := bot_nodes.size() + 1
		var team: int = int(sim.get_team(index))
		var bot := ActorVisuals.make_humanoid_bot(self, index, team)
		_attach_weapon(bot, index)
		bot_nodes.append(bot)
		_alive_prev[index] = true
		_fire_prev[index] = 0
		_death_seconds[index] = 0.0


func _attach_weapon(bot: Node3D, index: int) -> void:
	var holder := Node3D.new()
	holder.name = "WeaponHolder"
	holder.position = HAND_OFFSET
	holder.rotation_degrees = HAND_AIM_TILT
	bot.add_child(holder)
	var flash := MeshInstance3D.new()
	flash.name = "MuzzleFlash"
	var sphere := SphereMesh.new()
	sphere.radius = 5.0
	sphere.height = 10.0
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(1.0, 0.78, 0.35)
	material.emission_enabled = true
	material.emission = Color(1.0, 0.55, 0.12)
	material.emission_energy_multiplier = 3.0
	sphere.material = material
	flash.mesh = sphere
	flash.position = HAND_OFFSET + MUZZLE_OFFSET
	flash.visible = false
	bot.add_child(flash)
	_holder_by_bot[index] = holder
	_flash_by_bot[index] = flash
	_recoil_by_bot[index] = 0.0
	_flash_seconds[index] = 0.0


func _sync_bot(sim: RefCounted, index: int, bot: Node3D) -> void:
	var alive := bool(sim.is_alive(index))
	var pos := Vector3(-float(sim.get_y(index)), 0.0, -float(sim.get_x(index)))
	var yaw := float(sim.get_view_yaw(index))
	var weapon_id := int(sim.get_weapon_id(index))
	_ensure_weapon_model(index, weapon_id)
	bot.rotation.y = deg_to_rad(yaw)
	var was_alive := bool(_alive_prev.get(index, true))
	if alive and not was_alive:
		_death_seconds[index] = 0.0
		bot.rotation.z = 0.0
		bot.position.y = 0.0
	elif not alive and was_alive:
		_death_seconds[index] = 0.0001  # 开始倒地动画
	_alive_prev[index] = alive
	if alive:
		bot.position = pos
		bot.visible = true
	var fire_count := int(sim.get_fire_count(index))
	if fire_count > int(_fire_prev.get(index, 0)):
		_flash_seconds[index] = FLASH_SECONDS
		_recoil_by_bot[index] = 1.0
	_fire_prev[index] = fire_count


func _ensure_weapon_model(index: int, weapon_id: int) -> void:
	if int(_weapon_ids.get(index, -1)) == weapon_id:
		return
	var holder: Node3D = _holder_by_bot.get(index, null)
	if holder == null:
		return
	for child in holder.get_children():
		child.queue_free()
	var model := _weapon_model(weapon_id)
	if model != null:
		holder.add_child(model)
	_weapon_ids[index] = weapon_id


func _weapon_model(weapon_id: int) -> Node3D:
	var path := str(WEAPON_GLB.get(weapon_id, ""))
	if path.is_empty():
		return null
	if not _prototypes.has(weapon_id):
		_prototypes[weapon_id] = _load_glb(path)
	var prototype: Node3D = _prototypes.get(weapon_id, null)
	if prototype == null:
		return null
	var copy := prototype.duplicate() as Node3D
	copy.scale = Vector3.ONE * WEAPON_SCALE
	return copy


func _load_glb(path: String) -> Node3D:
	var document := GLTFDocument.new()
	var state := GLTFState.new()
	var error := document.append_from_file(path, state)
	if error != OK:
		error = document.append_from_file(ProjectSettings.globalize_path(path), state)
	if error != OK:
		return null
	var generated := document.generate_scene(state)
	if generated is Node3D:
		return generated as Node3D
	return null


func _animate(delta: float) -> void:
	for index in _flash_by_bot.keys():
		if float(_flash_seconds.get(index, 0.0)) > 0.0:
			_flash_seconds[index] = maxf(0.0, float(_flash_seconds[index]) - delta)
		var flash: Node3D = _flash_by_bot[index]
		if flash != null:
			flash.visible = float(_flash_seconds[index]) > 0.0
		var recoil := maxf(0.0, float(_recoil_by_bot.get(index, 0.0)) - delta * 6.0)
		_recoil_by_bot[index] = recoil
		var holder: Node3D = _holder_by_bot.get(index, null)
		if holder != null:
			holder.position = HAND_OFFSET + Vector3(0.0, 0.0, 5.0 * recoil)
	for index in bot_nodes.size():
		var bot: Node3D = bot_nodes[index]
		var death := float(_death_seconds.get(index + 1, 0.0))
		if death <= 0.0:
			continue
		death = minf(DEATH_SECONDS, death + delta)
		_death_seconds[index + 1] = death
		var progress := clampf(death / DEATH_SECONDS, 0.0, 1.0)
		bot.rotation.z = deg_to_rad(DEATH_ROLL_DEGREES * progress)
		bot.position.y = -DEATH_SINK * progress
		bot.visible = death < DEATH_SECONDS


func _sync_ground(sim: RefCounted) -> void:
	var count: int = int(sim.get_ground_weapon_count())
	while ground_nodes.size() < count:
		var node := Node3D.new()
		node.name = "GroundWeapon%d" % ground_nodes.size()
		add_child(node)
		ground_nodes.append(node)
	for index in ground_nodes.size():
		var node: Node3D = ground_nodes[index]
		var active := index < count
		node.visible = active
		if not active:
			continue
		node.position = Vector3(-float(sim.get_ground_weapon_y(index)), 6.0,
				-float(sim.get_ground_weapon_x(index)))
		node.rotation_degrees = Vector3(0.0, 90.0, -90.0)
		var weapon_id := int(sim.get_ground_weapon_id(index))
		if int(node.get_meta("weapon_id", -1)) != weapon_id:
			for child in node.get_children():
				child.queue_free()
			var model := _weapon_model(weapon_id)
			if model != null:
				node.add_child(model)
			node.set_meta("weapon_id", weapon_id)


func _sync_prompt(sim: RefCounted) -> void:
	var nearest := -1
	if sim.has_method("get_nearest_ground_weapon"):
		nearest = int(sim.get_nearest_ground_weapon(0))
	var text := ""
	if nearest >= 0:
		var weapon_id := int(sim.get_ground_weapon_id(nearest))
		text = "E 拾取 · %s" % str(WEAPON_NAMES.get(weapon_id, "武器"))
	_prompt_text = text
	if prompt != null:
		prompt.text = text
		prompt.visible = not text.is_empty() and bool(sim.is_alive(0))


func _build_prompt() -> void:
	prompt_layer = CanvasLayer.new()
	prompt_layer.name = "PickupPromptLayer"
	prompt_layer.layer = 2
	add_child(prompt_layer)
	prompt = Label.new()
	prompt.name = "PickupPrompt"
	prompt.add_theme_font_size_override("font_size", 22)
	prompt.add_theme_color_override("font_color", Color(1.0, 0.92, 0.55))
	prompt.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	prompt.size = Vector2(520.0, 28.0)
	prompt.position = Vector2(380.0, 520.0)
	prompt.visible = false
	prompt_layer.add_child(prompt)
