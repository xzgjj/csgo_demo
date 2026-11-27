class_name SettingsWidgets
extends RefCounted
## 设置条目控件工厂（SPEC-05 §5.4.4）：由注册表 type 生成控件，写回 SettingsStore。
## 单一职责：只处理“一条设置 ↔ 一个控件”的绑定；页面导航/搜索/页脚在 settings_page.gd。

const ROW_LABEL_WIDTH := 210.0
const CONTROL_WIDTH := 260.0
const DOT_TEXT := "●"

var store: SettingsStore
var rows: Dictionary = {}
var rebind_target := ""


func setup(settings: SettingsStore) -> void:
	store = settings


## 在 container 里按 items 顺序建行；返回行节点数量。
func build(container: Control, items: Array[Dictionary]) -> int:
	var count := 0
	for item: Dictionary in items:
		if rows.has(str(item["id"])):
			continue
		var row := _build_row(item)
		container.add_child(row["root"])
		rows[str(item["id"])] = row
		count += 1
	return count


func row(item_id: String) -> Dictionary:
	var found: Variant = rows.get(item_id, {})
	return found if typeof(found) == TYPE_DICTIONARY else {}


## 外部改动（商店/快捷键/页面重置）后刷新单行显示与“已修改”圆点。
func sync_item(item_id: String) -> void:
	_sync_row(item_id)


func control_of(item_id: String) -> Control:
	var found := row(item_id)
	return found.get("control", null) as Control


func sync_all() -> void:
	for item_id: String in rows.keys():
		_sync_row(item_id)


## 捕获下一次按键并写入当前重绑项；返回是否消费了事件。
func capture_rebind(event: InputEvent) -> bool:
	if rebind_target.is_empty():
		return false
	if event is InputEventKey and (event as InputEventKey).echo:
		return false
	if event is InputEventKey and (event as InputEventKey).keycode == KEY_ESCAPE:
		rebind_target = ""
		return true
	var binding := Keybinds.binding_of(event)
	if binding.is_empty():
		return false
	var item_id := rebind_target
	rebind_target = ""
	store.set_value(item_id, binding)
	_sync_row(item_id)
	return true


func begin_rebind(item_id: String) -> void:
	rebind_target = item_id


func end_rebind() -> void:
	rebind_target = ""


func is_rebinding() -> bool:
	return not rebind_target.is_empty()


func _build_row(item: Dictionary) -> Dictionary:
	var item_id := str(item["id"])
	var root := HBoxContainer.new()
	root.add_theme_constant_override("separation", 12)
	root.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var label := Label.new()
	label.text = str(item["label"])
	label.custom_minimum_size = Vector2(ROW_LABEL_WIDTH, 30)
	label.tooltip_text = item_id
	root.add_child(label)
	var dot := Label.new()
	dot.text = DOT_TEXT
	dot.custom_minimum_size = Vector2(16, 30)
	dot.add_theme_color_override("font_color", Color(1.0, 0.78, 0.32))
	dot.visible = store.is_modified(item_id)
	root.add_child(dot)
	var control := _make_control(item)
	control.custom_minimum_size = Vector2(CONTROL_WIDTH, 30)
	control.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	root.add_child(control)
	var value_label := Label.new()
	value_label.custom_minimum_size = Vector2(78, 30)
	value_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	root.add_child(value_label)
	var result := {"root": root, "control": control, "dot": dot, "value": value_label,
			"label": label}
	_sync_row_with(item, result)
	return result


func _make_control(item: Dictionary) -> Control:
	var item_id := str(item["id"])
	match str(item["type"]):
		"bool":
			var toggle := CheckButton.new()
			toggle.focus_mode = Control.FOCUS_ALL
			toggle.toggled.connect(func(value: bool) -> void: store.set_value(item_id, value))
			return toggle
		"enum":
			var option := OptionButton.new()
			option.focus_mode = Control.FOCUS_ALL
			var values: Array = item.get("values", [])
			for index in range(values.size()):
				option.add_item(_enum_text(str(values[index])), index)
			option.item_selected.connect(func(index: int) -> void:
				store.set_value(item_id, str(values[index])))
			return option
		"slider":
			var slider := HSlider.new()
			slider.focus_mode = Control.FOCUS_ALL
			slider.min_value = float(item["min"])
			slider.max_value = float(item["max"])
			slider.step = float(item["step"])
			slider.value_changed.connect(func(value: float) -> void:
				store.set_value(item_id, value))
			return slider
		"keybind":
			var button := Button.new()
			button.focus_mode = Control.FOCUS_ALL
			button.pressed.connect(func() -> void: begin_rebind(item_id))
			return button
		"color":
			var picker := ColorPickerButton.new()
			picker.focus_mode = Control.FOCUS_ALL
			picker.custom_minimum_size = Vector2(64, 30)
			picker.color_changed.connect(func(color: Color) -> void:
				store.set_value(item_id, "#" + color.to_html(false)))
			return picker
		"text":
			var edit := LineEdit.new()
			edit.max_length = int(item.get("max_length", 0))
			edit.text_submitted.connect(func(text: String) -> void:
				store.set_value(item_id, text))
			edit.focus_exited.connect(func() -> void:
				store.set_value(item_id, edit.text))
			return edit
		_:
			return Label.new()


func _sync_row(item_id: String) -> void:
	var data := row(item_id)
	var item := store.registry.item(item_id)
	if data.is_empty() or item.is_empty():
		return
	_sync_row_with(item, data)


func _sync_row_with(item: Dictionary, data: Dictionary) -> void:
	var item_id := str(item["id"])
	var value: Variant = store.get_value(item_id)
	var control: Control = data["control"]
	match str(item["type"]):
		"bool":
			(control as CheckButton).set_pressed_no_signal(bool(value))
		"enum":
			var values: Array = item.get("values", [])
			(control as OptionButton).select(maxi(values.find(value), 0))
		"slider":
			(control as HSlider).set_value_no_signal(float(value))
		"keybind":
			(control as Button).text = str(value)
		"color":
			(control as ColorPickerButton).color = Color.from_string(str(value), Color.WHITE)
		"text":
			(control as LineEdit).text = str(value)
	(data["value"] as Label).text = _value_text(item, value)
	(data["dot"] as Label).visible = store.is_modified(item_id)


func _value_text(item: Dictionary, value: Variant) -> String:
	var unit := str(item.get("unit", ""))
	match str(item["type"]):
		"slider":
			var step := float(item.get("step", 1.0))
			var text := "%d" % int(value) if step >= 1.0 else "%.2f" % float(value)
			return text + (" " + unit if not unit.is_empty() else "")
		"bool":
			return "开" if bool(value) else "关"
		"keybind", "text":
			return str(value)
		"color":
			return str(value)
		"enum":
			return _enum_text(str(value))
		_:
			return str(value)


func _enum_text(raw: String) -> String:
	var names := {
		"fullscreen": "全屏", "windowed": "窗口", "borderless": "无边框",
		"low": "低", "medium": "中", "high": "高", "ultra": "极高", "off": "关",
		"nearest": "最近邻", "bilinear": "双线性", "4x": "4×", "8x": "8×",
		"stereo": "立体声", "surround": "环绕声", "headphones": "耳机",
		"classic": "经典", "dot": "点", "cross": "十字", "tshape": "T 形",
		"protanopia": "红色盲", "deuteranopia": "绿色盲", "tritanopia": "蓝色盲",
		"default": "默认", "southpaw": "左手", "zh": "中文", "en": "English",
	}
	return str(names.get(raw, raw))
