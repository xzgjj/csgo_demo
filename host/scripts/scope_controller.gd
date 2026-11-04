class_name ScopeController
extends RefCounted
## 开镜/机瞄表现层：倍率优先取内核武器倍镜，取不到时回退表现层武器表；
## 负责 overlay 显隐、倍率文字与 FOV 目标值（只读状态，不做规则判断）。

const IRON_MAGNIFICATION := 1.15  # 机瞄轻微收窄（AK/M4）
const FOV_LERP_RATE := 0.25
const ScopeOverlayScript = preload("res://scripts/scope_overlay.gd")

var overlay
var aiming := false
var magnification := 1.0
var weapon_name := ""


func build(parent: Node) -> void:
	overlay = ScopeOverlayScript.new()
	parent.add_child(overlay)
	overlay.build()


## 每帧同步：瞄准状态 + 当前武器倍率 + 武器名。
func sync(aiming_now: bool, magnification_now: float, name_now: String) -> void:
	aiming = aiming_now
	magnification = magnification_now
	weapon_name = name_now
	if overlay == null:
		return
	overlay.visible = aiming
	overlay.set_scope(magnification, weapon_name)


func is_scoped() -> bool:
	return aiming and magnification > 1.0


func target_fov(base_fov: float) -> float:
	if not aiming:
		return base_fov
	if magnification > 1.0:
		return base_fov / magnification
	return base_fov / IRON_MAGNIFICATION


func fps_step(current_fov: float, base_fov: float) -> float:
	return lerpf(current_fov, target_fov(base_fov), FOV_LERP_RATE)
