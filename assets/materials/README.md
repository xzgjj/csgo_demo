# 材质规范（6 类 + 2 个高亮派生）

本目录登记材质的命名与 PBR 参数；**运行时定义在 `host/scripts/material_lib.gd`（程序化
`StandardMaterial3D`，无外部贴图依赖）**，本文件是参数权威表，改参数时两处同步。

## 1 命名

| 类别 | id | 派生材质 id | 用途 |
|---|---|---|---|
| 混凝土 | `concrete` | — | 墙体、路面（lane）、通用结构 |
| 金属 | `metal` | — | 油桶、灯具、标志、门框、武器金属件 |
| 木 | `wood` | — | 木箱、木板、门板、AK 木质护木/枪托 |
| 玻璃 | `glass` | — | 玻璃窗、AWP 瞄具镜片 |
| 布料 | `fabric` | — | 沙袋墙、软包 |
| 泥土 | `dirt` | — | 地面（floor）、户外地表 |
| 包点高亮 | — | `site` | 由 concrete 派生，红色自发光标识包点 |
| 出生点高亮 | — | `spawn` | 由 concrete 派生，蓝色自发光标识出生点 |

命名规则：材质 id 全小写 snake_case；派生材质以场景语义命名（`site`/`spawn`），不写颜色名。

## 2 PBR 参数

| id | albedo（sRGB 0–1） | roughness | metallic | transparency | emission |
|---|---|---|---|---|---|
| `concrete` | 0.46, 0.45, 0.43 | 0.92 | 0.0 | opaque | — |
| `metal` | 0.56, 0.58, 0.62 | 0.32 | 0.95 | opaque | — |
| `wood` | 0.45, 0.30, 0.17 | 0.78 | 0.0 | opaque | — |
| `glass` | 0.70, 0.82, 0.88 @ a=0.34 | 0.06 | 0.10 | alpha | — |
| `fabric` | 0.42, 0.37, 0.29 | 1.00 | 0.0 | opaque | — |
| `dirt` | 0.34, 0.28, 0.21 | 0.98 | 0.0 | opaque | — |
| `site` | 0.62, 0.22, 0.18 | 0.88 | 0.0 | opaque | 0.85,0.18,0.12 ×0.9 |
| `spawn` | 0.20, 0.38, 0.62 | 0.88 | 0.0 | opaque | 0.15,0.40,0.85 ×0.9 |

## 3 场景映射

`host/scripts/blockout.gd` 按 `host/scenes/de_ash_boxes.json` 的 `kind` 选材质：

| kind | 材质 | 说明 |
|---|---|---|
| `floor` | `dirt` | 地面厚板 |
| `lane` | `concrete` | 路线路面 |
| `wall` | `concrete` | 四周边界墙 |
| `site` | `site` | 包点高亮 |
| `spawn` | `spawn` | 出生点高亮 |
| `prop` | 由几何特征推断（`MaterialLib.prop_material_id`） | 木箱→wood、油桶/灯具/标志→metal、玻璃窗→glass、沙袋墙→fabric |

`de_ash.layout.json` 的 `type` 字段映射保留在 `MaterialLib.material_for_prop_type()`，
供后续把 layout 数据接入 Godot 工程后直接复用。

## 4 贴图（占位）

`assets/generated/` 下 6 张 128×128 程序化贴图由 `tools/assets/make_placeholder_assets.py`
生成（固定 seed，可重复运行字节一致），许可与来源登记在 `assets/manifest.json`。
当前它们作为贴图源资产保留，运行时材质暂未采样贴图（`assets/` 位于 Godot 工程根 `host/`
之外，导入前不可用），接入方案见 `docs/v3-batch-b-materials.md` 的遗留问题。
