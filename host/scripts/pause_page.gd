class_name PausePage
extends CanvasLayer
## 暂停页：继续 / 设置 / 返回主菜单；Esc 恢复由 main 处理。

var resume_button: Button
var settings_button: Button
var menu_button: Button


func build() -> void:
	layer = 3
	var box := UiFactory.build_card(self, "已暂停", 330.0)
	UiFactory.add_label(box, "回合仍在等待，恢复后继续推进", 15)
	resume_button = UiFactory.add_button(box, "继续")
	settings_button = UiFactory.add_button(box, "设置")
	menu_button = UiFactory.add_button(box, "返回主菜单")
	UiFactory.add_label(box, "Esc 继续游戏", 14)


func button_texts() -> Array[String]:
	var texts: Array[String] = []
	for button in [resume_button, settings_button, menu_button]:
		texts.append(str(button.text))
	return texts
