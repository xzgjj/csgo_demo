extends SceneTree
## 输入契约回归（headless CLI）：WASD/跳/换弹/切枪/右键瞄准/F5 视角/F3 调试。
## 用法：godot --headless --path host --script res://tests/input_smoke.gd
## 失败时输出 INPUT_FAIL 行并以非 0 退出；run_all.py / CI 可据此断言。

var main: Node
var failures: Array[String] = []
var player_id := 0


func _initialize() -> void:
	main = load("res://main.tscn").instantiate()
	root.add_child(main)
	_run.call_deferred()


func _run() -> void:
	await process_frame
	main.call("_start_play")
	await _check_move()
	await _check_jump()
	await _check_weapon_switch()
	await _check_aim()
	await _check_view_toggle()
	await _check_debug_toggle()
	await _check_reload()
	await _check_combat_fx()
	if failures.is_empty():
		print("INPUT_SMOKE PASS")
		quit(0)
	else:
		for item in failures:
			print("INPUT_FAIL ", item)
		quit(1)


func _check_move() -> void:
	var start := Vector2(float(main.sim.get_x(player_id)), float(main.sim.get_y(player_id)))
	Input.action_press("move_forward")
	await physics_frame
	await physics_frame
	for i in 20:
		await physics_frame
	Input.action_release("move_forward")
	var moved := Vector2(float(main.sim.get_x(player_id)), float(main.sim.get_y(player_id)))
	if moved.distance_to(start) < 40.0:
		failures.append("WASD 未移动：%s -> %s" % [start, moved])


func _check_jump() -> void:
	var z0 := float(main.sim.get_z(player_id))
	Input.action_press("jump")
	await physics_frame
	Input.action_release("jump")
	var peak := z0
	for i in 24:
		await physics_frame
		peak = maxf(peak, float(main.sim.get_z(player_id)))
	if peak < z0 + 2.0:
		failures.append("Space 未起跳：z %.1f -> %.1f" % [z0, peak])


func _press_key(code: Key) -> void:
	var event := InputEventKey.new()
	event.physical_keycode = code
	event.keycode = code
	event.pressed = true
	Input.parse_input_event(event)


func _release_key(code: Key) -> void:
	var event := InputEventKey.new()
	event.physical_keycode = code
	event.keycode = code
	event.pressed = false
	Input.parse_input_event(event)


func _press_mouse(button: MouseButton) -> void:
	var event := InputEventMouseButton.new()
	event.button_index = button
	event.pressed = true
	Input.parse_input_event(event)


func _release_mouse(button: MouseButton) -> void:
	var event := InputEventMouseButton.new()
	event.button_index = button
	event.pressed = false
	Input.parse_input_event(event)


func _check_weapon_switch() -> void:
	_press_key(KEY_3)
	await physics_frame
	await physics_frame
	_release_key(KEY_3)
	if main.viewmodel.current_id != "awp":
		failures.append("3 未切到 AWP：%s" % main.viewmodel.current_id)
	_press_key(KEY_1)
	await physics_frame
	await physics_frame
	_release_key(KEY_1)
	if main.viewmodel.current_id != "ak":
		failures.append("1 未切回 AK：%s" % main.viewmodel.current_id)


func _check_aim() -> void:
	var base_fov: float = float(main.settings.fov)
	_press_mouse(MOUSE_BUTTON_RIGHT)
	for i in 10:
		await physics_frame
	if float(main.camera.fov) >= base_fov - 3.0:
		failures.append("右键瞄准 FOV 未收窄：%.1f (base %.1f)" % [main.camera.fov, base_fov])
	_release_mouse(MOUSE_BUTTON_RIGHT)
	for i in 10:
		await physics_frame


func _check_view_toggle() -> void:
	_press_key(KEY_F5)
	await physics_frame
	await physics_frame
	_release_key(KEY_F5)
	if not bool(main.third_person):
		failures.append("F5 未切到第三人称")
	await physics_frame
	_press_key(KEY_F5)
	await physics_frame
	await physics_frame
	_release_key(KEY_F5)
	if bool(main.third_person):
		failures.append("F5 未切回第一人称")


func _check_debug_toggle() -> void:
	_press_key(KEY_F3)
	await physics_frame
	await physics_frame
	_release_key(KEY_F3)
	if not bool(main.debug_visible):
		failures.append("F3 未打开调试信息")
	_press_key(KEY_F3)
	await physics_frame
	await physics_frame
	_release_key(KEY_F3)


func _check_reload() -> void:
	main.sim.fire(player_id, 0.0, 0.0)
	await physics_frame
	var mag_before := int(main.sim.get_mag(player_id))
	_press_key(KEY_R)
	await physics_frame
	await physics_frame
	_release_key(KEY_R)
	for i in 170:
		await physics_frame
	if int(main.sim.get_mag(player_id)) != 30:
		failures.append("R 换弹未生效：mag %d -> %d" % [mag_before, main.sim.get_mag(player_id)])


func _check_combat_fx() -> void:
	main.sim.set_player_position(0, 0.0, 0.0, 0.0)
	var tx := float(main.sim.get_x(1))
	var ty := float(main.sim.get_y(1))
	main.yaw = atan2(ty, tx) * 180.0 / PI
	main.pitch = 0.0
	var shot_before: int = int(main.fx.shot_count)
	var hit_before: int = int(main.fx.hit_count)
	_press_mouse(MOUSE_BUTTON_LEFT)
	for i in 3:
		await physics_frame
	_release_mouse(MOUSE_BUTTON_LEFT)
	for i in 3:
		await physics_frame
	if int(main.fx.shot_count) <= shot_before:
		failures.append("开火未触发枪声（shot_count 未增加）")
	if int(main.fx.hit_count) <= hit_before:
		failures.append("命中未触发命中音/火花（dmg=%d last=%d hit=%d target_alive=%s hp=%d）" % [int(main.sim.get_total_damage_events()), int(main._last_damage), int(main.fx.hit_count), str(main.sim.is_alive(1)), int(main.sim.get_health(1))])
	if not bool(main.fx.spark.emitting):
		failures.append("命中火花未发射")
