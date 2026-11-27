class_name MainMenuPage
extends CanvasLayer
## 主菜单：单机式左侧指挥栏（作战 / 装备 / 设置 / 退出）+ 右下当前部署。
## “作战部署”打开 StartGamePage；设置入口沿用 SPEC-05 的 8 页设置页。

var start_button: Button
var settings_button: Button
var loadout_button: Button
var quit_button: Button
var hint_label: Label
var status_label: Label


func build() -> void:
	layer = 3
	var root := MenuSkin.root(self)
	var box := MenuSkin.side_panel(root, 520.0)
	MenuSkin.kicker(box, "LOCAL TACTICAL SANDBOX  //  00.1")
	MenuSkin.heading(box, "PROJECT\nRECOIL", 52)
	MenuSkin.note(box, "单一玩家 · 本地 5v5 · 新地图已锁定")
	MenuSkin.rule(box)
	MenuSkin.kicker(box, "指挥中心  /  COMMAND")
	start_button = MenuSkin.nav_button(box, "01   作战部署   /   DEPLOY", true)
	loadout_button = MenuSkin.nav_button(box, "02   装备整备   /   LOADOUT")
	settings_button = MenuSkin.nav_button(box, "03   系统设置   /   OPTIONS")
	quit_button = MenuSkin.nav_button(box, "04   退出游戏   /   EXIT")
	var spacer := Control.new()
	spacer.size_flags_vertical = Control.SIZE_EXPAND_FILL
	box.add_child(spacer)
	hint_label = MenuSkin.note(
			box, "WASD 移动 · B 购买 · E 下包 · Esc 暂停 · Tab 比分板")
	hint_label.add_theme_font_size_override("font_size", 12)
	_build_status(root)


func _build_status(root: Control) -> void:
	var card := MenuSkin.corner_card(root, 360.0, 168.0)
	MenuSkin.kicker(card, "当前部署  /  OPERATION")
	MenuSkin.heading(card, MapLoader.default_map_id().to_upper(), 30)
	status_label = MenuSkin.note(card, "5v5 · 爆破 · 本地运行")
	MenuSkin.note(card, "地图已锁定；装备与设置进入后生效。")


func button_texts() -> Array[String]:
	var texts: Array[String] = []
	for button in [start_button, loadout_button, settings_button, quit_button]:
		texts.append(str(button.text))
	return texts
