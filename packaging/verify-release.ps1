param([switch]$TestInstaller,[string]$GitExecutable='')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$testDir = Join-Path $root 'build-release\verification'
[IO.Directory]::CreateDirectory($testDir) | Out-Null
Push-Location $testDir
try {
  & cl /nologo /std:c++20 /EHsc /utf-8 "/I$root\src" "$root\tests\advisory_cad_test.cpp" "$root\src\cad.cpp" "$root\src\process.cpp" /Fe:advisory_cad_test.exe /link shell32.lib shlwapi.lib
  if ($LASTEXITCODE -ne 0) { throw 'CAD検証プログラムをビルドできません。' }
  & '.\advisory_cad_test.exe' $testDir
  if ($LASTEXITCODE -ne 0) { throw 'CADの書き込み検証に失敗しました。' }
  & cl /nologo /std:c++20 /EHsc /utf-8 "/I$root\src" "$root\tests\git_identity_test.cpp" "$root\src\git.cpp" "$root\src\process.cpp" "$root\src\json.cpp" /Fe:git_identity_test.exe /link shell32.lib shlwapi.lib
  if ($LASTEXITCODE -ne 0) { throw '氏名・メールの検証プログラムをビルドできません。' }
  if (-not $GitExecutable) { $GitExecutable=(Get-Command git -ErrorAction Stop).Source }
  $identityDir=Join-Path $testDir ([guid]::NewGuid().ToString('N'))
  & '.\git_identity_test.exe' $GitExecutable $identityDir
  if ($LASTEXITCODE -ne 0) { throw 'Gitへの氏名・メール反映の検証に失敗しました。' }
} finally { Pop-Location }

$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
$cmake = if ($cmakeCommand) { $cmakeCommand.Source } else {
  Get-ChildItem 'C:\Program Files\Microsoft Visual Studio\*\*\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' | Select-Object -First 1 -ExpandProperty FullName
}
& $cmake --build (Join-Path $root 'build-release') --config Release --target FolderBrowserTest
if ($LASTEXITCODE -ne 0) { throw 'フォルダ表示検証プログラムのビルドに失敗しました。' }
$folderTest=Join-Path $root 'build-release\Release\FolderBrowserTest.exe'
if (-not (Test-Path -LiteralPath $folderTest)) { $folderTest=Join-Path $root 'build-release\FolderBrowserTest.exe' }
& $folderTest $GitExecutable (Join-Path $testDir ([guid]::NewGuid().ToString('N')))
if ($LASTEXITCODE -ne 0) { throw 'フォルダ表示・変更検出・自動チェックの検証に失敗しました。' }

# Only CI may install a test instance. Local verification never changes the installed app.
if ($TestInstaller) {
  if ($env:GITHUB_ACTIONS -ne 'true') { throw 'インストール試験はGitHub Actions専用です。' }
  $appDir = Join-Path $env:RUNNER_TEMP 'RoboconCadManager-verify'
  $setup = Join-Path $root 'dist\RoboconCadManagerSetup.exe'
  $log = Join-Path $env:RUNNER_TEMP 'RoboconCadManager-setup.log'
  $arguments = @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',('/DIR="'+$appDir+'"'),('/LOG="'+$log+'"'))
  for ($attempt=0;$attempt -lt 2;$attempt++) {
    $process = Start-Process -FilePath $setup -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
    if ($process.ExitCode -ne 0) { Get-Content $log -Tail 50; throw "インストール検証に失敗: $($process.ExitCode)" }
    $git = Join-Path $appDir 'tools\git\bin\git.exe'
    & $git --version
    if ($LASTEXITCODE -ne 0) { throw '同梱Gitが起動しません。' }
    & $git lfs version
    if ($LASTEXITCODE -ne 0) { throw '同梱Git LFSが起動しません。' }
    & $git credential-manager --version
    if ($LASTEXITCODE -ne 0) { throw '同梱Git Credential Managerが起動しません。' }
    # Verify hooks and LFS filtering in a local repository, without network or CAD data.
    $repo = Join-Path $testDir 'lfs-repo'
    [IO.Directory]::CreateDirectory($repo) | Out-Null
    & $git -C $repo init --quiet
    if ($LASTEXITCODE -ne 0) { throw 'LFS試験用フォルダを作成できません。' }
    & $git -C $repo lfs install --local
    if ($LASTEXITCODE -ne 0) { throw 'LFSフックを設定できません。' }
    'sample content' | & $git -C $repo lfs clean -- sample.sldprt
    if ($LASTEXITCODE -ne 0) { throw 'LFSポインターを生成できません。' }
  }
}
