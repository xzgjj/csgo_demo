class_name BuyWheel
extends Control
## 购买轮盘（SPEC-06 BUY-07）：6 扇区环形分类按钮，选择回调交给 BuyMenu。
## 独立文件以便 BuyMenu 保持在 CI 单文件 400 行预算内。

const RADIUS := 120.0
const CENTER := Vector2(180.0, 150.0)

var _on_select := Callable()


func rebuild(names: Array[String], on_select: Callable) -> void:
	_on_select = on_select
	for child in get_children():
		child.queue_free()
	var count := maxi(names.size(), 1)
	for i in names.size():
		var angle := -PI / 2.0 + TAU * float(i) / float(count)
		var button := Button.new()
		button.text = names[i]
		button.custom_minimum_size = Vector2(104.0, 36.0)
		button.position = CENTER + Vector2(cos(angle), sin(angle)) * RADIUS
		button.pressed.connect(func() -> void: _on_select.call(names[i]))
		add_child(button)
