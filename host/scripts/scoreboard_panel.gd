class_name ScoreboardPanel
extends CanvasLayer
## Tab 比分板：双方队伍、比分，以及每名玩家的 HP / 护甲 / 击杀 / 金钱 / 存活状态。
## 玩家数取 UiFactory.player_count（内核补 get_player_count 后自动扩展）。

const GRID_COLUMNS := 7
const CELL_FONT_SIZE := 17
const COLUMN_TITLES: Array[String] = ["队伍", "玩家", "HP", "护甲", "击杀", "金钱", "状态"]

var score_label: Label
var grid: GridContainer
var row_count := 0
var _cells: Array = []


func build() -> void:
	layer = 2
	var box := UiFactory.build_panel(self, Vector2(720, 320))
	UiFactory.add_label(box, "比分板 · Tab 关闭", 22)
	score_label = UiFactory.add_label(box, "T 0 : 0 CT", 30)
	grid = GridContainer.new()
	grid.columns = GRID_COLUMNS
	grid.add_theme_constant_override("h_separation", 24)
	grid.add_theme_constant_override("v_separation", 6)
	grid.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	box.add_child(grid)
	for title: String in COLUMN_TITLES:
		var header := Label.new()
		header.text = title
		header.add_theme_font_size_override("font_size", 16)
		header.add_theme_color_override("font_color", UiFactory.NEUTRAL_COLOR)
		header.custom_minimum_size = Vector2(72, 22)
		grid.add_child(header)
	visible = false


func refresh(sim: RefCounted) -> void:
	if sim == null:
		return
	var count := UiFactory.player_count(sim)
	_ensure_rows(count)
	score_label.text = "T %d : %d CT" % [sim.get_score_t(), sim.get_score_ct()]
	for index in _cells.size():
		var cells: Array = _cells[index]
		var active := index < count
		for cell: Label in cells:
			cell.visible = active
		if active:
			_fill_row(cells, sim, index)


## 供 smoke 断言：把当前比分板内容拼成文本行。
func row_texts() -> Array[String]:
	var lines: Array[String] = [score_label.text]
	for cells: Array in _cells:
		var parts: PackedStringArray = []
		for cell: Label in cells:
			parts.append(cell.text)
		lines.append(" ".join(parts))
	return lines


func _ensure_rows(count: int) -> void:
	while _cells.size() < count:
		var cells: Array = []
		for column in GRID_COLUMNS:
			var cell := Label.new()
			cell.add_theme_font_size_override("font_size", CELL_FONT_SIZE)
			cell.custom_minimum_size = Vector2(72, 22)
			grid.add_child(cell)
			cells.append(cell)
		_cells.append(cells)
	row_count = _cells.size()


func _fill_row(cells: Array, sim: RefCounted, index: int) -> void:
	var team_cell: Label = cells[0]
	team_cell.text = UiFactory.team_name(sim, index)
	team_cell.add_theme_color_override("font_color", UiFactory.team_color(sim, index))
	var name_cell: Label = cells[1]
	if index == 0:
		name_cell.text = "P0（你）"
	else:
		name_cell.text = "P%d" % index
	var health_cell: Label = cells[2]
	health_cell.text = str(sim.get_health(index))
	var armor_cell: Label = cells[3]
	armor_cell.text = str(sim.get_armor(index))
	var kill_cell: Label = cells[4]
	kill_cell.text = str(sim.get_kills(index))
	var money_cell: Label = cells[5]
	money_cell.text = "$%d" % sim.get_money(index)
	var state_cell: Label = cells[6]
	state_cell.text = "存活" if sim.is_alive(index) else "阵亡"
	state_cell.add_theme_color_override("font_color",
			UiFactory.NEUTRAL_COLOR if sim.is_alive(index) else Color(0.85, 0.35, 0.30))
