extends SceneTree
## SET-04 非法值处理：类型不符→默认、越界→钳制、负数/NaN/枚举越界→拒绝，未知 id→拒绝。
## 用法：& godot --headless --path host --script res://tests/settings_clamp_test.gd

const StoreScript = preload("res://scripts/settings_store.gd")

var failures: Array[String] = []


func _initialize() -> void:
	var store = StoreScript.new("user://settings_clamp_unused.cfg")
	_check_slider(store)
	_check_type_mismatch(store)
	_check_enum_and_bool(store)
	_check_text_color_keybind(store)
	_check_disk_config()
	if failures.is_empty():
		print("SETTINGS_CLAMP PASS")
		quit(0)
		return
	for item in failures:
		print("SETTINGS_CLAMP_FAIL %s" % item)
	quit(1)


func _check_slider(store) -> void:
	_expect(store.set_value("camera.fov", 500.0), "越界 FOV 应被钳制后接受")
	_expect(is_equal_approx(float(store.get_value("camera.fov")), 110.0), "FOV 应钳到上限 110")
	store.set_value("camera.fov", -50.0)
	_expect(is_equal_approx(float(store.get_value("camera.fov")), 60.0), "FOV 应钳到下限 60")
	store.set_value("camera.fov", 92.6)
	_expect(is_equal_approx(float(store.get_value("camera.fov")), 93.0), "FOV 应按 step=1 取整")
	_expect(not store.set_value("camera.fov", "90"), "字符串不能写入 slider")


func _check_type_mismatch(store) -> void:
	_expect(not store.set_value("controls.mouse.sensitivity", "fast"), "灵敏度字符串应拒绝")
	_expect(not store.set_value("controls.mouse.sensitivity", NAN), "NaN 应拒绝")
	_expect(not store.set_value("controls.mouse.sensitivity", INF), "INF 应拒绝")
	var before := float(store.get_value("controls.mouse.sensitivity"))
	store.set_value("controls.mouse.sensitivity", 0.001)
	_expect(is_equal_approx(float(store.get_value("controls.mouse.sensitivity")), 0.006),
			"低于下限应钳制（before=%f）" % before)
	_expect(not store.set_value("no.such.item", 1.0), "未知 id 应拒绝")


func _check_enum_and_bool(store) -> void:
	_expect(not store.set_value("display.window.mode", "cinema"), "枚举越界应拒绝")
	_expect(str(store.get_value("display.window.mode")) == "borderless", "枚举拒绝后保持原值")
	_expect(store.set_value("display.window.mode", "windowed"), "合法枚举应接受")
	_expect(not store.set_value("display.window.vsync", "yes"), "bool 只接受布尔值")


func _check_text_color_keybind(store) -> void:
	_expect(store.set_value("account.profile.nickname", "abcdefghijklmnopqrstuvw"),
			"昵称应接受并截断")
	_expect(str(store.get_value("account.profile.nickname")).length() <= 16, "昵称超过 16 字符未截断")
	_expect(not store.set_value("gameplay.crosshair.color", "red"), "非法颜色应拒绝")
	_expect(store.set_value("gameplay.crosshair.color", "#FF0000"), "合法颜色应接受")
	_expect(store.set_value("controls.bind.forward", "w"), "小写键位应归一为大写")
	_expect(str(store.get_value("controls.bind.forward")) == "W", "键位应转大写")


## 磁盘侧：v2 文件里的字符串/越界值也必须在读入时钳制。
func _check_disk_config() -> void:
	var config := ConfigFile.new()
	config.set_value("meta", "version", 2)
	config.set_value("display", "display.window.vsync", "yes")
	config.set_value("display", "display.window.max_fps", 9999.0)
	config.set_value("audio", "audio.master.volume", "loud")
	config.set_value("controls", "controls.mouse.sensitivity", -1.0)
	var store = StoreScript.new("user://settings_clamp_unused.cfg")
	store.load_from(config)
	_expect(bool(store.get_value("display.window.vsync")), "bool 类型不符应回退默认 true")
	_expect(int(store.get_value("display.window.max_fps")) == 240, "越界 max_fps 应钳到 240")
	_expect(int(store.get_value("audio.master.volume")) == 80, "非法音量应回退默认 80")
	_expect(is_equal_approx(float(store.get_value("controls.mouse.sensitivity")), 0.006),
			"负灵敏度应钳到下限")


func _expect(condition: bool, message: String) -> void:
	if not condition:
		failures.append(message)
