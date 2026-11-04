# 时间线验证（Timeline Suite）

把「需要游玩时间的场景」变成 **tick 步进 + 时间线断言**：时间不是等出来的，是推进出来的。
1 秒 = 64 tick（`host/project.godot: common/physics_ticks_per_second=64`，`main.gd:_physics_process` 每帧 1 tick）。

## 为什么

截图 golden 只证明「某一刻长这样」，证明不了「过程对不对」（阶段流转、计时精度、连发漂移、AI 长时行为、卡墙、内存/性能漂移）。
时间线套件补这一层：一次进程跑完整回合/整场，落盘逐拍采样 + 阶段边界，再用断言检查曲线与边界。

实测（本机 Godot 4.7.2，debug 构建）：

| 场景 | 模拟时长 | tick | 墙钟 |
|---|---|---|---|
| 纯内核推进（`level=sim`，无 AI 决策） | 115 s | 7360 | ≈0.7 ms |
| 宿主逐帧（`level=host`，含输入/HUD/相机） | 115 s | 7360 | ≈6 s |
| 12 回合 soak（`level=sim`，含人机 A* 决策） | 24 min | 94464 | ≈72 s |

## 组成

| 文件 | 作用 |
|---|---|
| `host/tests/timeline_suite.gd` | 驱动器：加载 `main.tscn`，关掉引擎自动物理，按 spec 手动推进 tick，采样/记边界/抓关键帧 |
| `verification/timelines/<id>.json` | 场景：level / mode / sample_every / expect 预算 / script 动作 / 截图点 |
| `tools/verify/run_timeline.py` | 断言 + 报告 + golden：跑场景 → 校验 → `log/verify/timeline/report.md` |
| `verification/goldens/timeline/<id>.json` | 时间线 golden（每回合 freeze/live/end 时长、比分、最终状态） |
| `verification/actual/timelines/<id>/` | 制品：`timeline.jsonl`、`transitions.jsonl`、`marks.jsonl`、`meta.json`、关键帧 PNG |

## 用法

```powershell
python -X utf8 tools/verify/run_timeline.py                       # 全部时间线场景
python -X utf8 tools/verify/run_timeline.py --scenario timeline_bomb
python -X utf8 tools/verify/run_timeline.py --visual              # 关键帧截图（窗口化 GUI/GPU）
python -X utf8 tools/verify/run_timeline.py --determinism         # 同场景跑两遍，时间线逐字节比对
python -X utf8 tools/verify/run_timeline.py --update              # 行为变更后重建 golden（人工门）
```

退出码：0 全过；1 有失败。报告在 `log/verify/timeline/report.md`，逐项 PASS/FAIL + 每回合时长表。

## 现有场景

| id | 覆盖 |
|---|---|
| `timeline_round` | 整回合：Freeze 192 → Live 7360（到点 CT 胜）→ RoundEnd 320 → 下一回合；host 层关键帧 5 张 |
| `timeline_soak` | 12 回合 soak（94464 tick）：每回合边界、比分、存活/位移不变量、性能预算 |
| `timeline_bomb` | C4 时间线：安放后 **2559 tick** 起爆（`bomb_ticks=2560` 的首 tick 被安放当拍消耗）→ RoundEnd 319 → 下一回合；计时器每 tick -1 |

## 断言清单（run_timeline.py）

- 结构：阶段转换只允许 `0→1 / 1→2 / 2→0`；回合数与转换条数 = 回合数×3；10 人
- 时长：`freeze == 192`、`round_end == expect.end_ticks`、`live ∈ [min,max]`；每回合胜方得分 +1
- 不变量：hp 0–100、mag 0–30、z ≥ -1、坐标在 ±2300 内、存活数 0–5、金钱 0–16000、C4 计时器 0–2560
- 位移：同一回合内单 tick 位移 ≤ 12 u（防瞬移/穿地；`teleport` 动作点自动豁免）
- 性能：`ms/tick` 与墙钟预算（防 AI 退化）；确定性：`--determinism` 两遍逐字节一致
- golden：回合时长/比分/最终状态精确比对（kernel 确定性，可精确 golden）

## 加一个新场景

```json
{
  "id": "timeline_xxx", "level": "sim", "mode": "match", "sample_every": 16,
  "expect": {"rounds": 1, "freeze_ticks": 192, "end_ticks": 320,
             "min_live_ticks": 64, "max_live_ticks": 7360,
             "max_step_per_tick": 12.0, "max_ms_per_tick": 3.0},
  "script": [
    {"action": "wait_until", "field": "round", "equals": 2, "max_ticks": 10000}
  ],
  "screenshots": [{"tick": 192, "name": "live_start"}]
}
```

动作：`auto_tick`（人机驱动）/ `tick`（无人机）/ `wait_until`（按字段推进）/ `teleport` / `plant` / `defuse` / `mark`。
`level=host` 走真实 `main._physics_process`（含输入/HUD/相机/音频，慢约 10×）；`level=sim` 只推进内核（快，不跑表现层）。

## 接入 CI / run_all（建议）

`run_timeline.py` 目前独立，不改动 `run_all.py`（该文件由另一会话维护）。建议：

- 每次改动后：`--scenario timeline_bomb --scenario timeline_round`（约 8 s）
- 夜间/合入前：全量（含 soak，约 80 s）
- 行为变更（AI/回合/经济/武器）后：先 `--update` 重建 golden 并在 `diff.md` 记录，再跑一次确认

## 已知注意

- `run_scenarios.ensure_host()` 会在内核库比 DLL 新时自动重建；若 Godot 编辑器正占用 DLL，重建会失败（Permission denied）但会继续用旧 DLL，时间线结果可能对应旧内核 → 编辑器开着时先确认 DLL 版本。
- C4 起爆那拍 `EndRound` 在 `StepBomb` 内触发，同 tick `AdvanceRound` 已扣 1，故 `round_end` 观测到 319 而非 320；`timeline_bomb.json` 用 `end_ticks: 319` 记录该事实。
- 当前 12 回合 soak 观察：CT 12:0 全部靠 115 s 到点获胜、T 方 12 回合 0 次下包——时间线套件把「T 方不下包/回合永不提前结束」暴露为可跟踪数据（属玩法缺口，不是套件问题）。
