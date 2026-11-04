extends SceneTree
## 宿主脚本型 smoke 的公共骨架（避免各 smoke 重复 bootstrap）：
## 实例化 main.tscn → 子类实现 `_run()` → 用 `expect()` 收集失败 → `finish()` 统一退出码。
## 用法：`extends "res://tests/scene_test.gd"`，实现 `_run()`；无 class_name（不依赖 .godot 类缓存）。

var main: Node
var failures: Array[String] = []


func _initialize() -> void:
	main = load("res://main.tscn").instantiate()
	root.add_child(main)
	_run.call_deferred()


## 子类实现（异步测试主体）；基类给一个空实现以便解析。
func _run() -> void:
	push_error("SceneTest: 子类必须实现 _run()")


func expect(condition: bool, message: String) -> void:
	if not condition:
		failures.append(message)


func finish(label: String) -> void:
	if failures.is_empty():
		print("%s PASS" % label)
		quit(0)
	else:
		for item in failures:
			print("%s_FAIL %s" % [label, item])
		quit(1)
