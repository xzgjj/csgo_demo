param(
    [switch]$Editor
)
# 快速启动：默认运行游戏；-Editor 打开工作编辑器（authoring）。
$Godot = 'D:\app_finish\Godot\Godot_v4.7.2-stable_win64.exe'
$Project = Join-Path $PSScriptRoot '..\host'
if ($Editor) {
    & $Godot --path $Project --editor
} else {
    & $Godot --path $Project
}
