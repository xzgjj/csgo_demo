class_name SettingsBridge
extends RefCounted
## 宿主侧热应用接线（SPEC-05 §5.4.3）：把 SettingsApplier 的 target 接到具体节点/服务。
## 只读 store、只写表现层；规则与默认值仍在注册表/内核，这里不重复任何数值。
## main.gd 只保留 `bridge.attach(self, settings)` 一行，避免主文件继续膨胀。

var main: Node
var store: SettingsStore
var applier: SettingsApplier


func attach(main_node: Node, settings: SettingsStore) -> void:
	main = main_node
	store = settings
	applier = SettingsApplier.new()
	applier.register("camera", _on_camera)
	applier.register("audio", _on_audio)
	applier.register("input", _on_input)
	applier.register("hud", _on_hud)
	applier.register("window", _on_window)
	applier.register("renderer", _on_renderer)
	applier.register("env_adjustment", _on_env)
	applier.register("gameplay", _on_noop)
	applier.register("account", _on_noop)
	store.changed.connect(applier.on_changed)
	Keybinds.apply(store)
	apply_all()


## 全量重放：启动、关闭设置页与“应用”按钮都走这里。
func apply_all() -> int:
	if applier == null:
		return 0
	var total := applier.apply_all(store)
	_apply_hud()
	return total


func _apply_hud() -> void:
	var hud: Variant = main.get("hud")
	if hud != null and hud.has_method("apply_settings"):
		hud.call("apply_settings", store)
	var radar: Variant = main.get("radar")
	if radar != null:
		var zoom := float(store.get_value("gameplay.radar.zoom"))
		(radar as Control).scale = Vector2(zoom, zoom)
		radar.set("enabled", bool(store.get_value("gameplay.info.radar")))
	var info: Variant = main.get_node_or_null("InfoOverlay")
	if info != null and info.has_method("refresh"):
		info.refresh()


func _on_camera(item_id: String, value: Variant) -> void:
	var camera: Variant = main.get("camera")
	if item_id == "camera.fov" and camera is Camera3D:
		(camera as Camera3D).fov = float(value)


func _on_audio(item_id: String, value: Variant) -> void:
	if item_id == "audio.master.mute":
		AudioServer.set_bus_mute(0, bool(value))
		return
	var bus_name := "Master"
	if item_id.begins_with("audio.sfx"):
		bus_name = "SFX"
	elif item_id.begins_with("audio.music"):
		bus_name = "Music"
	elif item_id.begins_with("audio.voice"):
		bus_name = "Voice"
	var index := AudioServer.get_bus_index(bus_name)
	if index < 0:
		index = 0
	var level := clampf(float(value) / 100.0, 0.0001, 1.0)
	AudioServer.set_bus_volume_db(index, linear_to_db(level))


func _on_input(item_id: String, _value: Variant) -> void:
	if item_id.begins_with("controls.bind."):
		Keybinds.apply(store)


func _on_hud(_item_id: String, _value: Variant) -> void:
	_apply_hud()


func _on_window(item_id: String, value: Variant) -> void:
	var mode := str(value)
	if item_id == "display.window.mode":
		if mode == "fullscreen":
			DisplayServer.window_set_flag(DisplayServer.WINDOW_FLAG_BORDERLESS, false)
			DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_FULLSCREEN)
		elif mode == "windowed":
			DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_WINDOWED)
			DisplayServer.window_set_flag(DisplayServer.WINDOW_FLAG_BORDERLESS, false)
		else:
			DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_WINDOWED)
			DisplayServer.window_set_flag(DisplayServer.WINDOW_FLAG_BORDERLESS, true)
	elif item_id == "display.window.resolution":
		var parts := mode.split("x")
		if parts.size() == 2:
			DisplayServer.window_set_size(Vector2i(int(parts[0]), int(parts[1])))


func _on_renderer(item_id: String, value: Variant) -> void:
	if item_id == "display.window.max_fps":
		Engine.max_fps = int(value)
	elif item_id == "display.window.vsync":
		var mode := DisplayServer.VSYNC_ENABLED if bool(value) else DisplayServer.VSYNC_DISABLED
		DisplayServer.window_set_vsync_mode(mode)


func _on_env(_item_id: String, _value: Variant) -> void:
	# de_ash 世界没有独立 WorldEnvironment；亮度项保留分发记录，待地图侧提供环境节点后接入。
	pass


func _on_noop(_item_id: String, _value: Variant) -> void:
	pass
