class_name ResultPage
extends CanvasLayer
## 结算页：回合结算（胜者/比分/击杀/金钱/下一回合倒计时）与比赛结算（最终胜者/统计/按钮）。
## 回合结算只读覆盖层，不抢鼠标，回合重置后自动收起；比赛结算停住对局并提供按钮。

var mode := ""
var title_label: Label
var score_label: Label
var stats_label: Label
var hint_label: Label
var continue_button: Button
var restart_button: Button
var menu_button: Button
var _countdown := 0.0


func build() -> void:
	layer = 3
	var box := UiFactory.build_card(self, "结算", 380.0)
	title_label = UiFactory.add_label(box, "", 30)
	score_label = UiFactory.add_label(box, "", 26)
	stats_label = UiFactory.add_label(box, "", 20)
	hint_label = UiFactory.add_label(box, "", 15)
	hint_label.add_theme_color_override("font_color", UiFactory.NEUTRAL_COLOR)
	restart_button = UiFactory.add_button(box, "再来一场")
	menu_button = UiFactory.add_button(box, "返回主菜单")
	visible = false


func show_round_result(winner: String, score_t: int, score_ct: int, kills: int, money: int,
		seconds_left: float) -> void:
	mode = "round"
	_countdown = seconds_left
	title_label.text = "回合结束 — %s 获胜" % winner
	score_label.text = "T %d : %d CT" % [score_t, score_ct]
	stats_label.text = "本局击杀 %d · 金钱 $%d" % [kills, money]
	_update_round_hint()
	restart_button.visible = false
	menu_button.visible = false
	visible = true


func show_match_result(winner: String, score_t: int, score_ct: int, kills: int, money: int,
		rounds: int) -> void:
	mode = "match"
	title_label.text = "比赛结束 — %s 胜利" % winner
	score_label.text = "T %d : %d CT" % [score_t, score_ct]
	stats_label.text = "击杀 %d · 金钱 $%d · 回合 %d" % [kills, money, rounds]
	hint_label.text = "Esc 或按钮返回主菜单"
	restart_button.visible = true
	menu_button.visible = true
	visible = true


func update_countdown(seconds_left: float) -> void:
	if mode != "round":
		return
	_countdown = seconds_left
	_update_round_hint()


func hide_result() -> void:
	mode = ""
	visible = false


func summary_text() -> String:
	return "%s | %s | %s" % [title_label.text, score_label.text, stats_label.text]


func _update_round_hint() -> void:
	hint_label.text = "下一回合 %.1f 秒后开始" % maxf(_countdown, 0.0)
