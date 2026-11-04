extends SceneTree

## 最小冒烟：扩展类可实例化、能推进 tick、状态/事件可读。
func _initialize() -> void:
	var sim = RecoilSim.new()
	sim.reset()
	var start_x: float = sim.get_x(0)
	sim.set_move(0, 0.0, 0.0, 1.0, 0.0, false, false)
	for i in 64:
		sim.tick()
	var moved: float = sim.get_x(0) - start_x

	# 目标在 +X 方向 400u；开火应命中并产生事件。
	sim.fire(0, 0.0, 0.0)
	sim.tick()
	var damage_events: int = sim.get_last_damage_events()
	var fire_events: int = sim.get_last_fire_events()
	var target_hp: int = sim.get_health(1)
	print("SMOKE moved=%.2f hp0=%d mag=%d hp1=%d dmg_events=%d fire_events=%d tick=%d" % [
		moved, sim.get_health(0), sim.get_mag(0), target_hp,
		damage_events, fire_events, sim.get_tick(),
	])
	var ok: bool = moved > 100.0 and target_hp == 72 and damage_events == 1 and fire_events == 1
	quit(0 if ok else 1)
