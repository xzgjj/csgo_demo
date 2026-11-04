# csgo_demo · Project Recoil

原创战术射击游戏（行为/数值对标 CS:GO，资产与内容原创）。架构：**C++20 纯函数模拟内核 + Godot 4.7.2 表现层**；以「状态转换 + 代码断言 + 截图 golden」双检查验收。

- 交付目标：1 张原创地图的纵向切片，**启动页/功能页/游戏逻辑模块完整**，5v5 战术 Bot 分阶段实现。
- 架构文档（本地）：`docs/csgo-architecture/`；研究基线（本地）：`research/`；可运行 PoC（本地）：`prototypes/sim_core_poc/`。
- 规划与协议：`CLAUDE.md`、`SPEC.md`、`implementation_plan.md`、`AGENT_EXECUTION_PROTOCOL.md`、`notes.txt`、`diff.md`、`implementation-notes.md`。
- 以上文档/测试/日志按约定**不入版本库**（见 `.gitignore`）。

## 快速开始

```powershell
# 全量构建 + 测试 + 本地 CI
python -X utf8 build.py --all

# 仅本地 CI（规则见 CI.md，阈值见 ci/spec.json）
python -X utf8 tools/ci/run_local.py

# 环境基线
python -X utf8 build.py --env
```

## 目录

| 目录 | 内容 |
|---|---|
| `core/` | C++20 纯函数内核（无引擎依赖） |
| `host/` | Godot 4.7.2 宿主与表现层（只读状态） |
| `content/` | 武器/规则/地图/实体 defs（JSON + schema） |
| `assets/` | 模型/贴图/材质与许可清单 |
| `verification/` | 场景、golden、校准参考 |
| `ci/` | CI 规则 spec、基线、受保护用例 |
| `tools/ci/` | 本地 CI 运行器 |
| `tests/` | 测试（本地维护，不入库） |
| `log/` | 运行/CI 日志（不入库） |

## 许可

见 `LICENSE`（仓库初始提交自带）。
