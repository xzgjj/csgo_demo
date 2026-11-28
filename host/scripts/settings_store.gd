class_name SettingsStore
extends RefCounted
## 设置单一事实源（SPEC-05 §5.4.2/§5.4.3）：注册表 + 值 + 持久化(v2) + 迁移 + 钳制 + 热应用信号。
## 磁盘格式：user://settings.cfg，[meta] version=2，其余 section=页面 id、key=完整项 id。
## 所有磁盘读入值一律类型判断 + 范围钳制（注册表 coerce）；非法值回退默认，未知键保留到 [legacy]。
## 本类不直接操作相机/音频/窗口：改值只发 changed 信号，由 SettingsApplier 分发给各 target。

signal changed(id: String, value: Variant, target: String, apply: String)
signal save_failed(reason: String)

const CONFIG_VERSION := 2
const META_SECTION := "meta"
const LEGACY_SECTION := "legacy"
const LEGACY_PLAYER := "player"
const SAVE_DEBOUNCE_SECONDS := 0.5
const DEFAULT_PATH := "user://settings.cfg"
## 兼容旧调用方（迁移前的三项设置）。
const FOV_MIN := 60.0
const FOV_MAX := 110.0
const SENSITIVITY_MIN := 0.006
const SENSITIVITY_MAX := 0.08
const VOLUME_MIN := 0
const VOLUME_MAX := 100

var registry: SettingsRegistry
var path := DEFAULT_PATH
var values: Dictionary = {}
var errors: PackedStringArray = []

var _defaults: Dictionary = {}
var _snapshot: Dictionary = {}
var _legacy: Dictionary = {}
var _dirty := false
var _dirty_seconds := 0.0
var _loaded_version := 0

var fov: float:
	get:
		return float(get_value("camera.fov"))
	set(value):
		set_value("camera.fov", value)

var sensitivity: float:
	get:
		return float(get_value("controls.mouse.sensitivity"))
	set(value):
		set_value("controls.mouse.sensitivity", value)

var volume: int:
	get:
		return int(get_value("audio.master.volume"))
	set(value):
		set_value("audio.master.volume", value)


func _init(settings_path: String = DEFAULT_PATH, registry_path: String = "") -> void:
	path = settings_path
	registry = SettingsRegistry.new()
	var ok := registry.load_default() if registry_path.is_empty() else registry.load_file(registry_path)
	for message: String in registry.errors:
		errors.append(message)
	if not ok:
		errors.append("注册表不可用，设置回退为空表")
	for item_id: String in registry.item_ids():
		_defaults[item_id] = registry.default_of(item_id)
		values[item_id] = _defaults[item_id]
	_snapshot = values.duplicate(true)


## 读取 user://settings.cfg；文件不存在不算错误（返回 false 但使用默认值）。
func load_settings() -> bool:
	var config := ConfigFile.new()
	if config.load(path) != OK:
		return false
	load_from(config)
	return true


## 从已加载 ConfigFile 吸收（测试注入非法/旧版配置也用这个入口）。
func load_from(config: ConfigFile) -> void:
	_legacy.clear()
	var version := int(config.get_value(META_SECTION, "version", 1))
	_loaded_version = version
	if version >= CONFIG_VERSION:
		_absorb_v2(config)
	else:
		_migrate_v1_to_v2(config)
	_snapshot = values.duplicate(true)
	_dirty = true
	_dirty_seconds = 0.0


## v1→v2：旧 [player] 的 fov/sensitivity/volume 映射到 camera/controls/audio；
## 其余未知键原样保留到 [legacy]，避免静默丢配置。
func _migrate_v1_to_v2(config: ConfigFile) -> void:
	_absorb_raw("camera.fov", _read(config, LEGACY_PLAYER, "fov"))
	_absorb_raw("controls.mouse.sensitivity",
			_read(config, LEGACY_PLAYER, "sensitivity"))
	_absorb_raw("audio.master.volume", _read(config, LEGACY_PLAYER, "volume"))
	if not config.has_section(LEGACY_PLAYER):
		return
	for key in config.get_section_keys(LEGACY_PLAYER):
		if key in ["version", "fov", "sensitivity", "volume"]:
			continue
		_legacy["%s.%s" % [LEGACY_PLAYER, key]] = config.get_value(LEGACY_PLAYER, key)


func _read(config: ConfigFile, section: String, key: String) -> Variant:
	return config.get_value(section, key) if config.has_section_key(section, key) else null


