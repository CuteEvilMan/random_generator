# 放在脚本第一行
$OutputEncoding = [System.Text.Encoding]::UTF8
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$ErrorActionPreference = "Stop"

Write-Host "正在编译 str_random.cc ..." -ForegroundColor Cyan

# 检查是否是 Windows 环境
$isWin = $IsWindows -or $env:OS -eq "Windows_NT"

# 执行编译：
# 核心改动：直接把 -lbcrypt 写在命令行最后，确保链接顺序
if ($isWin) {
g++ -std=c++17 -O2 -Wall -s -ffunction-sections -fdata-sections `
    str_random.cc -o out -lbcrypt "-Wl,--gc-sections"
} 

if ($LASTEXITCODE -eq 0) {
    $ext = if ($isWin) { ".exe" } else { "" }
    Write-Host "✓ 编译成功！可执行文件: out$ext" -ForegroundColor Green
} else {
    Write-Host "× 编译失败" -ForegroundColor Red
    exit $LASTEXITCODE
}