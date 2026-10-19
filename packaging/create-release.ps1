param(
  [string]$Version = '',
  [ValidateSet('Release')][string]$Configuration = 'Release'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root 'build-release'
$dist = Join-Path $root 'dist'
$projectText = Get-Content (Join-Path $root 'CMakeLists.txt') -Raw
if ($projectText -notmatch 'project\(RoboconCadManager VERSION ([0-9]+\.[0-9]+\.[0-9]+)') { throw 'CMakeのバージョンを取得できません。' }
$projectVersion = $Matches[1]
if (-not $Version) { $Version = $projectVersion }
if ($Version -ne $projectVersion) { throw "指定バージョン $Version とCMakeの $projectVersion が一致しません。" }
$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
$cmake = if ($cmakeCommand) { $cmakeCommand.Source } else {
    Get-ChildItem 'C:\Program Files\Microsoft Visual Studio\*\*\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
}
if (-not $cmake) { throw 'CMakeが見つかりません。' }
& $cmake -S $root -B $build
if ($LASTEXITCODE -ne 0) { throw 'CMakeの構成に失敗しました。' }
& $cmake --build $build --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw 'Releaseビルドに失敗しました。' }
# Visual Studio and NMake use different output directories.
$binary = Join-Path $build "$Configuration\RoboconCadManager.exe"
if (-not (Test-Path -LiteralPath $binary)) { $binary = Join-Path $build 'RoboconCadManager.exe' }
if (-not (Test-Path -LiteralPath $binary)) { throw 'ビルド済みEXEがありません。' }
$binaryVersion = (Get-Item -LiteralPath $binary).VersionInfo.ProductVersion
if ($binaryVersion -ne $Version) { throw "EXEのバージョン $binaryVersion が $Version と一致しません。" }
$isccCommand = Get-Command iscc -ErrorAction SilentlyContinue
$iscc = if ($isccCommand) { $isccCommand.Source } else {
    @('C:\Program Files\Inno Setup 7\ISCC.exe', 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe') | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if (-not $iscc) { throw 'Inno Setup (ISCC.exe) が見つかりません。' }
[IO.Directory]::CreateDirectory($dist) | Out-Null
$env:APP_VERSION = $Version
& $iscc "/DAppBinary=$binary" (Join-Path $root 'installer\RoboconCadManager.iss')
if ($LASTEXITCODE -ne 0) { throw 'インストーラーの生成に失敗しました。' }
$setup = Join-Path $dist 'RoboconCadManagerSetup.exe'
$manifest = [ordered]@{
  version = $Version
  installerUrl = 'https://github.com/daihachi10/robocon-cad-manager/releases/latest/download/RoboconCadManagerSetup.exe'
  sha256 = (Get-FileHash -LiteralPath $setup -Algorithm SHA256).Hash.ToLowerInvariant()
  size = (Get-Item -LiteralPath $setup).Length
  notes = "高専ロボコンCAD管理アプリ $Version — インストーラー名を固定"
}
[IO.File]::WriteAllText((Join-Path $dist 'update.json'), ($manifest | ConvertTo-Json), [Text.UTF8Encoding]::new($false))
Write-Host "生成: $setup"
Write-Host "生成: $(Join-Path $dist 'update.json')"