func _absorb_v2(config: ConfigFile) -> void:
	if config.has_section(LEGACY_SECTION):
		for key in config.get_section_keys(LEGACY_SECTION):
			_legacy[key] = config.get_value(LEGACY_SECTION, key)
	for item_id: String in registry.item_ids():
		var spec := registry.item(item_id)
		var section := str(spec.get("page", ""))
		if not config.has_section_key(section, item_id):
			continue
		_absorb_raw(item_id, config.get_value(section, item_id))
	for section in config.get_sections():
		if section in [META_SECTION, LEGACY_SECTION]:
			continue
		for key in config.get_section_keys(section):
			if not registry.items.has(key):
				_legacy["%s.%s" % [section, key]] = config.get_value(section, key)


func _absorb_raw(item_id: String, raw: Variant) -> void:
	if raw == null:
		return
	var spec := registry.item(item_id)
	if spec.is_empty():
		return
	var coerced: Variant = registry.coerce(str(spec["type"]), raw, spec)
	values[item_id] = coerced if coerced != null else _defaults[item_id]


func save_settings() -> bool:
	var config := ConfigFile.new()
	config.set_value(META_SECTION, "version", CONFIG_VERSION)
	for item_id: String in registry.item_ids():
		var spec := registry.item(item_id)
		config.set_value(str(spec.get("page", "misc")), item_id, values.get(item_id))
	for key in _legacy.keys():
		config.set_value(LEGACY_SECTION, key, _legacy[key])
	var ok := config.save(path) == OK
	if not ok:
		errors.append("写入失败：%s" % path)
		save_failed.emit(path)
		return false
	_dirty = false
	_dirty_seconds = 0.0
	return true


## 去抖：改动即写（0.5s 合并），调用方每帧传入 delta；退出时显式 flush。
func tick(delta: float) -> void:
	if not _dirty:
		return
	_dirty_seconds += delta
	if _dirty_seconds >= SAVE_DEBOUNCE_SECONDS:
		flush()


func flush() -> bool:
	return save_settings() if _dirty else true


func is_dirty() -> bool:
	return _dirty


func pending_save_seconds() -> float:
	return _dirty_seconds


func get_value(item_id: String) -> Variant:
	return values.get(item_id, _defaults.get(item_id, null))


## 校验 + 钳制 + 热应用信号；类型不符返回 false（不改值），越界按注册表钳制。
func set_value(item_id: String, value: Variant, persist := true) -> bool:
	var spec := registry.item(item_id)
	if spec.is_empty():
		return false
	var coerced: Variant = registry.coerce(str(spec["type"]), value, spec)
	if coerced == null:
		return false
	if _same_value(get_value(item_id), coerced):
		return true
	values[item_id] = coerced
	if persist:
		_dirty = true
		_dirty_seconds = 0.0
	changed.emit(item_id, coerced, str(spec["target"]), str(spec["apply"]))
	return true


func reset(item_id: String) -> void:
	if registry.items.has(item_id):
		set_value(item_id, registry.default_of(item_id))


func reset_all(emit_signal := true) -> void:
	for item_id: String in registry.item_ids():
		if emit_signal:
			reset(item_id)
		else:
			values[item_id] = _defaults[item_id]
	_snapshot = values.duplicate(true)


## 兼容旧 API：恢复默认（不落盘，调用方再 save）。
func reset_defaults() -> void:
	reset_all()


## 打开设置页时拍快照，撤销更改用。
func begin_edit() -> void:
	_snapshot = values.duplicate(true)


func undo() -> bool:
	if _snapshot.is_empty():
		return false
	var changed_any := false
	for item_id: String in registry.item_ids():
		if _same_value(get_value(item_id), _snapshot.get(item_id, _defaults[item_id])):
			continue
		set_value(item_id, _snapshot[item_id], false)
		changed_any = true
	return changed_any


func is_modified(item_id: String) -> bool:
	return not _same_value(get_value(item_id), _defaults.get(item_id, null))


func modified_count() -> int:
	var total := 0
	for item_id: String in registry.item_ids():
		if is_modified(item_id):
			total += 1
	return total


func legacy_values() -> Dictionary:
	return _legacy.duplicate(true)


func loaded_version() -> int:
	return _loaded_version


func _same_value(a: Variant, b: Variant) -> bool:
	if typeof(a) in [TYPE_FLOAT, TYPE_INT] and typeof(b) in [TYPE_FLOAT, TYPE_INT]:
		return is_equal_approx(float(a), float(b))
	return a == b

