class_name Keybinds
extends RefCounted
## 键位重建（SPEC-05 §5.4.3 target=input）：注册表 controls.bind.* → InputMap 动作。
## 动作名保持宿主既有约定（move_forward/…）；新增动作只在 ACTION_ITEMS 里登记，
## main.gd 通过 Input.is_action_pressed 读取，不再散落 KEY_* 常量。

const ACTION_ITEMS := {
	"move_forward": "controls.bind.forward",
	"move_back": "controls.bind.back",
	"move_left": "controls.bind.left",
	"move_right": "controls.bind.right",
	"jump": "controls.bind.jump",
	"duck": "controls.bind.duck",
	"fire": "controls.bind.fire",
	"reload": "controls.bind.reload",
	"buy": "controls.bind.buy",
	"pickup": "controls.bind.pickup",
	"throw_he": "controls.bind.throw_he",
	"throw_flash": "controls.bind.throw_flash",
	"throw_smoke": "controls.bind.throw_smoke",
	"scoreboard": "controls.bind.scoreboard",
	"refund": "controls.bind.refund",
}
const SPECIAL_KEYS := {
	"SPACE": KEY_SPACE,
	"TAB": KEY_TAB,
	"CTRL": KEY_CTRL,
	"SHIFT": KEY_SHIFT,
	"ALT": KEY_ALT,
	"ESC": KEY_ESCAPE,
	"ESCAPE": KEY_ESCAPE,
	"ENTER": KEY_ENTER,
	"BACKSPACE": KEY_BACKSPACE,
}
const MOUSE_BUTTONS := {
	"MOUSE_LEFT": MOUSE_BUTTON_LEFT,
	"MOUSE_RIGHT": MOUSE_BUTTON_RIGHT,
	"MOUSE_MIDDLE": MOUSE_BUTTON_MIDDLE,
}
const MOUSE_PREFIX := "MOUSE_"


## 按键名 → 物理键码；未知返回 KEY_NONE。
static func keycode_of(binding: String) -> int:
	var name := binding.strip_edges().to_upper()
	if SPECIAL_KEYS.has(name):
		return int(SPECIAL_KEYS[name])
	if name.begins_with(MOUSE_PREFIX):
		return KEY_NONE
	var code := OS.find_keycode_from_string(name)
	return code if code != KEY_NONE else KEY_NONE


static func is_mouse(binding: String) -> bool:
	return binding.strip_edges().to_upper().begins_with(MOUSE_PREFIX)


static func mouse_button_of(binding: String) -> int:
	return int(MOUSE_BUTTONS.get(binding.strip_edges().to_upper(), MOUSE_BUTTON_NONE))


## 写入 InputMap（幂等：先清空该动作已有事件，再按注册表加一条）。
static func apply(store: SettingsStore) -> int:
	var rebuilt := 0
	for action: String in ACTION_ITEMS.keys():
		var item_id := str(ACTION_ITEMS[action])
		if not store.registry.items.has(item_id):
			continue
		_rebuild_action(action, str(store.get_value(item_id)))
		rebuilt += 1
	return rebuilt


static func _rebuild_action(action: String, binding: String) -> void:
	if InputMap.has_action(action):
		InputMap.action_erase_events(action)
	else:
		InputMap.add_action(action)
	var event := _event_for(binding)
	if event != null:
		InputMap.action_add_event(action, event)


static func _event_for(binding: String) -> InputEvent:
	if is_mouse(binding):
		var mouse := InputEventMouseButton.new()
		mouse.button_index = mouse_button_of(binding)
		mouse.pressed = true
		return mouse
	var code := keycode_of(binding)
	if code == KEY_NONE:
		return null
	var key := InputEventKey.new()
	key.physical_keycode = code
	key.pressed = true
	return key


## 输入事件 → 注册表绑定名（设置页“按键中…”捕获后用）。
static func binding_of(event: InputEvent) -> String:
	if event is InputEventMouseButton:
		var button: int = (event as InputEventMouseButton).button_index
		for name: String in MOUSE_BUTTONS.keys():
			if int(MOUSE_BUTTONS[name]) == button:
				return name
		return ""
	if event is InputEventKey:
		var key := (event as InputEventKey).physical_keycode
		if key == KEY_NONE:
			key = (event as InputEventKey).keycode
		for name: String in SPECIAL_KEYS.keys():
			if int(SPECIAL_KEYS[name]) == key:
				return name
		var text := OS.get_keycode_string(key)
		return text if not text.is_empty() else ""
	return ""


static func action_items() -> Dictionary:
	return ACTION_ITEMS.duplicate()
