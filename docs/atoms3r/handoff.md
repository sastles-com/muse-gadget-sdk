# AtomS3R ポート ハンドオフ

このファイルは「いま何が終わっていて、次に何をするか」を引き継ぐための
生きたメモ。作業したらその場で更新すること。

最終更新: 2026-10-24（Muse、実機 bring-up とペアリング完了）

## 現在地

**Phase 1 完了：実機ペアリング成功、Muse からコマンド実行を確認。**

- 実機（MAC ac:27:6e:d1:93:7c、node homelink-d1937c）へ書き込み・起動成功。
  表示（GC9107 仮定駆動でアニメーション再生）、ボタン GPIO41、BLE、Wi-Fi
  スキャンすべて動作
- SDK トークンをシリアルコンソールから保存（`>sdk_token=...` + LF）
- Muse アプリ（iPhone）でペアリング完了：デバイス一覧に
  `MuseGadget-Disp-D1937C`（platform esp32）として online 表示
- `device.health` OK（接続 SSID aterm-05cc60-5p）、`display.draw_file` で
  128x128 JPEG の描画成功（Phase 1 の受け入れ条件を達成）
- bring-up の知見：
  - 工場ファームのフラッシュモードは DIO 8MB 80MHz（ダンプで確認）
  - esptool v5 は `--flash_mode` 等のアンダースコア形式オプションが必須
  - パネル ID は 0x004883ff と読め既知 ID に一致しないが、GC9107 仮定で
    表示は正常（ID 読みのバイト並び切り分けは未完の宿題）
  - iPhone アプリで SSID 選択直後に「接続中」のまま止まる事象があった。
    端末側には provision が届いておらず、アプリ再起動で解消した
  - 自宅 Aterm（Wi-Fi 7 / 11be ON）への接続自体は問題なかった

- ESP-IDF v6.0.1 で `tools/board.sh m5stack-atoms3r build` 成功
  （muse-gadget.bin 0x1a1000、最小アプリパーティション 0x200000 に
  対して 19% 空き）。overlay の全行が生成 sdkconfig に適用されたことを
  レシピ手順 8.2 で確認済み
- ホストテスト 629 件 OK（skipped 7）。`led_status.c` を触ったため
  ideaspark / SenseCAP Indicator の回帰ビルドも通過を確認
- **実機は未経験**。書き込み・ペアリング確認はこれから

## Phase 2 準備メモ（2026-10-24、未着手・範囲確認待ち）

コマンドは `esp32/main/app.c` のディスパッチに追加する形（既存:
`display.draw_file` / `display.draw_url` / `display.show_animation` /
`voice.configure`）。追加候補:

1. `display.text` — 文字列を 128x128 画面に描く（チャットからテキストを
   送って表示する「テキスト応答」の最小形）
2. `sensor.imu` — AtomS3R 内蔵 IMU (BMI270) の加速度・ジャイロを読む。
   バックライト制御で既に作っている I2C バス (SDA45/SCL0) を共有できる
   見込み（IMU も同じバス上）

## 完了済み

- [x] 上流 SDK の調査：対応ボード、ボード追加レシピ
      （`esp32/devices/AGENTS.md`）を確認
- [x] ハードウェア事実の採取（M5GFX / M5Unified から、出典付き）
      → `esp32/devices/atoms3r.md`
- [x] overlay 雛形 `esp32/devices/sdkconfig.m5stack-atoms3r`
- [x] GitHub 公開：sastles-com/muse-gadget-sdk に fork、`atoms3r`
      ブランチへ雛形一式を push 済み（2026-10-24）
- [x] ドキュメント整備：本 `docs/atoms3r/`（overview / design / handoff）
- [x] 表示バックエンド実装（下記「次のアクション」1 の内容すべて）
- [x] `tools/board.sh` に `m5stack-atoms3r` case 追加
- [x] ビルド検証（この VM 上の ESP-IDF v6.0.1。Air での再現は未実施）
- [x] `devices/README.md` のボード表に行を追加

## 次のアクション（順番に）

