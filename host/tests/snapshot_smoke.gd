extends SceneTree
## 快照一致性冒烟：get_snapshot() 解码结果必须与逐字段 getter 完全一致（headless 退出码 0/1）。
## 覆盖：reset 初态、移动 20 tick 后、开火后；另验证数组长度与越界读取。

var failures: Array[String] = []


func _initialize() -> void:
	var sim = RecoilSim.new()
	sim.reset()
	_check_all(sim, "reset")
	sim.set_move(0, 0.0, 0.0, 1.0, 0.0, false, false)
	for i in 20:
		sim.tick()
	_check_all(sim, "after-20tick")
	sim.fire(0, 0.0, 0.0)
	sim.tick()
	_check_all(sim, "after-fire")
	_check_bounds(sim)
	if failures.is_empty():
		print("SNAPSHOT SMOKE PASS")
	quit(0 if failures.is_empty() else 1)


func _check_all(sim, label: String) -> void:
	var snap := SimSnapshot.decode(sim.get_snapshot())
	_expect(snap.tick == int(sim.get_tick()), "%s tick %d != %d" % [label, snap.tick, sim.get_tick()])
	_expect(snap.player_count == int(sim.get_player_count()),
			"%s player_count" % label)
	_expect(snap.stride == SimSnapshot.STRIDE, "%s stride" % label)
	_expect(snap.players.size() == snap.player_count * snap.stride,
			"%s players.size=%d" % [label, snap.players.size()])
	_expect(snap.round_phase == int(sim.get_round_phase()), "%s round_phase" % label)
	_expect(snap.score_t == int(sim.get_score_t()), "%s score_t" % label)
	_expect(snap.score_ct == int(sim.get_score_ct()), "%s score_ct" % label)
	_expect(snap.smoke_active == (int(sim.get_smoke_active()) != 0), "%s smoke_active" % label)
	_expect(is_equal_approx(snap.smoke_x, float(sim.get_smoke_x())), "%s smoke_x" % label)
	_expect(is_equal_approx(snap.smoke_y, float(sim.get_smoke_y())), "%s smoke_y" % label)
	_expect(snap.bomb_timer == int(sim.get_bomb_timer()), "%s bomb_timer" % label)
	for i in snap.player_count:
		_expect(is_equal_approx(snap.x(i), float(sim.get_x(i))), "%s p%d x" % [label, i])
		_expect(is_equal_approx(snap.y(i), float(sim.get_y(i))), "%s p%d y" % [label, i])
		_expect(is_equal_approx(snap.z(i), float(sim.get_z(i))), "%s p%d z" % [label, i])
		_expect(is_equal_approx(snap.yaw(i), float(sim.get_view_yaw(i))), "%s p%d yaw" % [label, i])
		_expect(snap.health(i) == int(sim.get_health(i)), "%s p%d health" % [label, i])
		_expect(snap.armor(i) == int(sim.get_armor(i)), "%s p%d armor" % [label, i])
		_expect(snap.mag(i) == int(sim.get_mag(i)), "%s p%d mag" % [label, i])
		_expect(snap.reserve(i) == int(sim.get_reserve(i)), "%s p%d reserve" % [label, i])
		_expect(snap.money(i) == int(sim.get_money(i)), "%s p%d money" % [label, i])
		_expect(snap.alive(i) == bool(sim.is_alive(i)), "%s p%d alive" % [label, i])
		_expect(snap.team(i) == int(sim.get_team(i)), "%s p%d team" % [label, i])
		_expect(snap.weapon_id(i) == int(sim.get_weapon_id(i)), "%s p%d weapon" % [label, i])
		_expect(snap.blind_ticks(i) == int(sim.get_blind_ticks(i)),
				"%s p%d blind" % [label, i])


func _check_bounds(sim) -> void:
	var snap := SimSnapshot.decode(sim.get_snapshot())
	# 越界读取必须返回 0，而不是崩溃。
	_expect(snap.value(snap.player_count, SimSnapshot.FX) == 0.0, "越界玩家应返回 0")
	_expect(snap.value(-1, SimSnapshot.FX) == 0.0, "负索引应返回 0")
	# 世界坐标映射：-y 为前、-x 为右。
	var world := snap.world_position(0)
	_expect(is_equal_approx(world.x, -snap.y(0)), "world x = -sim y")
	_expect(is_equal_approx(world.z, -snap.x(0)), "world z = -sim x")


func _expect(condition: bool, message: String) -> void:
	if not condition:
		failures.append(message)
