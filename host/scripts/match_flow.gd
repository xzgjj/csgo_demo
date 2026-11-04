class_name MatchFlow
extends RefCounted
## 回合/比赛阶段 → 结算页（只读 sim；写 UI 页面与暂停态，不改规则状态）。
## 从 main.gd 拆出，避免主脚本超过 CI 400 行预算。

const TICKS_PER_SECOND := 64.0

var main: Node
var last_phase := -1
var last_score_t := 0
var last_score_ct := 0


func setup(main_node: Node) -> void:
	main = main_node


func reset() -> void:
	last_phase = -1
	last_score_t = 0
	last_score_ct = 0


func update(sim: RefCounted) -> void:
	var phase: int = sim.get_round_phase()
	if phase != last_phase:
		_handle_phase_change(sim, last_phase, phase)
		last_phase = phase
	last_score_t = sim.get_score_t()
	last_score_ct = sim.get_score_ct()
	if main.result_page.visible and main.result_page.mode == "round":
		main.result_page.update_countdown(float(sim.get_round_ticks()) / TICKS_PER_SECOND)


func _handle_phase_change(sim: RefCounted, old_phase: int, new_phase: int) -> void:
	if new_phase == 2:
		main.result_page.show_round_result(_round_winner(sim), sim.get_score_t(),
				sim.get_score_ct(), sim.get_kills(0), sim.get_money(0),
				float(sim.get_round_ticks()) / TICKS_PER_SECOND)
	elif new_phase == 3:
		_enter_match_end(sim)
	elif new_phase == 0 and old_phase >= 2:
		main.result_page.hide_result()


func _round_winner(sim: RefCounted) -> String:
	if sim.get_score_t() > last_score_t:
		return "T"
	if sim.get_score_ct() > last_score_ct:
		return "CT"
	return "—"


func _enter_match_end(sim: RefCounted) -> void:
	main.playing = false
	main.paused = false
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	var winner := "T" if sim.get_score_t() > sim.get_score_ct() else "CT"
	main.result_page.show_match_result(winner, sim.get_score_t(), sim.get_score_ct(),
			sim.get_kills(0), sim.get_money(0), sim.get_score_t() + sim.get_score_ct())
	# 模块 10：比赛结束写账号统计（个人击杀/阵亡 + 场次/胜场）。
	if main.account != null:
		var player_team: int = int(sim.get_team(0))
		var won: bool = (winner == "T" and player_team == 1) or (winner == "CT" and player_team == 2)
		main.account.record_match(int(main.hud.kill_total), int(main.hud.death_total), won)
