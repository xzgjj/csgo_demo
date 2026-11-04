class_name ShotFeedback
extends RefCounted
## 开火/命中反馈归因（模块 2 回归）：表现层只按内核"真正开火 / 真正命中"的计数触发，
## 因此：空弹扣扳机不播枪声/不给后坐（只记 dryfire）；bot 交火不触发玩家自己的击中特效。

var sim: RefCounted
var viewmodel: Node
var fx: Node

var fire_pending := false
var fire_request_base := 0
var damage_seen := 0
var last_damage := 0


func setup(sim_ref: RefCounted, viewmodel_ref: Node, fx_ref: Node) -> void:
	sim = sim_ref
	viewmodel = viewmodel_ref
	fx = fx_ref


func reset() -> void:
	fire_pending = false
	fire_request_base = 0
	damage_seen = 0
	last_damage = 0


## 玩家按下开火时调用（命令入队后）：记录基线，等待 tick 后用增量判定是否真的开火。
func request_fire() -> void:
	fire_pending = true
	fire_request_base = int(sim.get_fire_count(0))


## 每次 tick 后调用：开火反馈 + 命中反馈。
func after_tick(camera: Camera3D) -> void:
	if fire_pending:
		fire_pending = false
		if int(sim.get_fire_count(0)) > fire_request_base:
			viewmodel.kick()
			fx.trigger_shot()
		elif int(sim.get_mag(0)) == 0:
			fx.trigger_dryfire()
	if not sim.has_method("get_damage_count_by"):
		return
	var damage_by := int(sim.get_damage_count_by(0))
	if damage_by > damage_seen:
		fx.trigger_hit(CombatFx.hit_point(sim, camera))
	damage_seen = damage_by
	last_damage = damage_by
