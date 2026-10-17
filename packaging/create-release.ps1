param(
  [string]$Version = "0.1.0",
  [string]$Configuration = "Release"
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root "build-release"
$dist = Join-Path $root "dist"
cmake --preset release 2>$null
if ($LASTEXITCODE -ne 0) { cmake -S $root -B $build -G "Visual Studio 17 2022" -A x64 }
cmake --build $build --config $Configuration --parallel
$iscc = (Get-Command iscc -ErrorAction SilentlyContinue).Source
if (-not $iscc) { throw "Inno Setup (iscc.exe) が見つかりません。" }
$env:APP_VERSION = $Version
& $iscc (Join-Path $root "installer\RoboconCadManager.iss")
$setup = Join-Path $dist "RoboconCadManagerSetup-$Version.exe"
if (-not (Test-Path $setup)) { throw "インストーラーが生成されませんでした。" }
$hash = (Get-FileHash $setup -Algorithm SHA256).Hash.ToLowerInvariant()
$manifest = [ordered]@{
  version = $Version
  installerUrl = "https://github.com/daihachi10/robocon-cad-manager/releases/download/v$Version/RoboconCadManagerSetup-$Version.exe"
  sha256 = $hash
  size = (Get-Item $setup).Length
  notes = "高専ロボコンCAD管理アプリ $Version"
}
$manifest | ConvertTo-Json | Set-Content (Join-Path $dist "update.json") -Encoding utf8NoBOM
Write-Host "生成: $setup"
Write-Host "生成: $(Join-Path $dist 'update.json')"
