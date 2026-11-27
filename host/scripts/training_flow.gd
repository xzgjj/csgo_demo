class_name TrainingFlow
extends RefCounted
## 训练模式入口：与正式对战同一张地图；直接 `_start_play` 仍保留旧自由场给场景/golden。

const MapLoaderScript = preload("res://scripts/map_loader.gd")


static func start(main: Variant) -> bool:
	if not MapLoaderScript.start_training(main.sim, str(main.map_id)):
		main.call("_show_menu")
		return false
	main.call("_enter_game", false)
	return true