1. ~~**表示バックエンド実装**（Muse 担当）~~ **完了**
   - `main/Kconfig.projbuild`：`HOMEHUB_LED_BACKEND_M5STACK_ATOMS3R`
     （`depends on IDF_TARGET_ESP32S3`）と `HOMEHUB_DISPLAY` の
     `default y if` を追加
   - `main/led_status.c`：AtomS3R ブロック追加。既存の4つの `#if` 分岐点
     （include、lcd_draw/lcd_from_be、lcd_panel_init/lcd_panel_on、
     ピクセルコピー）を ideaspark / SenseCAP の並びに合わせて拡張
   - パネル ID 判別（GC9107 `0x079100` / ST7735S `0x7683`,`0x897C`）と
     両初期化パス。ID 読みは 3-wire SPI（sio_mode）の panel IO で
     RDDID(0x04) を4バイト読み、M5GFX と同じマスクで判定。無応答時は
     100 kHz で再プローブし、それでも不明なら警告ログを出して
     GC9107 とみなす。ST7735S は IDF 内蔵 ST7789 ドライバで駆動
     （gap 2/1、反転 on）する v1 方式。両リビジョンとも M5GFX に倣い
     mirror(true, true)（= offset_rotation 2）
   - バックライトの I2C シーケンス（SDA45/SCL0、0x30、初期化レジスタ
     0x00/0x08/0x70、輝度 0x0E に 0xFF）を `led_hw_init()` で実行
   - `main/idf_component.yml` に `espressif/esp_lcd_gc9107` を追加し、
     `main/CMakeLists.txt` の `GADGET_REQUIRES` に `if()` 条件付きで追加
     （led_strip と同じ形。マニフェスト側に `rules:` は付けない：
     理由は `esp32/components/esp_lcd_gc9107/VENDORED.md` 参照）
   - 128 行パネルではタイトル（デバイス名）がアニメーション上に
     入らないため、`led_hw_set_title()` に `LCD_TITLE_Y < 0` の
     ガードを追加し、このボードでは名前を表示しない（AIPI Lite と
     同じ割り切り）
2. ~~**tools/board.sh に `m5stack-atoms3r` case 追加**~~ **完了**
3. **実機 bring-up**（片野 が書き込み、ログを共有）
   - 起動ログで `M5Stack AtomS3R panel: GC9107/ST7735S (ID 0x........)` の
     検出 ID、`LED status ready: M5Stack AtomS3R 128x128 display` を確認
   - 画面の上下・左右が正しいか（mirror の当てが外れていたらここで直す）、
     バックライト点灯、ステータスバーの表示を確認
   - BLE アドバタイズ名（サフィックス `-Disp`）とペアリングを確認
   - Flash モード（DIO/QIO）の確定もこのとき（下記ブロッカー参照）
4. 実機で動いたら `atoms3r` ブランチへ push し、記事化へ

## 環境メモ

- 作業リポジトリ（片野）：MacBook Air の `~/work/muse-gadget-sdk`、
  ブランチ `atoms3r`
- Muse の作業コピー：別途保持。2026-10-24 に Deploy key（muse-vm、
  Read/write）が登録され、Muse から `atoms3r` ブランチへ直接 push
  できるようになった（疎通確認済み）。Air の clone は `git pull` で同期する
- ビルドには SDK トークン（gadgets.muse.ai）が必要。**トークンは
  コミットしない・チャットに貼らない**。ビルドディレクトリの
  sdkconfig にだけ設定する
- 実機：AtomS3R 複数台あり（球内蔵の1台は実験に使わない。予備機を使う）
- isolation-sphere のサーバ機 NucBox-G10 は Muse の Linux ガジェット
  としてペアリング済み（将来の評価環境）

## ブロッカー / 注意

- Flash モード（DIO/QIO）未確定（design.md 未確定事項 #1）。
  ビルドはデフォルトのまま通過したが、書き込み前に工場ファームの
  esptool ダンプ（flash mode の表示）で確認する
- レジストリの `espressif/esp_lcd_gc9107`（1.0.0 / 2.0.0 とも）は
  ESP-IDF v6.0.1 でコンパイルできない（`rgb_endian` 改名に未追随）。
  2.0.0 を `esp32/components/esp_lcd_gc9107/` に vendoring し、IDF 6
  向け最小パッチを当ててある（詳細 `VENDORED.md`）。上流に修正版が
  出たらディレクトリごと削除してレジストリ版へ戻す。上流
  （esp-iot-solution）への issue/PR も検討する
- 上流 main は進行が速い。`atoms3r` ブランチのベースは
  commit `e6fe517`（2026-10-21）。上流追従時は差分を明示すること
- パッチ適用は `git am`。コミットは小さく分け、メッセージに
  「未ビルド/未実機」を正直に書く（上流 AGENTS.md の作法）
