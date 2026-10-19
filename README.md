# 高専ロボコン CAD 管理

Windows 11 x64 用の軽量な Win32 ネイティブアプリです。共有用インストーラーはユーザー単位で導入し、インストール中にGit for WindowsをHTTPSで取得します。

## 利用者向けマニュアル

GitHubアカウントの作成からアプリの初回設定、リポジトリの複製、LFS設定、変更の送信までを説明した初心者向け手順は [docs/setup-manual-ja.md](docs/setup-manual-ja.md) を参照してください。

## ビルド

Visual Studio の x64 Native Tools Command Prompt で実行します。

```powershell
# この環境のMSVCではPDB出力先に日本語を含められないため、出力先はASCIIパスにします。
cmake -S . -B C:\robocon-cad-build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build C:\robocon-cad-build
cmake --install C:\robocon-cad-build --prefix .\dist
```

`dist/RoboconCadManager.exe` を、`tools/git/bin/git.exe` と `tools/git-lfs.exe` を含む配布フォルダへ置きます。

## インストーラーと更新

Inno SetupがインストールされたWindowsで、x64 Native Tools Command Promptから次を実行します。

```powershell
$env:APP_VERSION = "0.1.0"
.\packaging\create-release.ps1 -Version $env:APP_VERSION
```

`dist/RoboconCadManagerSetup.exe` と `dist/update.json` が生成されます。インストーラーは管理者権限を要求せず、既存の `%LocalAppData%\RoboconCadManager` を削除しません。アプリは起動後にGitHub Releasesの `update.json` を1回だけ確認し、最新版があるときだけ確認ダイアログを表示します。通信できない場合は通常の起動を続けます。

最新版のインストーラーは常に `https://github.com/daihachi10/robocon-cad-manager/releases/latest/download/RoboconCadManagerSetup.exe` から取得できます。

公開リリースは `daihachi10/robocon-cad-manager` の `v*` タグでGitHub Actionsが作成します。コード署名は行わず、更新インストーラーはHTTPSとSHA-256で検証します。

## 初期設定

1. 「設定」からリポジトリURL、保存先、氏名・メールを登録します。
2. 「複製」でリポジトリを取得します。認証が必要な場合は、Gitの認証画面からGitHubへサインインします。
3. 以後は「最新状態を取得」と「保存して送信」を使用します。

「変更」画面では、送信するファイルにチェックを付けます。新しく検出した通常ファイルは自動で選択され、`~$`で始まるSolidWorksの一時ファイルは選択されません。利用者が変更したチェック状態は、一覧を更新しても保持されます。

個人トークンの入力や保存は行いません。Gitの認証管理機能を使用します。

## 管理者用ポリシー例

```json
{"version":1,"mainBranch":"main","leaders":["github-id"],"backupGuide":"月1回、GitHub以外の保存先へバックアップする"}
```

本アプリは作業場所を `main` に限定し、変更を `main` へ直接送信します。現在の作業場所が `main` 以外の場合は送信せず、日本語で切り替え手順を表示します。

## 未実装の境界

SolidWorks の参照関係自動解析とGit LFSの厳格なファイルロックは未実装です。CADのバイナリ競合は自動マージせず、Gitの競合情報を表示します。編集予約、ブランチ、プルリクエスト、レビュー機能は扱いません。
