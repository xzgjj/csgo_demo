# 获取并构建 godot-cpp（MinGW / Godot 4.7.2 宿主扩展）
# 用法：powershell -ExecutionPolicy Bypass -File tools/fetch_godot_cpp.ps1
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$godotCpp = Join-Path $root "third_party/godot-cpp"
$tar = Join-Path $env:TEMP "godot-cpp-4.5.tar.gz"
$build = Join-Path $godotCpp "build"

if (-not (Test-Path $godotCpp)) {
    Write-Host "[fetch] godot-cpp 4.5"
    curl.exe --max-time 120 -L -o $tar "https://codeload.github.com/godotengine/godot-cpp/tar.gz/refs/heads/4.5"
    if (-not (Test-Path $tar)) { throw "下载 godot-cpp 失败（可手动放置到 third_party/godot-cpp）" }
    $tmp = Join-Path $env:TEMP "godot-cpp-extract"
    if (Test-Path $tmp) { Remove-Item $tmp -Recurse -Force }
    New-Item -ItemType Directory -Path $tmp | Out-Null
    tar.exe -xzf $tar -C $tmp
    Move-Item (Join-Path $tmp "godot-cpp-4.5") $godotCpp
}

if (-not (Test-Path (Join-Path $build "bin/libgodot-cpp.windows.template_debug.x86_64.a"))) {
    Write-Host "[fetch] 构建 godot-cpp（template_debug）"
    cmake -S $godotCpp -B $build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DGODOTCPP_TARGET=template_debug
    if ($LASTEXITCODE -ne 0) { throw "godot-cpp 配置失败（需要 MinGW g++/CMake/Ninja）" }
    cmake --build $build
    if ($LASTEXITCODE -ne 0) { throw "godot-cpp 构建失败" }
}

# MinGW 运行时 DLL（GDExtension 动态库依赖）
$bin = Join-Path $root "host/bin"
New-Item -ItemType Directory -Force -Path $bin | Out-Null
foreach ($dll in "libstdc++-6.dll", "libgcc_s_seh-1.dll", "libwinpthread-1.dll") {
    $src = Join-Path "C:/msys64/ucrt64/bin" $dll
    if (Test-Path $src) { Copy-Item $src $bin -Force }
}
Write-Host "[fetch] 完成：$godotCpp"
