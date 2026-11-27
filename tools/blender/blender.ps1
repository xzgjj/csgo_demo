# blender.ps1 —— 调 Blender MCP 桥的稳定入口（任意目录可用）
#
# 用法：
#   .\blender.ps1 blender_status
#   .\blender.ps1 blender_py '{"code":"import bpy; print(len(bpy.data.objects))","see":true}'
#   .\blender.ps1 blender_op '{"action":"plan","op":"audit_scene","args":{"summary_only":true}}'
#
# 说明：本脚本只是把 mcporter（MCP 客户端）指向项目级 mcporter.json 里的 blender 服务器，
# 真正的协议转换在 tools/blender/mcp_bridge.mjs 里完成；不依赖当前工作目录。
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$Tool,

    [Parameter(Position = 1)]
    [string]$ArgsJson = '{}'
)

$ErrorActionPreference = 'Stop'
$config = Join-Path $PSScriptRoot '..\..\config\mcporter.json'
if (-not (Test-Path -LiteralPath $config)) {
    throw "找不到 mcporter 配置：$config（先跑 mcporter config add blender ...）"
}

& mcporter --config $config call "blender.$Tool" --args "$ArgsJson" --output json
exit $LASTEXITCODE
