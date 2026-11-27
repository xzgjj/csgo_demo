# Blender MCP 桥（v0.2）—— DSH Gateway → 标准 MCP，9 个工具，29 条路由全覆盖

> 默认约定：**以后调 Blender 一律走本桥**（`tools/blender/mcp_bridge.mjs` + `tools/blender/blender.ps1`），
> 不再启动官方 `blender-mcp`（`uvx` + 9876 addon）那条路。`dsh-blender-plugin` 的全部文件**只读借用，未做任何修改**。

## 1. 为什么是"包一层"

| 链路 | 协议 | 状态 |
|---|---|---|
| 官方 `blender-mcp`（`uvx blender-mcp` + `MCP for Blender` addon） | MCP 原生 | **不可用**：`127.0.0.1:9876` 未监听；磁盘 addon 协议过旧（期望 16） |
| `dsh-blender-plugin` 3.1.0-alpha.1 | DSH 插件 + HTTP Gateway `39877` + addon 反向 WS | **可用**：`readiness.ready=true`、`bpy.ok=true`、34 family / 228 op |
| 本桥 `mcp_bridge.mjs` | MCP stdio → Gateway HTTP | **可用**，9 工具已实测 |

插件文档明令 Blender 侧只允许一套 agent bridge：3.1 addon 不监听入站端口，再启 9876 会两套桥打架。
所以正确姿势是复用 39877 这条通道，用 MCP 适配层把它变成任何 MCP 客户端都能吃的服务器。

```
MCP 客户端（mcporter / Claude / Cursor）
   │ JSON-RPC 2.0 over stdio（逐行）
   ▼
mcp_bridge.mjs（零依赖 Node，本仓 tools/blender/）
   │ HTTP 127.0.0.1:39877 + X-DSH-* 身份头
   ▼
dsh-blender-plugin runtime（DSH 宿主 PID 6812 拉起，只读）
   │ WebSocket（addon 主动反连，instanceId 路由）
   ▼
DSH Blender Bridge addon → Blender 主线程 bpy
```

## 2. 工具面（9 个）与路由覆盖

| MCP 工具 | 覆盖路由 | 能力 |
|---|---|---|
| `blender_status` | `/status`、`/who` | 就绪探针：gateway/addon/bpy、实例 ID、租约、metrics |
| `blender_py` | `/act` | GUI 会话执行 Python（持久内核），`see=true` 附带抓帧 |
| `blender_see` | `/frame.png`、`/view` | 视口/相机/自定义视角出图，回 PNG + 元信息 |
| `blender_inspect` | `/commands`、`/cmd` | 命令目录（help）+ 执行只读命令（run） |
| `blender_op` | `/plan`、`/loop`、`/opt`、`/preset` | 34 family / 228 op 唯一入口；内环搜索、优化、配方库 |
| `blender_run` | `/job`、`/headless`、`/worker`、`/perf` | 作业提交/状态/等待/日志/取消/列表；热无头 worker；渲染性能诊断 |
| `blender_session` | `/lease`、`/release`、`/who` | 会话归属、写 scope 授权、写租约、pause/resume（人类专属） |
| `blender_txn` | `/txn` | 回退点：snapshot / mark / restore / revert / prune |
| `blender_gateway` | `/doctor`、`/host/*` 等只读白名单 | 逃生口，未单独封装的端点直查 |

未封装且**故意不封装**：`/launch`（模型面不该拉起 Blender，宿主职责）、`/host/selection*` 写入路径（属于 DSH 面板交互）。

流式回执兼容：`/headless`、`/job?op=wait`、`/worker`、`/txn`、`/preset` 用的是每行一条 JSON 的流式响应，
桥里做了"取最后一条有效信封 + 保留 `stream` 心跳"的解析，不会被多行 body 打断。

## 3. 怎么用

### 3.1 已注册（项目级 mcporter 配置）

`config/mcporter.json` 里已有 `blender` 服务器，任意目录都能调：

```powershell
# 任意 cwd
D:\c++_myproject\game_project\csgo_demo\tools\blender\blender.ps1 blender_status
D:\c++_myproject\game_project\csgo_demo\tools\blender\blender.ps1 blender_py '{"code":"import bpy; print(len(bpy.data.objects))","see":true}'
D:\c++_myproject\game_project\csgo_demo\tools\blender\blender.ps1 blender_op '{"action":"plan","op":"audit_scene","args":{"summary_only":true}}'
```

```powershell
# 或直接 mcporter（显式 --config，避免 cwd 依赖）
mcporter --config D:\c++_myproject\game_project\csgo_demo\config\mcporter.json call blender.blender_status --output json
```

### 3.2 其他 MCP 客户端

```json
{
  "mcpServers": {
    "blender": {
      "type": "stdio",
      "command": "node",
      "args": ["D:\\c++_myproject\\game_project\\csgo_demo\\tools\\blender\\mcp_bridge.mjs"],
      "env": {
        "DSH_BLENDER_GATEWAY": "http://127.0.0.1:39877",
        "BLENDER_MCP_OUT_DIR": "C:\\Users\\72440\\AppData\\Local\\Temp\\blender-mcp"
      }
    }
  }
}
```

