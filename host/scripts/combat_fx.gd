class_name CombatFx
extends Node3D
## 战斗表现层：枪声 / 命中音 / 命中火花。只读内核计数，不参与规则；
## 回归测试通过 shot_count / hit_count / spark.emitting 断言（见 input_smoke.gd）。

var shot_count := 0
var hit_count := 0
var dryfire_count := 0
var spark: CPUParticles3D
var muzzle_flash: MeshInstance3D
var muzzle_light: OmniLight3D
var shell: MeshInstance3D
## 开镜/第三人称时关掉枪口焰（scoped 视角看不到枪，闪一下会破坏“贴脸光斑”的观感）。
var muzzle_visible_enabled := true
var _shot_player: AudioStreamPlayer
var _hit_player: AudioStreamPlayer
var _flash_life := 0.0
var _shell_life := 0.0
var _shell_velocity := Vector3.ZERO


func _ready() -> void:
	_shot_player = _make_player("res://audio/shot.wav", -6.0)
	_hit_player = _make_player("res://audio/hit.wav", -9.0)
	spark = CPUParticles3D.new()
	spark.name = "HitSpark"
	spark.amount = 12
	spark.lifetime = 0.3
	spark.one_shot = true
	spark.explosiveness = 0.95
	spark.direction = Vector3(0.0, 0.6, 0.0)
	spark.spread = 55.0
	spark.initial_velocity_min = 80.0
	spark.initial_velocity_max = 220.0
	spark.gravity = Vector3(0.0, -600.0, 0.0)
	spark.scale_amount_min = 0.6
	spark.scale_amount_max = 1.4
	var mesh := SphereMesh.new()
	mesh.radius = 2.0
	mesh.height = 4.0
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(1.0, 0.85, 0.45)
	material.emission_enabled = true
	material.emission = Color(1.0, 0.55, 0.1)
	mesh.material = material
	spark.mesh = mesh
	spark.emitting = false
	add_child(spark)


func _make_player(path: String, volume_db: float) -> AudioStreamPlayer:
	var player := AudioStreamPlayer.new()
	player.stream = load_wav(path)
	player.volume_db = volume_db
	add_child(player)
	return player


## 运行时解析 WAV（不依赖编辑器 import，源码工程/导出工程都可读）。
static func load_wav(path: String) -> AudioStreamWAV:
	var bytes := FileAccess.get_file_as_bytes(path)
	if bytes.size() < 44:
		bytes = FileAccess.get_file_as_bytes(ProjectSettings.globalize_path(path))
	var stream := AudioStreamWAV.new()
	if bytes.size() < 44:
		return stream
	var channels := 1
	var rate := 44100
	var bits := 16
	var data := PackedByteArray()
	var pos := 12
	while pos + 8 <= bytes.size():
		var chunk := bytes.slice(pos, pos + 4).get_string_from_ascii()
		var size := bytes.decode_u32(pos + 4)
		if chunk == "fmt ":
			channels = bytes.decode_u16(pos + 10)
			rate = bytes.decode_u32(pos + 12)
			bits = bytes.decode_u16(pos + 22)
		elif chunk == "data":
			data = bytes.slice(pos + 8, pos + 8 + size)
			break
		pos += 8 + size + (size & 1)
	stream.format = AudioStreamWAV.FORMAT_16_BITS if bits >= 16 else AudioStreamWAV.FORMAT_8_BITS
	stream.mix_rate = rate
	stream.stereo = channels == 2
	stream.data = data
	return stream


static func hit_point(sim: RefCounted, camera: Camera3D) -> Vector3:
	var eye: Vector3 = camera.position
	var dir: Vector3 = -camera.basis.z
	var best: Vector3 = eye + dir * 300.0
	var best_along := 1e9
	var count: int = int(sim.get_player_count())
	for index in range(1, count):
		if not bool(sim.is_alive(index)):
			continue
		var point := Vector3(-float(sim.get_y(index)), 36.0, -float(sim.get_x(index)))
		var to_target := point - eye
		var along := to_target.dot(dir)
		if along < 0.0 or along >= best_along:
			continue
		if (to_target - dir * along).length() < 70.0:
			best_along = along
			best = point
	return best


