# v0.1.17 検証記録

## 変更内容

- Git LFSや読み取り専用属性が付いたファイルを削除する前に、Windowsの読み取り専用属性を解除するようにしました。
- 自動更新でインストーラーを起動した後、完了メッセージを表示せずにアプリを終了するようにしました。
- Inno Setupの`CloseApplications`を`force`に設定し、更新インストール時にアプリの終了確認で止まらないようにしました。

## 検証

- Releaseビルドでコンパイル確認を行います。
- 既存のIntegrationFixTest、FolderBrowserTest、GitIdentityTestを実行します。
- 自動更新のインストーラー実行は、このPCへインストールしない方針のため未実行です。配布用Actionsでインストーラーを生成します。
