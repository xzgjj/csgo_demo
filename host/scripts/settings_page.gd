class_name SettingsPage
extends CanvasLayer
## 设置页：FOV / 灵敏度 / 主音量（音量当前只有占位值，主总线跟随）。
## 改动实时生效（changed 信号由 main 应用），返回时写入 user://settings.cfg。

signal changed

var store: SettingsStore
var fov_slider: HSlider
var sensitivity_slider: HSlider
var volume_slider: HSlider
var fov_value: Label
var sensitivity_value: Label
var volume_value: Label
var status_label: Label
var back_button: Button


func build(settings: SettingsStore) -> void:
	store = settings
	layer = 3
	var box := UiFactory.build_card(self, "设置", 400.0)
	_build_sliders(box)
	status_label = UiFactory.add_label(box, "设置写入 user://settings.cfg", 14)
	status_label.add_theme_color_override("font_color", UiFactory.NEUTRAL_COLOR)
	back_button = UiFactory.add_button(box, "保存并返回")
	sync_from_store()


func _build_sliders(box: Control) -> void:
	var fov_row: Dictionary = UiFactory.add_slider_row(box, "视野 FOV")
	fov_slider = fov_row["slider"] as HSlider
	fov_value = fov_row["value"] as Label
	fov_slider.min_value = SettingsStore.FOV_MIN
	fov_slider.max_value = SettingsStore.FOV_MAX
	fov_slider.step = 1.0
	fov_slider.value_changed.connect(_on_fov_changed)
	var sensitivity_row: Dictionary = UiFactory.add_slider_row(box, "鼠标灵敏度")
	sensitivity_slider = sensitivity_row["slider"] as HSlider
	sensitivity_value = sensitivity_row["value"] as Label
	sensitivity_slider.min_value = SettingsStore.SENSITIVITY_MIN
	sensitivity_slider.max_value = SettingsStore.SENSITIVITY_MAX
	sensitivity_slider.step = 0.005
	sensitivity_slider.value_changed.connect(_on_sensitivity_changed)
	var volume_row: Dictionary = UiFactory.add_slider_row(box, "主音量")
	volume_slider = volume_row["slider"] as HSlider
	volume_value = volume_row["value"] as Label
	volume_slider.min_value = SettingsStore.VOLUME_MIN
	volume_slider.max_value = SettingsStore.VOLUME_MAX
	volume_slider.step = 1.0
	volume_slider.value_changed.connect(_on_volume_changed)


func sync_from_store() -> void:
	if store == null:
		return
	fov_slider.set_value_no_signal(store.fov)
	sensitivity_slider.set_value_no_signal(store.sensitivity)
	volume_slider.set_value_no_signal(store.volume)
	fov_value.text = "%d" % int(store.fov)
	sensitivity_value.text = "%.3f" % store.sensitivity
	volume_value.text = "%d" % store.volume


## 把滑条值写回 store 并落盘；返回是否写成功。
func save_settings() -> bool:
	store.fov = fov_slider.value
	store.sensitivity = sensitivity_slider.value
	store.volume = int(volume_slider.value)
	var ok: bool = store.save_settings()
	status_label.text = "已保存 → user://settings.cfg" if ok else "保存失败：user:// 不可写"
	return ok


func _on_fov_changed(value: float) -> void:
	store.fov = clampf(value, SettingsStore.FOV_MIN, SettingsStore.FOV_MAX)
	fov_value.text = "%d" % int(store.fov)
	changed.emit()


func _on_sensitivity_changed(value: float) -> void:
	store.sensitivity = clampf(value, SettingsStore.SENSITIVITY_MIN, SettingsStore.SENSITIVITY_MAX)
	sensitivity_value.text = "%.3f" % store.sensitivity
	changed.emit()


func _on_volume_changed(value: float) -> void:
	store.volume = clampi(int(value), SettingsStore.VOLUME_MIN, SettingsStore.VOLUME_MAX)
	volume_value.text = "%d" % store.volume
	changed.emit()
