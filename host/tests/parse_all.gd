extends SceneTree
## 解析全部 .gd 脚本，替代 VS Code Problems 的“语法/类型错误=0”门。
## 用法：godot --headless --path host --script res://tests/parse_all.gd

var errors: Array[String] = []


func _initialize() -> void:
	_scan("res://scripts")
	_scan("res://tests")
	print("PARSE_ALL scripts=%d errors=%d" % [_count, errors.size()])
	if errors.is_empty():
		print("PARSE_ALL PASS")
		quit(0)
		return
	for item in errors:
		print("PARSE_ERROR ", item)
	quit(1)


var _count := 0


func _scan(root: String) -> void:
	var dir := DirAccess.open(root)
	if dir == null:
		return
	dir.list_dir_begin()
	var name := dir.get_next()
	while name != "":
		var path := root.path_join(name)
		if dir.current_is_dir():
			_scan(path)
		elif name.ends_with(".gd"):
			_count += 1
			var resource: Resource = load(path)
			if resource == null:
				errors.append(path)
		name = dir.get_next()
	dir.list_dir_end()
