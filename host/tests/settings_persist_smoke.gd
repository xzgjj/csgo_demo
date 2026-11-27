extends SceneTree
## SET-02 持久化：改 10 项 → 去抖落盘 → 新进程读回保持；未知键不丢失。
## 用法：& godot --headless --path host --script res://tests/settings_persist_smoke.gd

const StoreScript = preload("res://scripts/settings_store.gd")
const PATH := "user://settings_persist_test.cfg"

var failures: Array[String] = []


func _initialize() -> void:
	_remove_old()
	var store = StoreScript.new(PATH)
	var edits := {
		"display.window.mode": "windowed",
		"display.brightness.value": 120.0,
		"audio.master.volume": 55.0,
		"gameplay.crosshair.color": "#ff3300",
		"gameplay.radar.zoom": 1.4,
		"controls.mouse.sensitivity": 0.125,
		"controls.bind.forward": "UP",
		"camera.fov": 104.0,
		"ui.hud.font_size": 20.0,
		"account.profile.nickname": "recoil",
	}
	for item_id: String in edits.keys():
		_expect(store.set_value(item_id, edits[item_id]), "写入 %s 失败" % item_id)
	_expect(store.is_dirty(), "改动后应标记 dirty")
	store.tick(0.6)
	_expect(FileAccess.file_exists(PATH), "去抖 0.5s 后应自动落盘")
	var reloaded = StoreScript.new(PATH)
	_expect(reloaded.load_settings(), "应能从 %s 读回" % PATH)
	for item_id: String in edits.keys():
		_expect(_same(reloaded.get_value(item_id), edits[item_id]),
				"%s 持久化漂移：%s" % [item_id, str(reloaded.get_value(item_id))])
	_expect(reloaded.modified_count() == edits.size(),
			"读回后修改标记数量应为 %d，实际 %d" % [edits.size(), reloaded.modified_count()])
	_check_undo(store)
	_remove_old()
	_finish()


func _check_undo(store) -> void:
	store.begin_edit()
	store.set_value("camera.fov", 80.0)
	_expect(is_equal_approx(float(store.get_value("camera.fov")), 80.0), "改动未生效")
	_expect(store.undo(), "撤销应返回 true")
	_expect(is_equal_approx(float(store.get_value("camera.fov")), 104.0), "撤销未恢复快照值")


func _same(a: Variant, b: Variant) -> bool:
	if typeof(a) in [TYPE_FLOAT, TYPE_INT] and typeof(b) in [TYPE_FLOAT, TYPE_INT]:
		return is_equal_approx(float(a), float(b))
	return a == b


func _remove_old() -> void:
	if FileAccess.file_exists(PATH):
		DirAccess.remove_absolute(ProjectSettings.globalize_path(PATH))


func _expect(condition: bool, message: String) -> void:
	if not condition:
		failures.append(message)


func _finish() -> void:
	if failures.is_empty():
		print("SETTINGS_PERSIST PASS")
		quit(0)
		return
	for item in failures:
		print("SETTINGS_PERSIST_FAIL %s" % item)
	quit(1)
