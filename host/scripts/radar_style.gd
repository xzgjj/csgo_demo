class_name RadarStyle
extends RefCounted
## 雷达绘制的纯函数：盒 → sim 平面矩形、kind → 颜色。
## 从 radar.gd 拆出（文件长度约束），不持有状态、不读节点。

## 一个盒子在 sim x/y 平面的范围：Godot 中心/尺寸 → sim（x=-gz, y=-gx）。
static func box_sim_extent(item: Dictionary) -> Rect2:
	var center: Array = item["center"]
	var size: Array = item["size"]
	var gx := float(center[0])
	var gz := float(center[2])
	var sx := float(size[0])
	var sz := float(size[2])
	return Rect2(Vector2(-gz - sz * 0.5, -gx - sx * 0.5), Vector2(sz, sx))


## kind → 颜色；skip_kinds 内的 kind 返回全透明（不画）。
static func kind_color(kind: String, skip_kinds: PackedStringArray,
		site_color: Color, spawn_color: Color, prop_color: Color,
		stair_color: Color, wall_color: Color) -> Color:
	if kind in skip_kinds:
		return Color(0.0, 0.0, 0.0, 0.0)
	if kind == "site":
		return site_color
	if kind == "spawn":
		return spawn_color
	if kind == "prop":
		return prop_color
	if kind == "stairs" or kind == "ramp":
		return stair_color
	return wall_color
