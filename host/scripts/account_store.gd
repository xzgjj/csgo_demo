class_name AccountStore
extends RefCounted
## 账号与装备持久化（user://account.json）：昵称、loadout、设置引用、统计。
## 版本号 + 字段默认值，后续加字段通过 migrate 补全，兼容旧存档。

const PATH := "user://account.json"
const VERSION := 1

var display_name := "Recruit"
var loadout := {"primary": "ak", "secondary": "m4", "knife": "knife"}
var stats := {"matches": 0, "kills": 0, "deaths": 0}


func load_account() -> void:
	if not FileAccess.file_exists(PATH):
		return
	var file := FileAccess.open(PATH, FileAccess.READ)
	if file == null:
		return
	var parsed: Variant = JSON.parse_string(file.get_as_text())
	if not parsed is Dictionary:
		return
	display_name = str(parsed.get("display_name", display_name))
	if parsed.get("loadout") is Dictionary:
		for key in loadout.keys():
			loadout[key] = str(parsed["loadout"].get(key, loadout[key]))
	if parsed.get("stats") is Dictionary:
		for key in stats.keys():
			stats[key] = int(parsed["stats"].get(key, stats[key]))


func save_account() -> void:
	var payload := {"version": VERSION, "display_name": display_name,
		"loadout": loadout, "stats": stats}
	var file := FileAccess.open(PATH, FileAccess.WRITE)
	if file == null:
		return
	file.store_string(JSON.stringify(payload, "\t"))


func set_primary(weapon_id: String) -> void:
	loadout["primary"] = weapon_id
	save_account()


func record_match(kills: int, deaths: int) -> void:
	stats["matches"] = int(stats["matches"]) + 1
	stats["kills"] = int(stats["kills"]) + kills
	stats["deaths"] = int(stats["deaths"]) + deaths
	save_account()
