class_name AccountStore
extends RefCounted
## 账号/装备/统计持久化（user://account.json，schema 版本化）：
## nickname、loadout{primary, secondary, knife}、stats{kills, deaths, matches, wins}。
## 存档来自磁盘属不可信输入：读入一律做类型判断 + 白名单/范围钳制；缺失/损坏回退默认值不崩溃。
## 写入原子化：先写 <path>.tmp，成功后替换目标文件，避免半截 JSON。
## 路径可注入（测试用 user://account_smoke_test.json，不污染真实账号）。

const DEFAULT_PATH := "user://account.json"
const SCHEMA := "account/1"
const SCHEMA_VERSION := 1
const NICKNAME_DEFAULT := "Recruit"
const NICKNAME_MAX := 24

const PRIMARY_IDS: Array[String] = ["ak", "m4", "awp"]
const SECONDARY_IDS: Array[String] = ["usp", "glock"]
const STAT_KEYS: Array[String] = ["kills", "deaths", "matches", "wins"]
const DEFAULT_PRIMARY := "ak"
const DEFAULT_SECONDARY := "usp"

var path := DEFAULT_PATH
var nickname := NICKNAME_DEFAULT
var loadout := {"primary": DEFAULT_PRIMARY, "secondary": DEFAULT_SECONDARY, "knife": "knife"}
var stats := {"kills": 0, "deaths": 0, "matches": 0, "wins": 0}
## 上次读档是否成功（false = 文件缺失/损坏，已回退默认值）。
var last_load_ok := false

## 兼容旧调用方（input_smoke/旧 loadout_page）读取 account.display_name。
var display_name: String:
	get:
		return nickname
	set(value):
		nickname = _sanitize_nickname(value)


func _init(store_path: String = DEFAULT_PATH) -> void:
	path = store_path


func load_account() -> bool:
	reset_defaults()
	last_load_ok = false
	if not FileAccess.file_exists(path):
		return false
	var text := FileAccess.get_file_as_string(path)
	if text.is_empty():
		return false
	var parsed: Variant = JSON.parse_string(text)
	if not parsed is Dictionary:
		return false
	_absorb(parsed as Dictionary)
	last_load_ok = true
	return true


func save_account() -> bool:
	var payload := {
		"schema": SCHEMA,
		"schema_version": SCHEMA_VERSION,
		"nickname": nickname,
		"loadout": loadout.duplicate(),
		"stats": stats.duplicate(),
	}
	var tmp_path := path + ".tmp"
	var tmp_file := FileAccess.open(tmp_path, FileAccess.WRITE)
	if tmp_file == null:
		return false
	tmp_file.store_string(JSON.stringify(payload, "\t"))
	tmp_file.close()
	var global_path := ProjectSettings.globalize_path(path)
	var global_tmp := ProjectSettings.globalize_path(tmp_path)
	if global_path.is_empty() or global_tmp.is_empty():
		return false
	if FileAccess.file_exists(path):
		DirAccess.remove_absolute(global_path)
	return DirAccess.rename_absolute(global_tmp, global_path) == OK


func reset_defaults() -> void:
	nickname = NICKNAME_DEFAULT
	loadout = {"primary": DEFAULT_PRIMARY, "secondary": DEFAULT_SECONDARY, "knife": "knife"}
	stats = {"kills": 0, "deaths": 0, "matches": 0, "wins": 0}


func set_nickname(value: String) -> bool:
	nickname = _sanitize_nickname(value)
	return save_account()


func set_primary(weapon_id: String) -> bool:
	if not (weapon_id in PRIMARY_IDS):
		return false
	loadout["primary"] = weapon_id
	return save_account()


func set_secondary(weapon_id: String) -> bool:
	if not (weapon_id in SECONDARY_IDS):
		return false
	loadout["secondary"] = weapon_id
	return save_account()


## 一局结束：matches/wins 累加，kills/deaths 累加（负数按 0 处理）。
func record_match(kills: int, deaths: int, won: bool = false) -> bool:
	stats["matches"] = int(stats["matches"]) + 1
	stats["kills"] = int(stats["kills"]) + maxi(kills, 0)
	stats["deaths"] = int(stats["deaths"]) + maxi(deaths, 0)
	if won:
		stats["wins"] = int(stats["wins"]) + 1
	return save_account()


func add_kill(amount: int = 1) -> bool:
	stats["kills"] = int(stats["kills"]) + maxi(amount, 0)
	return save_account()


func add_death(amount: int = 1) -> bool:
	stats["deaths"] = int(stats["deaths"]) + maxi(amount, 0)
	return save_account()


func reset_stats() -> bool:
	stats = {"kills": 0, "deaths": 0, "matches": 0, "wins": 0}
	return save_account()


func kd_ratio() -> float:
	var deaths := int(stats["deaths"])
	if deaths <= 0:
		return float(stats["kills"])
	return float(stats["kills"]) / float(deaths)


## 从已解析的 JSON 对象吸收字段；非法值一律回退/钳制。
func _absorb(data: Dictionary) -> void:
	var raw_name: Variant = data.get("nickname", data.get("display_name", nickname))
	if raw_name is String:
		nickname = _sanitize_nickname(raw_name as String)
	var raw_loadout: Variant = data.get("loadout", {})
	if raw_loadout is Dictionary:
		var saved_loadout := raw_loadout as Dictionary
		loadout["primary"] = _pick_id(saved_loadout.get("primary", DEFAULT_PRIMARY),
				PRIMARY_IDS, DEFAULT_PRIMARY)
		loadout["secondary"] = _pick_id(saved_loadout.get("secondary", DEFAULT_SECONDARY),
				SECONDARY_IDS, DEFAULT_SECONDARY)
	var raw_stats: Variant = data.get("stats", {})
	if raw_stats is Dictionary:
		var saved_stats := raw_stats as Dictionary
		for key in STAT_KEYS:
			stats[key] = maxi(_int_or(saved_stats.get(key, 0), 0), 0)


func _sanitize_nickname(value: String) -> String:
	var trimmed := value.strip_edges()
	if trimmed.is_empty():
		return NICKNAME_DEFAULT
	return trimmed.substr(0, NICKNAME_MAX)


func _pick_id(raw: Variant, allowed: Array[String], fallback: String) -> String:
	var value := str(raw)
	if value in allowed:
		return value
	return fallback


## 数字类字段：接受 int/float/bool（Godot 里 JSON 数字都是 float），其余回退。
func _int_or(raw: Variant, fallback: int) -> int:
	if raw is int or raw is float or raw is bool:
		return int(raw)
	return fallback