## 把枪口焰/弹壳挂到相机（第一人称，位置随视角）。main 在相机入树后调用。
## 枪口焰用程序化星芒贴图 + ADD 混合：4 瓣短火花 + 中心热核，40ms、随机 roll（两帧内一闪），
## 不再是一块 0.14m 的纯色方片；另配 1 帧 OmniLight 让枪口真的照亮周围墙面。
func attach(camera: Camera3D) -> void:
	var flash_mesh := QuadMesh.new()
	flash_mesh.size = Vector2(0.34, 0.34)
	var flash_material := StandardMaterial3D.new()
	flash_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	flash_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	flash_material.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	flash_material.albedo_texture = _flash_texture()
	flash_material.albedo_color = Color(1.0, 0.93, 0.78, 1.0)
	flash_material.emission_enabled = true
	flash_material.emission = Color(1.0, 0.72, 0.30)
	flash_material.emission_energy_multiplier = 2.2
	flash_material.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
	flash_material.no_depth_test = true
	flash_material.depth_draw_mode = BaseMaterial3D.DEPTH_DRAW_DISABLED
	flash_mesh.material = flash_material
	muzzle_flash = MeshInstance3D.new()
	muzzle_flash.name = "MuzzleFlash"
	muzzle_flash.mesh = flash_mesh
	# 对齐 viewmodel REST_POSITION(0.24,-0.26,-0.58) + 枪管末端（约 -0.31 × 0.85）。
	muzzle_flash.position = Vector3(0.26, -0.19, -0.84)
	muzzle_flash.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	muzzle_flash.visible = false
	camera.add_child(muzzle_flash)

	muzzle_light = OmniLight3D.new()
	muzzle_light.name = "MuzzleLight"
	muzzle_light.position = muzzle_flash.position
	muzzle_light.light_color = Color(1.0, 0.72, 0.35)
	muzzle_light.light_energy = 1.8
	muzzle_light.omni_range = 5.0
	muzzle_light.shadow_enabled = false
	muzzle_light.visible = false
	camera.add_child(muzzle_light)

	var shell_mesh := BoxMesh.new()
	shell_mesh.size = Vector3(0.008, 0.008, 0.024)
	var shell_material := StandardMaterial3D.new()
	shell_material.albedo_color = Color(0.78, 0.62, 0.22)
	shell_material.metallic = 0.8
	shell_mesh.material = shell_material
	shell = MeshInstance3D.new()
	shell.name = "Shell"
	shell.mesh = shell_mesh
	shell.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	shell.visible = false
	camera.add_child(shell)


func _process(delta: float) -> void:
	if _flash_life > 0.0:
		_flash_life = maxf(0.0, _flash_life - delta)
		var lit := _flash_life > 0.0
		if muzzle_flash != null:
			muzzle_flash.visible = lit
		if muzzle_light != null:
			muzzle_light.visible = lit
	if _shell_life > 0.0:
		_shell_life = maxf(0.0, _shell_life - delta)
		_shell_velocity.y -= 260.0 * delta
		if shell != null:
			shell.position += _shell_velocity * delta
			shell.rotation_degrees += Vector3(180.0, 90.0, 0.0) * delta
			shell.visible = _shell_life > 0.0


## 开火：枪声 + 枪口焰 + 抛壳（BAL-06；只由 shot_feedback 在"真的开火"后调用）。
func trigger_shot() -> void:
	shot_count += 1
	if _shot_player.stream != null:
		_shot_player.play()
	if muzzle_flash != null and muzzle_visible_enabled:
		# 每次开火随机 roll + 10% 尺寸抖动，连发时不会像一张静止贴图在闪。
		muzzle_flash.rotation.z = randf() * TAU
		muzzle_flash.scale = Vector3.ONE * randf_range(0.85, 1.15)
		muzzle_flash.visible = true
		_flash_life = 0.04
		if muzzle_light != null:
			muzzle_light.visible = true
			muzzle_light.light_energy = randf_range(1.6, 2.2)
	if shell != null:
		shell.position = Vector3(0.12, -0.12, -0.34)
		_shell_velocity = Vector3(0.55, 0.35, 0.15)
		_shell_life = 0.4
		shell.visible = true


## 程序化枪口焰贴图：中心热核 + 4 瓣短星芒（64×64），避免依赖编辑器导入资源。
static func _flash_texture() -> ImageTexture:
	var size := 64
	var image := Image.create(size, size, false, Image.FORMAT_RGBA8)
	var half := float(size) * 0.5
	for y in size:
		for x in size:
			var u := (float(x) + 0.5 - half) / half
			var v := (float(y) + 0.5 - half) / half
			var radius := sqrt(u * u + v * v)
			if radius >= 1.0:
				image.set_pixel(x, y, Color(0, 0, 0, 0))
				continue
			var angle := atan2(v, u)
			var core := pow(maxf(0.0, 1.0 - radius), 3.4)
			var spikes := pow(maxf(0.0, cos(angle * 4.0)), 14.0) * pow(maxf(0.0, 1.0 - radius), 1.6)
			var alpha := clampf(core * 1.15 + spikes * 1.25, 0.0, 1.0)
			var warmth := clampf(core * 1.2 + spikes * 0.4, 0.0, 1.0)
			image.set_pixel(x, y, Color(1.0, 0.86 + 0.10 * warmth, 0.55 + 0.30 * warmth, alpha))
	return ImageTexture.create_from_image(image)


## 空弹/冷却被内核拒绝时只记一次"空扣扳机"，不播枪声、不给后坐（防止"没子弹还能开枪"的假反馈）。
func trigger_dryfire() -> void:
	dryfire_count += 1


func trigger_hit(world_position: Vector3) -> void:
	hit_count += 1
	if _hit_player.stream != null:
		_hit_player.play()
	spark.global_position = world_position
	spark.restart()
	spark.emitting = true
