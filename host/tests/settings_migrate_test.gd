extends SceneTree
## SET-03 迁移：v1 [player] → v2（fov/sens/volume 不丢），未知键进 [legacy]，再写回仍可读。
## 用法：& godot --headless --path host --script res://tests/settings_migrate_test.gd

const StoreScript = preload("res://scripts/settings_store.gd")
const V1_PATH := "user://settings_migrate_v1.cfg"
const V2_PATH := "user://settings_migrate_v2.cfg"

var failures: Array[String] = []


func _initialize() -> void:
	_write_v1()
	var store = StoreScript.new(V1_PATH)
	_expect(store.load_settings(), "v1 配置应能读取")
	_expect(store.loaded_version() == 1, "应识别为 v1")
	_expect(is_equal_approx(float(store.get_value("camera.fov")), 75.0), "fov 未迁移")
	_expect(is_equal_approx(float(store.get_value("controls.mouse.sensitivity")), 0.05),
			"sensitivity 未迁移")
	_expect(int(store.get_value("audio.master.volume")) == 30, "volume 未迁移")
	_expect(str(store.legacy_values().get("player.unknown_key", "")) == "keep-me",
			"未知键应保留到 legacy")
	store.path = V2_PATH
	_expect(store.save_settings(), "v2 应写入成功")
	var reloaded = StoreScript.new(V2_PATH)
	_expect(reloaded.load_settings(), "v2 应能读回")
	_expect(reloaded.loaded_version() == 2, "新文件版本应为 2")
	_expect(is_equal_approx(float(reloaded.get_value("camera.fov")), 75.0), "v2 fov 漂移")
	_expect(is_equal_approx(float(reloaded.get_value("controls.mouse.sensitivity")), 0.05),
			"v2 sensitivity 漂移")
	_expect(str(reloaded.legacy_values().get("player.unknown_key", "")) == "keep-me",
			"v2 仍应保留 legacy 未知键")
	var meta := ConfigFile.new()
	meta.load(V2_PATH)
	_expect(int(meta.get_value("meta", "version", 0)) == 2, "cfg 应写 [meta] version=2")
	_cleanup()
	_finish()


func _write_v1() -> void:
	var config := ConfigFile.new()
	config.set_value("player", "version", 1)
	config.set_value("player", "fov", 75.0)
	config.set_value("player", "sensitivity", 0.05)
	config.set_value("player", "volume", 30)
	config.set_value("player", "unknown_key", "keep-me")
	config.save(V1_PATH)


func _cleanup() -> void:
	for path: String in [V1_PATH, V2_PATH]:
		if FileAccess.file_exists(path):
			DirAccess.remove_absolute(ProjectSettings.globalize_path(path))


func _expect(condition: bool, message: String) -> void:
	if not condition:
		failures.append(message)


func _finish() -> void:
	if failures.is_empty():
		print("SETTINGS_MIGRATE PASS")
		quit(0)
		return
	for item in failures:
		print("SETTINGS_MIGRATE_FAIL %s" % item)
	quit(1)
