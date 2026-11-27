class_name MainMenuPage
extends CanvasLayer
## 主菜单：品牌标题 + 当前内容摘要 + 开始游戏 / 物品栏 / 设置 / 退出。
## 开始游戏不再直连开局，而是打开 StartGamePage 做地图与模式选择。

var start_button: Button
var settings_button: Button
var loadout_button: Button
var quit_button: Button
var hint_label: Label


func build() -> void:
	layer = 3
	var box := UiFactory.build_card(self, "PROJECT RECOIL", 460.0, 560.0)
	UiFactory.add_label(box, "战术射击灰盒 · C++ 确定性内核 + Godot 表现层", 15)
	UiFactory.add_label(box, _map_line(), 14).add_theme_color_override(
			"font_color", UiFactory.NEUTRAL_COLOR)
	start_button = UiFactory.add_button(box, "开始游戏 · 选择地图与模式")
	loadout_button = UiFactory.add_button(box, "物品栏 / 装备")
	settings_button = UiFactory.add_button(box, "设置 · 画面 / 音频 / 按键")
	quit_button = UiFactory.add_button(box, "退出")
	hint_label = UiFactory.add_label(
			box, "WASD 移动 · Shift 静步 · Space 跳 · Ctrl 蹲 · 左键开火 · R 换弹 · 1/2/3 切枪 · F5 视角 · F3 调试 · Esc 暂停 · Tab 比分板 · B 购买 · E 下包", 13)
	hint_label.add_theme_color_override("font_color", UiFactory.NEUTRAL_COLOR)


func _map_line() -> String:
	var ids: Array[String] = []
	for item: Dictionary in MapLoader.registry().get("maps", []):
		ids.append(str(item.get("id", "")))
	if ids.is_empty():
		return "地图注册表不可用 · 将回退 de_ash"
	return "可用地图 %s · 默认 %s" % [" / ".join(ids), MapLoader.default_map_id()]


func button_texts() -> Array[String]:
	var texts: Array[String] = []
	for button in [start_button, loadout_button, settings_button, quit_button]:
		texts.append(str(button.text))
	return texts
