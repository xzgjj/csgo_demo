class_name MainMenuPage
extends CanvasLayer
## 主菜单页：标题 + 开始游戏 / 设置 / 退出，底部发操作提示；按钮由 main 连接。

var start_button: Button
var settings_button: Button
var quit_button: Button
var hint_label: Label


func build() -> void:
	layer = 3
	var box := UiFactory.build_card(self, "PROJECT RECOIL", 400.0)
	UiFactory.add_label(box, "de_ash 灰盒 Demo · C++ 纯函数内核 + Godot 表现层", 15)
	start_button = UiFactory.add_button(box, "开始游戏")
	settings_button = UiFactory.add_button(box, "设置")
	quit_button = UiFactory.add_button(box, "退出")
	hint_label = UiFactory.add_label(
			box, "WASD 移动 · Shift 静步 · Space 跳 · Ctrl 蹲 · 左键开火 · 右键瞄准 · R 换弹 · 1/2/3 切枪 · F5 视角 · F3 调试 · Esc 暂停 · Tab 比分板 · B 护甲 · E 下包 · G/F/V 投掷", 14)
	hint_label.add_theme_color_override("font_color", UiFactory.NEUTRAL_COLOR)


func button_texts() -> Array[String]:
	var texts: Array[String] = []
	for button in [start_button, settings_button, quit_button]:
		texts.append(str(button.text))
	return texts
