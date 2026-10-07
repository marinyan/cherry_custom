# Cherry ホイールスクロール追加パッチ

カーソルを合わせたペインのスクロールバーを、ホイールで動かします。クリックしてフォーカスを移す必要はありません。

- 通常のホイール：縦スクロール。ノート／イベントリスト、トラックリスト、ピアノロール、コントロール表示などが対象です。
- チルトホイール／サイドホイール：横スクロール。マウスから Windows の横ホイール入力が届く場合に対応します。
- 子ペインに横スクロールバーがない場合は、そのペインを含むコンテナーの共通スクロールバーを使います。
- 縦と横の端数入力を別々に蓄積し、Windows のスクロール行数／文字数に従います。ページ単位やスクロール無効の設定も扱います。
- 標準のコンボボックスやテキスト入力欄は、元の操作を維持します。

## すぐに使う

パッチを適用して生成される実行ファイルを起動してください。未改造のCherry 1.4.3からは `cherry-wheel.exe`、対応するchysharp適用版からは `cherry-sharp-wheel.exe` が生成されます。入力の `cherry.exe` を起動した場合はパッチ適用前の動作です。

Git に登録するパッチには Cherry 本体と適用済みの実行ファイルを含めていません。Git から取得した場合は、リポジトリ直下の `cherry_1` フォルダーに下記の対象原本と通常の動作に必要な同梱ファイルを配置してから、パッチを適用してください。

入力の `cherry.exe` は変更しません。パッチは外部の常駐ソフトや追加 DLL を必要としません。

## パッチの当て方

1. 使用中の Cherry で必要なデータを保存し、終了します。
2. このフォルダー構成を維持して、`patches/note-wheel/apply.cmd` をダブルクリックします。入力の版を自動判別します。chysharp版専用の `apply-sharp.cmd` も用意しています。
3. `Created:` または `Already applied:` が出たら適用完了です。
4. 表示された出力ファイルを起動します。未改造版なら `cherry_1/cherry-wheel.exe`、chysharp版なら `cherry_1/cherry-sharp-wheel.exe` です。

コンパイラーのインストールは不要です。`apply.cmd` は同梱の `wheel-hook.bin` を使います。

対象は **未改造の Cherry 1.4.3** と **chysharp全項目有効版** です。末尾に記載した SHA-256 と一致する入力に限って適用します。chysharpの一部項目を無効にした版や、異なるバージョン・その他の改造版は受け付けません。

## chysharp版を使う場合

1. `chysharp.exe` で全項目を有効にして作成した `cherry.exe` を、`cherry_1` に置きます。
2. `apply-sharp.cmd` をダブルクリックします。
3. 作成された `cherry_1/cherry-sharp-wheel.exe` を起動します。

`apply-sharp.cmd` は未改造版を渡すとエラーで停止します。`apply.cmd` はどちらも自動判別します。

旧ホイール処理の起動を置き換えるため、二重スクロールは発生しません。RPN/NRPN、歌詞出力、アフタータッチなどの既存修正と、chysharpが追加したコード・データは保持します。

適用順は **chysharp → このパッチ** です。今回の出力に `chysharp.exe` を後から適用すると実行ファイルが作り直され、今回のホイール対応は失われます。

## 別の場所へ適用する

別の場所にある同じバージョンへ適用する場合は、PowerShell で次を実行します。入力と出力は必ず別の名前にしてください。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "C:\path\patches\note-wheel\apply.ps1" -Source "C:\Cherry\cherry.exe" -Output "C:\Cherry\cherry-wheel.exe"
```

chysharp版を明示的に指定する場合：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "C:\path\patches\note-wheel\apply.ps1" -Variant ChySharp -Source "C:\Cherry\cherry.exe" -Output "C:\Cherry\cherry-sharp-wheel.exe"
```

`-Output` を省略すると入力ファイルと同じフォルダーに、判別した版に応じた名前で出力します。

同じパッチがすでに適用されていれば再実行しても変更しません。出力先に異なるファイルがある場合は上書きせず止まります。そのファイルを別名で保管してから実行するか、`-Output` に別のファイル名を指定してください。

## 元に戻す

生成された `cherry-wheel.exe` または `cherry-sharp-wheel.exe` を終了し、入力に使った `cherry.exe` を起動するだけです。元ファイルへの復元パッチは不要です。

## マウス側の設定

チルト／サイドホイールは、マウス設定ソフトで「横スクロール」に割り当ててください。「戻る／進む」やキー入力への割り当ては横ホイール入力ではありません。スクロール対象のペインにカーソルを置いて操作します。

## 開発用

`build.cmd` で `wheel-hook.bin` を再生成し、端数・方向反転・複数ノッチ・スクロール無効・ページ単位などのテストを実行します。Visual Studio の C++ x86 ツールと Windows SDK が必要です。

両方の入力版に対する適用テストは、未改造版とchysharp版を別々に用意して実行します。テスト用ファイルはGit対象外の `build` 内に作成します。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\test-patch.ps1 -OriginalSource "C:\Cherry-original\cherry.exe" -SharpSource "C:\Cherry-sharp\cherry.exe"
```

`wheel.c` が処理本体、`wheel-plan.h` がスクロール量の計算、`apply.ps1` が適用処理です。既存コードの WinMain と API 呼び出し位置は、付属の `cherry_1/src.rar` 内の `src/chysharp-mwhook.asm` を根拠にしています。元の機械語コードへの変更は WinMain 呼び出し先の差し替えで、追加コード用の PE セクションと再配置情報を追記します。

対応する入力の SHA-256:

```text
Cherry 1.4.3:
B562CB56BAFF6651DFCA0A14A79B19E261E4282319A5911D3238E56186B1CD54

chysharp全項目有効版:
659FA2948FC22689E2D87C0F36E33A504E9D94B24693D4E3315F5904E66C9FB3
```

## 確認した動作

- 実際の Cherry 画面で、ノートリストとトラックリストの上下スクロールを確認。
- トラックリストとピアノロールの横スクロールを、横ホイール入力で確認。
- ピアノロールと下側コントロール表示が、それぞれカーソルを置いたペインだけ縦スクロールすることを確認。
- C の計算テストに加え、原本への上書き拒否、対象外バイナリの拒否、異なる既存出力の保護、再適用時の無変更を確認。
- 未改造版とchysharp版の両方で、既存のコード・データ・リソースが変わらないことをバイト比較。変更はPEヘッダー、WinMain呼び出し先、新規セクションに限定。
- chysharp版の画面で縦横スクロールを確認し、縦1ノッチが設定どおりの3行となることを確認。

横方向は Windows の横ホイール入力による確認です。個別のマウス機種・設定ソフトの割り当ては確認していません。
