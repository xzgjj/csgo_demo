class_name SimSnapshot
extends RefCounted
## 一帧只读快照的解码器，对应 RecoilSim.get_snapshot()。
## 目的：每帧一次跨语言调用，替代逐字段 get_* 拉取；下标与 core/include/recoil/sim/snapshot.h 对齐。

const STRIDE := 24
const FX := 0
const FY := 1
const FZ := 2
const FVX := 3
const FVY := 4
const FVZ := 5
const FYAW := 6
const FPITCH := 7
const FRECOIL_PITCH := 8
const FRECOIL_YAW := 9
const FHEALTH := 10
const FARMOR := 11
const FALIVE := 12
const FDUCKING := 13
const FTEAM := 14
const FWEAPON := 15
const FPRIMARY := 16
const FSECONDARY := 17
const FMAG := 18
const FRESERVE := 19
const FMONEY := 20
const FBLIND := 21
const FINACCURACY := 22

var tick: int = 0
var player_count: int = 0
var stride: int = STRIDE
var round_number: int = 0
var round_phase: int = 0
var phase_ticks: int = 0
var score_t: int = 0
var score_ct: int = 0
var bomb_phase: int = 0
var bomb_timer: int = 0
var smoke_active: bool = false
var smoke_x: float = 0.0
var smoke_y: float = 0.0
var smoke_radius: float = 0.0
var smoke_ticks_left: int = 0
var players := PackedFloat32Array()


static func decode(raw: Dictionary) -> SimSnapshot:
	var snap := SimSnapshot.new()
	snap.tick = int(raw.get("tick", 0))
	snap.player_count = int(raw.get("player_count", 0))
	snap.stride = int(raw.get("stride", STRIDE))
	snap.round_number = int(raw.get("round_number", 0))
	snap.round_phase = int(raw.get("round_phase", 0))
	snap.phase_ticks = int(raw.get("phase_ticks", 0))
	snap.score_t = int(raw.get("score_t", 0))
	snap.score_ct = int(raw.get("score_ct", 0))
	snap.bomb_phase = int(raw.get("bomb_phase", 0))
	snap.bomb_timer = int(raw.get("bomb_timer", 0))
	snap.smoke_active = int(raw.get("smoke_active", 0)) != 0
	snap.smoke_x = float(raw.get("smoke_x", 0.0))
	snap.smoke_y = float(raw.get("smoke_y", 0.0))
	snap.smoke_radius = float(raw.get("smoke_radius", 0.0))
	snap.smoke_ticks_left = int(raw.get("smoke_ticks_left", 0))
	snap.players = raw.get("players", PackedFloat32Array())
	return snap


func value(player: int, field: int) -> float:
	var index := player * stride + field
	if index < 0 or index >= players.size():
		return 0.0
	return players[index]


func x(player: int) -> float: return value(player, FX)
func y(player: int) -> float: return value(player, FY)
func z(player: int) -> float: return value(player, FZ)
func yaw(player: int) -> float: return value(player, FYAW)
func pitch(player: int) -> float: return value(player, FPITCH)
func health(player: int) -> int: return int(value(player, FHEALTH))
func armor(player: int) -> int: return int(value(player, FARMOR))
func alive(player: int) -> bool: return value(player, FALIVE) != 0.0
func ducking(player: int) -> bool: return value(player, FDUCKING) != 0.0
func team(player: int) -> int: return int(value(player, FTEAM))
func weapon_id(player: int) -> int: return int(value(player, FWEAPON))
func mag(player: int) -> int: return int(value(player, FMAG))
func reserve(player: int) -> int: return int(value(player, FRESERVE))
func money(player: int) -> int: return int(value(player, FMONEY))
func blind_ticks(player: int) -> int: return int(value(player, FBLIND))


## sim 平面坐标 (x, y, z) → Godot 世界坐标：-y 为前、-x 为右（与表现层既有映射一致）。
func world_position(player: int) -> Vector3:
	return Vector3(-y(player), z(player), -x(player))
