class_name SettingsStore
extends RefCounted
## 用户设置持久化：FOV / 灵敏度 / 主音量（音量目前只有占位值，音频总线会跟随）。
## 启动时 load_settings() 读取 user://settings.cfg；改动后 save_settings() 写回。
## 配置来自磁盘，属不可信输入：读入值一律做类型判断 + 范围钳制。

const DEFAULT_PATH := "user://settings.cfg"
const CONFIG_VERSION := 1
const SECTION := "player"

const FOV_MIN := 70.0
const FOV_MAX := 110.0
const SENSITIVITY_MIN := 0.02
const SENSITIVITY_MAX := 0.2
const VOLUME_MIN := 0
const VOLUME_MAX := 100

var path := DEFAULT_PATH
var fov := 90.0
var sensitivity := 0.08
var volume := 80


func _init(settings_path: String = DEFAULT_PATH) -> void:
	path = settings_path


func load_settings() -> bool:
	var config := ConfigFile.new()
	if config.load(path) != OK:
		return false
	_absorb(config)
	return true


func save_settings() -> bool:
	var config := ConfigFile.new()
	config.set_value(SECTION, "version", CONFIG_VERSION)
	config.set_value(SECTION, "fov", fov)
	config.set_value(SECTION, "sensitivity", sensitivity)
	config.set_value(SECTION, "volume", volume)
	return config.save(path) == OK


## 从已加载的 ConfigFile 吸收数值（供 smoke 注入非法配置验证钳制）。
func load_from(config: ConfigFile) -> void:
	_absorb(config)


func reset_defaults() -> void:
	fov = 90.0
	sensitivity = 0.08
	volume = 80


func apply_camera(camera: Camera3D) -> void:
	if camera != null:
		camera.fov = fov


func apply_audio() -> void:
	if volume <= VOLUME_MIN:
		AudioServer.set_bus_volume_db(0, -80.0)
		return
	AudioServer.set_bus_volume_db(0, linear_to_db(float(volume) / float(VOLUME_MAX)))


func _absorb(config: ConfigFile) -> void:
	fov = clampf(_number(config, "fov", fov), FOV_MIN, FOV_MAX)
	sensitivity = clampf(_number(config, "sensitivity", sensitivity),
			SENSITIVITY_MIN, SENSITIVITY_MAX)
	volume = clampi(int(_number(config, "volume", float(volume))), VOLUME_MIN, VOLUME_MAX)


## 只接受数字；字符串/向量等异常值回退到当前值。
func _number(config: ConfigFile, key: String, fallback: float) -> float:
	var raw: Variant = config.get_value(SECTION, key, fallback)
	if raw is int or raw is float:
		return float(raw)
	return fallback
