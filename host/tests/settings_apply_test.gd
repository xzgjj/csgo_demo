extends SceneTree
## SET-05 热应用矩阵：immediate 项 1 次分发内生效（音频/相机/输入/HUD），next_round 进 deferred。
## 用法：& godot --headless --path host --script res://tests/settings_apply_test.gd

const StoreScript = preload("res://scripts/settings_store.gd")
const KeybindsScript = preload("res://scripts/keybinds.gd")

var failures: Array[String] = []
var hits: Dictionary = {}
var last_values: Dictionary = {}


func _initialize() -> void:
	var store = StoreScript.new("user://settings_apply_test.cfg")
	var applier = SettingsApplier.new()
	for target: String in SettingsApplier.TARGETS:
		hits[target] = 0
		applier.register(target, _probe.bind(target))
	store.changed.connect(applier.on_changed)
	_check_camera(store, applier)
	_check_audio(store, applier)
	_check_input(store, applier)
	_check_hud(store, applier)
	_check_next_round(store, applier)
	_check_apply_all(store, applier)
	_finish()


func _probe(item_id: String, value: Variant, target: String) -> void:
	hits[target] = int(hits.get(target, 0)) + 1
	last_values[item_id] = value
	if item_id == "audio.master.volume":
		var level := clampf(float(value) / 100.0, 0.0001, 1.0)
		AudioServer.set_bus_volume_db(0, linear_to_db(level))


func _check_camera(store, applier) -> void:
	store.set_value("camera.fov", 104.0)
	_expect(int(hits["camera"]) == 1, "camera.fov 应在同一次 set_value 内分发")
	_expect(is_equal_approx(float(last_values.get("camera.fov", 0.0)), 104.0), "相机分发值错误")
	_expect(is_equal_approx(float(applier.last_value("camera.fov")), 104.0), "applier 应记录最后值")


func _check_audio(store, applier) -> void:
	store.set_value("audio.master.volume", 50.0)
	_expect(int(hits["audio"]) == 1, "主音量应即时分发")
	var db := AudioServer.get_bus_volume_db(0)
	_expect(absf(db - linear_to_db(0.5)) < 0.5, "主总线 dB 应跟随 50%%（实际 %f）" % db)
	store.set_value("audio.master.volume", 80.0)


func _check_input(store, applier) -> void:
	store.set_value("controls.bind.forward", "UP")
	KeybindsScript.apply(store)
	_expect(int(hits["input"]) == 1, "键位改动应即时分发")
	var events := InputMap.action_get_events("move_forward")
	_expect(events.size() == 1, "move_forward 应恰好 1 条绑定")
	_expect((events[0] as InputEventKey).physical_keycode == KEY_UP, "前进键应改为 UP")
	store.set_value("controls.bind.forward", "W")
	KeybindsScript.apply(store)


func _check_hud(store, applier) -> void:
	store.set_value("gameplay.crosshair.color", "#ff0000")
	_expect(int(hits["hud"]) == 1, "准星颜色应即时分发")
	_expect(str(applier.last_value("gameplay.crosshair.color")) == "#ff0000", "HUD 分发值错误")


func _check_next_round(store, applier) -> void:
	store.set_value("gameplay.auto.pickup", false)
	_expect(applier.deferred_items().has("gameplay.auto.pickup"), "next_round 项应进入 deferred")
	store.set_value("gameplay.auto.pickup", true)
	applier.clear_deferred()
	_expect(applier.deferred_items().is_empty(), "clear_deferred 应清空")


func _check_apply_all(store, applier) -> void:
	var total: int = applier.apply_all(store)
	_expect(total >= SettingsApplier.TARGETS.size(), "apply_all 应覆盖全部 target（%d）" % total)
	for target: String in SettingsApplier.TARGETS:
		_expect(int(hits[target]) > 0, "target %s 从未被分发" % target)


func _expect(condition: bool, message: String) -> void:
	if not condition:
		failures.append(message)


func _finish() -> void:
	if failures.is_empty():
		print("SETTINGS_APPLY PASS")
		quit(0)
		return
	for item in failures:
		print("SETTINGS_APPLY_FAIL %s" % item)
	quit(1)
