class_name CombatFx
extends Node3D
## 战斗表现层：枪声 / 命中音 / 命中火花。只读内核计数，不参与规则；
## 回归测试通过 shot_count / hit_count / spark.emitting 断言（见 input_smoke.gd）。

var shot_count := 0
var hit_count := 0
var spark: CPUParticles3D
var _shot_player: AudioStreamPlayer
var _hit_player: AudioStreamPlayer


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


func trigger_shot() -> void:
	shot_count += 1
	if _shot_player.stream != null:
		_shot_player.play()


func trigger_hit(world_position: Vector3) -> void:
	hit_count += 1
	if _hit_player.stream != null:
		_hit_player.play()
	spark.global_position = world_position
	spark.restart()
	spark.emitting = true
