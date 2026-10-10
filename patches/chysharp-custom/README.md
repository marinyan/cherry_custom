# ChySharp Custom

gocha氏の **Cherry 1.4.3# Patcher（chysharp）** に、カーソル位置のペインを縦横にスクロールするホイール対応と、保存・編集・ファイル操作の標準ショートカットを組み込んだ改造版です。元の15項目とショートカット項目を個別に選択できます。旧ホイール処理と二重には動きません。

## 使い方

1. このフォルダーの [`chysharp-custom.exe`](chysharp-custom.exe) を、手元のCherryのフォルダーへコピーします。
2. 元のchysharp配布物に含まれる **`chysharp.bin`** を、`chysharp-custom.exe` と同じフォルダーに置きます。
3. `chysharp-custom.exe` を起動し、必要な修正を選択します。初期状態では全項目が有効です。
4. ホイール項目と「標準ショートカット」を選択した状態で「保存」を押します。
5. 初期ファイル名の **`cherry-custom.exe`** など、未使用の名前で保存し、作成した実行ファイルを起動します。

既存ファイルは上書きしません。元に戻す場合は、元の `cherry.exe` を起動してください。Cherryの設定や音源定義などは通常の同梱ファイルを使用するため、出力もCherryのフォルダーへ保存してください。

このパッチャーだけでchysharpの選択した修正とホイール対応を組み込みます。元の `chysharp.exe` や `note-wheel/apply.ps1` を先に実行する必要はありません。ホイール項目を無効にした場合は、新旧どちらのホイールフックも有効にしません。

通常の使用にコンパイラーやPowerShellは不要です。横ホイールはマウス設定ソフトで「横スクロール」に割り当ててください。動作確認環境はWindows 11です。

## 標準ショートカット（1.2追加、1.3拡張）

| キー | 動作 |
| --- | --- |
| Ctrl+S | 「ファイル → 上書き保存」。新規ファイルでは保存先を指定 |
| Ctrl+Z | 「編集 → 元に戻す」 |
| Ctrl+X | 「編集 → 切り取り」（1.3追加） |
| Ctrl+A | 「編集 → すべて選択」（1.3追加） |
| Ctrl+O | 「ファイル → 開く」（1.3追加） |
| Ctrl+N | 「ファイル → 新規作成」（1.3追加） |
| Ctrl+Shift+S | 「ファイル → 名前を付けて保存」（1.3追加） |
| Ctrl+C / Ctrl+V | 従来どおりコピー／貼り付け。1.3ではメニュー表示も追加 |
| Alt+Backspace | 従来どおり「元に戻す」 |
| Shift+Delete / Ctrl+Insert / Shift+Insert | 従来どおり切り取り／コピー／貼り付け |
| Ctrl+Alt+S | 旧Ctrl+Sの縦スクロール「1ページ上」 |
| Ctrl+Alt+X | 旧Ctrl+Xのピアノロールスクロール（1.3追加） |
| Ctrl+Alt+A | 旧Ctrl+Aの「1オクターブ下げる」（1.3追加） |

Cherryのアクセラレータリソースから既存の各メニューコマンドを呼びます。保存形式、Undo可能な編集範囲、新規作成・読み込み時の未保存確認は元のCherryと同じです。ファイル・編集メニューにも新しいキーを表示します。ホイール項目とは独立して有効／無効を選択できます。無効なら元のキー割り当てを保持します。

Ctrl+Aは元のCherryの専用キー処理と競合するため、標準ショートカットを有効にした場合のみ、その分岐を迂回してアクセラレータへ渡します。文字入力欄や保存ダイアログのキーは横取りしません。

Ctrl+Fは従来の「カーソル位置を演奏開始位置にセット」のままです。この改造で検索機能やRedo機能は追加していません。

以前の改造版を使っている場合も、更新したパッチャーから未使用の名前（例：`cherry-custom-keys-v13.exe`）で作り直してください。`note-wheel/apply.cmd` 単独ではキーバインドは変更されません。

