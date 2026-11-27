extends SceneTree
## SET-01 注册表契约：8 页 / >=60 项 / id 唯一 / 默认齐全且类型正确 / 搜索可用。
## 用法：& godot --headless --path host --script res://tests/settings_registry_test.gd

var failures: Array[String] = []


func _initialize() -> void:
	var registry := SettingsRegistry.new()
	var ok := registry.load_default()
	_expect(ok, "注册表应能加载 content/defs/settings_registry.json")
	_expect(registry.errors.is_empty(), "注册表校验错误：%s" % registry.errors)
	_expect(registry.pages.size() >= SettingsRegistry.MIN_PAGES,
			"页面数 %d < %d" % [registry.pages.size(), SettingsRegistry.MIN_PAGES])
	_expect(registry.items.size() >= SettingsRegistry.MIN_ITEMS,
			"设置项 %d < %d" % [registry.items.size(), SettingsRegistry.MIN_ITEMS])
	_expect(registry.page_ids().size() == registry.pages.size(), "页面 id 应唯一且非空")
	_check_defaults(registry)
	_check_search(registry)
	_check_rejects_bad_registry()
	_finish()


func _check_defaults(registry: SettingsRegistry) -> void:
	for item_id: String in registry.item_ids():
		var spec := registry.item(item_id)
		_expect(str(spec["page"]) == item_id.split(".")[0], "%s 页面前缀不一致" % item_id)
		var default_value: Variant = registry.default_of(item_id)
		_expect(default_value != null, "%s 缺少默认值" % item_id)
		_expect(registry.coerce(str(spec["type"]), default_value, spec) != null,
				"%s 默认值类型非法" % item_id)
		if str(spec["type"]) == "slider":
			var value := float(default_value)
			_expect(value >= float(spec["min"]) and value <= float(spec["max"]),
					"%s 默认值越界" % item_id)
		if str(spec["type"]) == "enum":
			_expect((spec["values"] as Array).has(default_value), "%s 默认值不在枚举" % item_id)
	var pages := registry.page_ids()
	for page_id: String in pages:
		var page := registry.page(page_id)
		_expect(not (page["groups"] as Array).is_empty(), "%s 页面没有分组" % page_id)


func _check_search(registry: SettingsRegistry) -> void:
	var by_label := registry.search("准星")
	_expect(by_label.has("gameplay.crosshair.color"), "中文搜索应命中准星项")
	var by_id := registry.search("camera.fov")
	_expect(by_id == ["camera.fov"], "id 搜索应只命中 camera.fov")


func _check_rejects_bad_registry() -> void:
	var bad := SettingsRegistry.new()
	var data := {"schema": "settings-registry/1", "pages": [
		{"id": "display", "title": "显示", "groups": [
			{"id": "g", "title": "组", "items": [
				{"id": "display.x", "label": "X", "type": "bogus", "default": 1,
						"apply": "immediate", "target": "hud"},
			]},
		]},
	]}
	_expect(not bad.load_dict(data), "非法 type 应被拒绝")
	_expect(not bad.errors.is_empty(), "非法注册表应有错误列表")


func _expect(condition: bool, message: String) -> void:
	if not condition:
		failures.append(message)


func _finish() -> void:
	if failures.is_empty():
		print("SETTINGS_REGISTRY PASS")
		quit(0)
		return
	for item in failures:
		print("SETTINGS_REGISTRY_FAIL %s" % item)
	quit(1)
