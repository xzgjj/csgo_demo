class_name SettingsRegistry
extends RefCounted
## 设置注册表（SPEC-05 §5.4.1）：content/defs/settings_registry.json 的唯一读入口。
## 页面/分组/条目全部由 JSON 生成；默认值只在这里读取，UI 与代码不得再写一份。
## 注意：注册表是内容数据，不在 host/ 工程内；以 res:// 上级目录的绝对路径读取（开发/演示运行方式）。

const DEFAULT_RELATIVE := "../content/defs/settings_registry.json"
const SCHEMA := "settings-registry/1"
const MIN_PAGES := 8
const MIN_ITEMS := 60
const VALID_TYPES: Array[String] = ["bool", "enum", "slider", "keybind", "color", "text"]
const VALID_APPLY: Array[String] = ["immediate", "restart", "next_round"]
const VALID_TARGETS: Array[String] = [
	"window", "renderer", "env_adjustment", "camera", "audio", "input", "hud",
	"gameplay", "account",
]

var pages: Array[Dictionary] = []
var items: Dictionary = {}
var errors: PackedStringArray = []


static func default_path() -> String:
	var root := ProjectSettings.globalize_path("res://")
	return root.path_join(DEFAULT_RELATIVE).simplify_path()


func load_default() -> bool:
	return load_file(default_path())


func load_file(path: String) -> bool:
	pages.clear()
	items.clear()
	errors.clear()
	if not FileAccess.file_exists(path):
		errors.append("注册表不存在：%s" % path)
		return false
	var text := FileAccess.get_file_as_string(path)
	var data: Variant = JSON.parse_string(text)
	if typeof(data) != TYPE_DICTIONARY:
		errors.append("注册表顶层必须是对象：%s" % path)
		return false
	return load_dict(data)


## 解析并校验整表；返回是否有错（errors 里是 `页/组/项:原因`）。
func load_dict(data: Dictionary) -> bool:
	pages.clear()
	items.clear()
	errors.clear()
	if str(data.get("schema", "")) != SCHEMA:
		errors.append("registry:schema 必须是 %s" % SCHEMA)
	var raw_pages: Variant = data.get("pages", [])
	if typeof(raw_pages) != TYPE_ARRAY:
		errors.append("registry:pages 必须是数组")
		return false
	for raw_page: Variant in raw_pages:
		if typeof(raw_page) != TYPE_DICTIONARY:
			continue
		_load_page(raw_page as Dictionary)
	if pages.size() < MIN_PAGES:
		errors.append("registry:页面 %d < %d" % [pages.size(), MIN_PAGES])
	if items.size() < MIN_ITEMS:
		errors.append("registry:设置项 %d < %d" % [items.size(), MIN_ITEMS])
	return errors.is_empty()


func _load_page(page: Dictionary) -> void:
	var page_id := str(page.get("id", ""))
	if page_id.is_empty():
		errors.append("registry:页面 id 缺失")
		return
	var page_title := str(page.get("title", ""))
	var groups: Array[Dictionary] = []
	for raw_group: Variant in page.get("groups", []):
		if typeof(raw_group) != TYPE_DICTIONARY:
			continue
		var group: Dictionary = raw_group
		var group_id := str(group.get("id", ""))
		var group_items: Array[Dictionary] = []
		for raw_item: Variant in group.get("items", []):
			if typeof(raw_item) != TYPE_DICTIONARY:
				continue
			var item: Dictionary = _normalize_item(page_id, group_id, raw_item as Dictionary)
			if item.is_empty():
				continue
			items[str(item["id"])] = item
			group_items.append(item)
		groups.append({
			"id": group_id,
			"title": str(group.get("title", group_id)),
			"items": group_items,
		})
	pages.append({"id": page_id, "title": page_title,
			"icon": str(page.get("icon", "settings")), "groups": groups})