## 対応するテンプレート

`chysharp.bin` はCherryのコードを含む生成用テンプレートで、このリポジトリには同梱していません。元の配布物から用意してください。起動時に次のサイズとSHA-256を確認します。

```text
size: 926208 bytes
SHA-256: 659FA2948FC22689E2D87C0F36E33A504E9D94B24693D4E3315F5904E66C9FB3
```

入力の `cherry.exe` は参照しません。選択した項目に従ってテンプレートから新しい実行ファイルを生成します。すでに手元のCherryへ加えた独自の変更は引き継がれません。

## ソースとビルド

Gitには改造版パッチャーの実行ファイルと、ビルドに必要なソース・リソースを収録しています。

- `src/chysharp.c`, `src/chysharp.h`：元のパッチャーを基にした画面・設定・保存処理。
- `src/chysharp.rc`, `src/resource.h`, `src/res/`：画面、アイコン、マニフェスト。
- `wheel-image.c`, `wheel-image.h`：テンプレート照合、新ホイール処理の組み込み、PE再配置情報の統合。
- `keys-image.c`, `keys-image.h`：標準ショートカットのアクセラレータとメニューを追加する独立したリソースセクション。
- [`../note-wheel/wheel.c`](../note-wheel/wheel.c), `wheel-plan.h`：組み込むホイール処理のソース。
- `custom-test.c`：保存とオプションの回帰テスト。

Visual StudioのC++ x86ツール、Windows SDK、Windows PowerShellを用意し、このフォルダーで次を実行します。

```bat
build.cmd
```

`chysharp-custom.exe` と `build/custom-test.exe` が生成されます。CRTは静的リンクします。**ビルドに元の `src.rar` や `chysharp.bin` は不要です。** ビルド時にはGit内の `../note-wheel/wheel-hook.bin` を埋め込みます。ホイール処理のソースを変更した場合は、先に `../note-wheel/build.cmd` で再生成してください。

回帰テストはPowerShellから次のように実行します。テンプレートは手元のものを指定してください。

```powershell
$out = Join-Path $PWD ('build/test-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $out | Out-Null
& ./build/custom-test.exe (Resolve-Path ../../cherry_1/chysharp.bin).Path $out
```

全項目ON・全項目OFF・16項目それぞれを1つずつOFFにした18通りを確認します。既存修正の保持、ホイール無効時の入口、上書き拒否、テンプレートの変更検出に加え、Windowsで生成物のアクセラレータとメニューを読み込み、キー割り当てと表示を検証します。ショートカット項目だけを無効にした生成物は、従来の「chysharp全項目有効版＋note-wheelパッチ」とバイト単位で一致します。

実画面でもCtrl+Sによる新規保存・上書き保存と、Ctrl+Zによる音量イベント編集の取り消しを確認しています。取り消した状態を再保存したCHYファイルは、編集前とバイト単位で一致しました。

1.3ではCtrl+Aによる全選択、Ctrl+Xによる切り取りとCtrl+Zによる復元、Ctrl+Oによる読み込み、Ctrl+Nによる新規作成、Ctrl+Shift+Sによる別名保存も実画面で確認しています。

`prepare.ps1` は元の `src.rar` から初期移植を再現する開発用スクリプトです。出力先は `build/upstream/` で、通常のビルドでは使用しません。以降の開発対象はGit内の `src/` です。対応アーカイブのSHA-256：

```text
55C4FD47224A6949F3589986E288B1914B5DDC297BA11F0B4ACC400D011D640D
```

## 原作者表記

元のchysharpソース冒頭にある `written by gocha, feel free to distribute ;)` の表記を保持しています。リソースにも元の著作者表記を残しています。本改造版は独自の派生版で、原作者による公式更新ではありません。

元のソース配布物には統一的なLICENSEファイルが見当たらないため、リポジトリ全体をMIT等のライセンスで扱う宣言はしていません。Cherry本体・`chysharp.bin` の権利は、パッチャーのソースとは別です。