前置条件：DSH（DeepSeek Harness）在跑（Gateway 39877 由它拉起），Blender 里 `DSH Blender Bridge` 面板已 Connect。
Gateway 没起时桥返回 `fetch failed`，与 Blender 无关。

### 3.3 可视化台（本地网页）

```powershell
node D:\c++_myproject\game_project\csgo_demo\tools\blender\viewer\server.mjs
# 浏览器打开 http://127.0.0.1:8765
```

页面内容：实时视口（1.2 s 轮询 `/frame.png`）、就绪/实例/租约三态、`/who` 的调用·错误·超时计数、
能力面版本（34 family / 228 op）、控制条（跑 Python、打回退点、场景概览、立即取帧）。
它只是本机 `127.0.0.1` 代理：不新开 Blender 连接、不改插件文件，端口可用 `BLENDER_VIEWER_PORT` 改。

## 4. 实测记录（2026-11-27，真实回包）

| 工具 | 结果 |
|---|---|
| `blender_status` | `ok=true`、`addon.connected=true`、`bpy.ok=true`、`3.1.0-alpha.1`、`families=34`、`opsCount=228` |
| `blender_py` | `executed=true, ms=46`，`OBJECTS=Camera:CAMERA,Cube:MESH,Light:LIGHT`、`RES=1920x1080` |
| `blender_see`(view) | PNG 960×540，`meta.shading_used=SOLID`，落盘 `blender-frame-*.png` |
| `blender_op`(catalog) | 返回 34 个 family 全表 + 顶层工具说明 |
| `blender_op`(plan=audit_scene) | `count=1`、`tris=12`、`closed=true`、`verdict=degraded`（初筛跳过自交检查） |
| `blender_inspect`(help) | `total=18` 条 addon 命令，含 access/gate/integration 状态 |
| `blender_run`(list) | `jobs=[]` |
| `blender_txn`(list) | `out_dir=%LOCALAPPDATA%\dsh-blender-rt`、`snapshot_dir=…\snapshots` |
| `blender_session`(list) | `sessions=[]`（见下节：addon 实例未走 session registry 认领） |
| 对照：官方 `blender-mcp` | `Could not connect to Blender at localhost:9876 … outdated (expected protocol 16)` |

### 4.1 端到端用例：`blockout_base` 造一个弹匣箱白模

`blender_txn(op="snapshot")` 先打回退点（`…\snapshots\before_blockout_test.blend`，97 KB / 3 objects），
再用 `blender_op(action="plan", op="blockout_base")` 声明 5 个 parts（body / lid / rail_l / rail_r / latch，mm 单位）：

- 结果：`object_count=5`，集合 `BLOCKOUT`，材质 `BO_White`，每件带 Bevel 修改器，`tris=12`
- 出图：`auto3` 三视图 + 拼图（front / right / iso / sheet，640×640）
- 导出：`tools/blender/out/ammo_crate.glb`（30 KB）
- 实时可视：`gui_frame` 取景后，可视化台实时视口直接看到这件白模

## 5. 已知边界

- **会话归属**：当前 addon 实例（`dcd62406…c7e5`）只进了 Gateway 的 ready 表，没进 session registry，所以
  `blender_session(action="list")` 返回空。要认领归属用 `action="attach"` + `instanceId`；`pause/resume` 只有人能点。
- **写租约**：`/act`、`/cmd`、`/plan` 的写型 op 都过 `lease` 门；DSH 持租约时其他调用方收 409。改场景前先 `blender_txn(op="snapshot")`。
- **身份头（踩过的坑）**：session 动作需要 `X-DSH-Host-Id / X-DSH-Session-Id / X-DSH-Agent-Id`，
  但**不能全站默认带** —— 带头的 `/act` 会进会话归属门，未认领会话的调用方直接吃 `409 session_unavailable`（实测）。
  所以桥默认匿名，只在 `blender_session` 的归属类动作上注入（默认 `doubao-mcp-bridge / mcp-bridge / main`，
  可用 `DSH_HOST_ID` 等覆盖；`BLENDER_MCP_IDENTITY=always` 可强制全站携带，仅供调试）。
- **并行边界**：桥不新开端口、不新建 Blender 连接，可与 DSH 自带 6 工具并存；但同一场景的写操作仍受同一租约串行。
- **插件升级**：alpha 版路由表是 29 条冻结契约；升级后先跑 `blender_status`，字段变了再调 `mcp_bridge.mjs`。

## 6. 文件

- `tools/blender/mcp_bridge.mjs` —— MCP stdio 桥（零依赖，Node ≥18；9 工具，29 路由）
- `tools/blender/blender.ps1` —— 任意目录可用的命令行入口（内部只转发 mcporter）
- `tools/blender/viewer/server.mjs` —— 可视化台（实时视口 + 状态 + 控制条，端口 8765）
- `tools/blender/out/ammo_crate.glb` —— 用例产物：5 件弹匣箱白模
- `config/mcporter.json` —— 项目级 MCP 注册（`blender` 服务器）
- `tools/blender/MCP-BRIDGE.md` —— 本文档
