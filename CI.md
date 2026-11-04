# CI 规则与执行（csgo_demo）



## 原则

- CI 在每次修改后自动运行：早期反馈、失败可归因、可复现。
- 机器判定可机械判定的；人工判断抽象必要性、命名/设计、越界合理性。
- 规则写入 `ci/spec.json`（机器）+ 本文件（人）；阈值只紧不宽、判定只认实测。
- 首次引入即基线；重试上限 3，不无限重跑。



## 自动检查（AUTO）

| 检查 | 规则 |
|---|---|
| 文档预算 | 规划文档 ≤3000 字符；其他文档 ≤20KB；分析文档（docs/、research/）不限 |
| 重复段落 | 同段 ≥80 字符重复 → 失败 |
| 重复代码块 | 归一化后 ≥8 行相同 → 失败 |
| 依赖方向 | core 禁引擎/平台头；模块按白名单 |
| 格式/Lint | clang-format、gdformat/gdlint、ruff/black（工具缺失=SKIP 并报告） |
| 复杂度 | 单函数圈复杂度 ≤12；单文件行数上限（C++/GDScript 400、Python 400） |
| 改动预算 | 单次 ≤50 文件、≤2400 行增删；超限需审批 |
| 受保护用例 | 测试文件/用例只增不减（本地 tests/） |
| 规则收紧 | spec 数值只许收紧；规则文件变更需 `CI_POLICY_APPROVER` |
| 作者 | 提交作者/共同作者仅 `xzgjj`；禁 `Co-authored-by` |
| 目录结构 | 顶层 ≤14；深度 ≤3（特殊 ≤4）；禁 application/domain/infrastructure/adapter/shared/common；测试仅 tests/ |
| 素材许可 | manifest 必填来源/许可/作者；缺失/不在白名单 → 失败 |
| 场景校验 | 场景 JSON schema、引用地图/defs 存在 |
| 日志 | log/ ≤20MB、≤100 文件 |
| 环境基线 | Godot 4.7.2、Python 3.13、g++≥16、CMake≥3.25 |



## 人工判断（MANUAL）

原创性/内容价值；抽象是否必要；命名与设计；越界是否批准；行为变更是否合理；素材艺术质量；测试移除的合理性。



## 执行

```powershell
python -X utf8 tools/ci/run_local.py            # 全流程，exit 0/1
python -X utf8 tools/ci/run_local.py --strict   # SKIP 视为失败
python -X utf8 tools/ci/run_local.py --retry 3  # 仅对构建/测试重试，上限 3
python -X utf8 build.py --all                   # 构建+测试+CI
```
首次运行生成 `ci/baseline_snapshot.json`；之后与基线比较，只允收紧；放宽须人工批准并在 `diff.md` 记录。



## 审批边界

`ci/`、`tools/ci/`、`CI.md`、`ci/spec.json`、受保护用例、基线文件的变更必须由 `CI_POLICY_APPROVER` 指定人批准（默认 `xzgjj`，单人仓库例外须记录）。不批准直接推送。



## 例外

人工批准方可豁免；例外写入提交说明与 `diff.md`，不做永久 skip。连续 3 次非确定性失败即隔离并查根因。
