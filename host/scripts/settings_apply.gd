class_name SettingsApplier
extends RefCounted
## 热应用分发（SPEC-05 §5.4.3）：把 `target` 映射到具体动作，自身不持有节点、不做规则。
## 目标按 sink 注册：main/_ready 里注册真实动作，单测里注册探针 Callable，两边行为一致。
## apply 语义：immediate 由 store.changed 同步调用；next_round/restart 只记录，等待调用方在
## 回合开始/重启时用 apply_all() 补齐（UI 会标记“重启生效”）。

const TARGETS: Array[String] = [
	"window", "renderer", "env_adjustment", "camera", "audio", "input", "hud",
	"gameplay", "account",
]

var sinks: Dictionary = {}
var applied: Dictionary = {}
var modes: Dictionary = {}
var counts: Dictionary = {}
var deferred: Dictionary = {}


func register(target: String, sink: Callable) -> bool:
	if not TARGETS.has(target) or not sink.is_valid():
		return false
	sinks[target] = sink
	counts[target] = int(counts.get(target, 0))
	return true


## 单项分发；没有 sink 时返回 false（仍记录，便于 UI 显示未接线的 target）。
func apply(item_id: String, value: Variant, target: String) -> bool:
	applied[item_id] = value
	if str(modes.get(item_id, "immediate")) != "immediate":
		deferred[item_id] = value
	var sink: Variant = sinks.get(target, null)
	if sink is Callable and (sink as Callable).is_valid():
		(sink as Callable).call(item_id, value)
		counts[target] = int(counts.get(target, 0)) + 1
		return true
	return false


## store.changed 的统一入口：apply 字段决定“立即/下回合/重启”。
func on_changed(item_id: String, value: Variant, target: String, apply_mode: String) -> void:
	modes[item_id] = apply_mode
	apply(item_id, value, target)


## 全量重放（启动/应用兜底/回合开始）：注册表顺序保证依赖顺序稳定。
func apply_all(store: SettingsStore) -> int:
	var total := 0
	for item_id: String in store.registry.item_ids():
		var spec := store.registry.item(item_id)
		if apply(item_id, store.get_value(item_id), str(spec["target"])):
			total += 1
	return total


func count_for(target: String) -> int:
	return int(counts.get(target, 0))


func last_value(item_id: String) -> Variant:
	return applied.get(item_id, null)


func deferred_items() -> Array[String]:
	var ids: Array[String] = []
	for key: String in deferred.keys():
		ids.append(key)
	ids.sort()
	return ids


func clear_deferred() -> void:
	deferred.clear()
