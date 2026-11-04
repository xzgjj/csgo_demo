extends SceneTree
func _initialize() -> void:
	var sim = RecoilSim.new()
	sim.reset()
	sim.fire(0, 0.0, 0.6)
	sim.tick()
	print("DEBUG headshot hp=", sim.get_health(1), " alive=", sim.is_alive(1), " mag=", sim.get_mag(0), " dmg_events=", sim.get_last_damage_events())
	quit(0)