func _normalize_item(page_id: String, group_id: String, raw: Dictionary) -> Dictionary:
	var item_id := str(raw.get("id", ""))
	var where := "%s/%s/%s" % [page_id, group_id, item_id if not item_id.is_empty() else "?"]
	if item_id.is_empty() or not item_id.begins_with(page_id + "."):
		errors.append("%s:id 缺失或前缀不等于页面 id" % where)
		return {}
	if items.has(item_id):
		errors.append("%s:id 重复" % where)
		return {}
	var type_name := str(raw.get("type", ""))
	if not VALID_TYPES.has(type_name):
		errors.append("%s:未知 type %s" % [where, type_name])
	if not VALID_APPLY.has(str(raw.get("apply", ""))):
		errors.append("%s:apply 非法" % where)
	if not VALID_TARGETS.has(str(raw.get("target", ""))):
		errors.append("%s:target 非法" % where)
	var default_value: Variant = coerce(type_name, raw.get("default"), raw)
	if default_value == null:
		errors.append("%s:默认值非法" % where)
		return {}
	var item := {
		"id": item_id,
		"label": str(raw.get("label", item_id)),
		"type": type_name,
		"default": default_value,
		"apply": str(raw.get("apply", "immediate")),
		"target": str(raw.get("target", "hud")),
		"page": page_id,
		"group": group_id,
		"min": float(raw.get("min", 0.0)),
		"max": float(raw.get("max", 0.0)),
		"step": float(raw.get("step", 0.0)),
		"values": raw.get("values", []),
		"unit": str(raw.get("unit", "")),
		"max_length": int(raw.get("max_length", 0)),
	}
	if type_name == "slider" and (item["step"] <= 0.0 or item["max"] < item["min"]):
		errors.append("%s:slider 范围非法" % where)
	return item


## 类型归一：非法值返回 null（调用方据此回退默认/当前值）。
func coerce(type_name: String, value: Variant, spec: Dictionary = {}) -> Variant:
	match type_name:
		"bool":
			return value if typeof(value) == TYPE_BOOL else null
		"enum":
			var values: Array = spec.get("values", [])
			if typeof(value) == TYPE_STRING and values.has(value):
				return str(value)
			return null
		"slider":
			if typeof(value) != TYPE_FLOAT and typeof(value) != TYPE_INT:
				return null
			var number := float(value)
			if is_nan(number) or is_inf(number):
				return null
			var low := float(spec.get("min", 0.0))
			var high := float(spec.get("max", 0.0))
			var step := float(spec.get("step", 0.0))
			number = clampf(number, low, high)
			if step > 0.0:
				number = low + roundf((number - low) / step) * step
			return clampf(number, low, high)
		"keybind":
			if typeof(value) == TYPE_STRING and not str(value).strip_edges().is_empty():
				return str(value).strip_edges().to_upper()
			return null
		"color":
			if typeof(value) != TYPE_STRING:
				return null
			var raw := str(value).strip_edges()
			if not raw.begins_with("#") or raw.length() < 4:
				return null
			var color := Color.from_string(raw, Color(-1.0, -1.0, -1.0))
			return "#" + color.to_html(false) if color.r >= 0.0 else null
		"text":
			if typeof(value) != TYPE_STRING:
				return null
			var limit := int(spec.get("max_length", 0))
			var text := str(value)
			return text.substr(0, limit) if limit > 0 else text
		_:
			return null


func page_ids() -> Array[String]:
	var ids: Array[String] = []
	for page: Dictionary in pages:
		ids.append(str(page["id"]))
	return ids


func page(page_id: String) -> Dictionary:
	for item: Dictionary in pages:
		if str(item["id"]) == page_id:
			return item
	return {}


func item(item_id: String) -> Dictionary:
	var found: Variant = items.get(item_id, {})
	return found if typeof(found) == TYPE_DICTIONARY else {}


func default_of(item_id: String) -> Variant:
	var spec := item(item_id)
	return spec.get("default", null) if not spec.is_empty() else null


func item_ids() -> Array[String]:
	var ids: Array[String] = []
	for key: String in items.keys():
		ids.append(key)
	ids.sort()
	return ids


## 搜索：id 子串或中文名子串（忽略大小写）。
func search(query: String) -> Array[String]:
	var needle := query.strip_edges().to_lower()
	var found: Array[String] = []
	for item_id: String in item_ids():
		var spec: Dictionary = items[item_id]
		if needle.is_empty() or item_id.to_lower().contains(needle) \
				or str(spec["label"]).to_lower().contains(needle):
			found.append(item_id)
	return found
