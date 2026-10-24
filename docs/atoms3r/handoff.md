# AtomS3R ポート ハンドオフ

このファイルは「いま何が終わっていて、次に何をするか」を引き継ぐための
生きたメモ。作業したらその場で更新すること。

最終更新: 2026-10-24（Muse、Phase 2 実機確認まで完了）

## 現在地

**Phase 2 実機確認完了：v2 を書き込み、`display.text` と `sensors.read` が実機で動作。**

- register に新コマンド2つが載り、デバイス一覧でも宣言を確認
- `sensors.read`: 実データ取得 OK。机に画面を上にして置いた状態で
  加速度 (0.107, -1.000, -0.036) g — 重力はチップ座標系の -Y 方向。
  ジャイロは静止でほぼ 0 dps
- 既知の癖: 起動後の初回 `sensors.read` は全軸 0 が返る（lazy init 直後で
  初回サンプル未着のため）。2回目以降は正常。初回はサンプル到着まで
  待つか no_readings を返すよう直すのが望ましい（未修正）
- `display.text`: 実行 OK（2行・scale 3 で描画）。色の見え方は目視確認待ち

- `sensors.read`：BMI270 IMU（加速度 g / ジャイロ dps、x/y/z）を追加。
  レジストリの `espressif/bmi270` ドライバ経由、初回コマンド時に
  0x69 → 0x68 の順でプローブして遅延初期化（`HOMEHUB_ATOMS3R_IMU`）
- `display.text`：ピクセルフォントでテキストを画面に描画
  （`HOMEHUB_ATOMS3R_DISPLAY_TEXT`）。折り返し・改行・中央寄せ、
  `color`（#rrggbb）と `size`（1–4、既定は自動フィット）対応
- バックライトと IMU で内部 I2C バス（SDA45/SCL0）を共有するため
  `atoms3r_i2c_bus()` アクセサを新設し、バックライト側もそれに移行
- 検証（ESP-IDF v6.0.1）：クリーンビルド成功（muse-gadget.bin
  0x1a1000、19% 空き）、生成 sdkconfig に新 CONFIG 2 件の適用を確認、
  ホストテスト 633 件 OK（skipped 7、Phase 1 の 629 + 新規 4）、
  ideaspark / SenseCAP Indicator の回帰ビルド成功（他ボードの
  sdkconfig・ELF に新機能が混入しないことも確認）
- **IMU とテキスト描画は実機未確認**。書き込み後に下記「次のアクション」
  4 で確認する

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

## Phase 2 実装メモ（2026-10-24、実装済み・実機未確認）

準備メモからの変更点と判断:

- コマンド名は `sensor.imu` ではなく `sensors.read` にした
  （esp32/AGENTS.md の作法：環境センサー系は全ボード共通の
  `sensors.read` で返す。結果も SenseCAP / reTerminal と同じ
  `{value, unit, age_s}` の形で、value が {x, y, z} のベクトル）
- BMI270 のコンフィグ配列は自前ベンダリングせず、レジストリの
  公式コンポーネント `espressif/bmi270` 1.1.0（esp-bsp、Apache-2.0）
  を採用。`bmi270_create()` がチップ ID 確認・ソフトリセット・
  Bosch コンフィグのアップロードまで行う。副作用として
  `espressif/sensor_hub`・`i2c_bus`・`cmake_utilities` が
  managed_components に入るが、他のボードではダウンロードのみで
  コンパイル・リンクされない（回帰ビルドで確認済み）
- アドレスは M5Unified の BMI270 クラスの既定に倣い 0x69 を先に、
  駄目なら 0x68 を試す。チップ ID (0x24) の確認はドライバ内蔵
- 軸の向きはチップの座標系のまま返す。M5Unified は AtomS3R 用に
  X/Y 入れ替え + Y 反転で基板座標に直している。どちらが自然かは
  実機で寝かせた状態の重力方向を見て決める（下記ブロッカー）
- `display.text` の描画は `led_status_draw_rect()` 経由なので、
  最初の描画でアニメーションが消え `display.show_animation` で
  戻る（draw_url と同じ挙動）。色のバイト順はタイトル描画
  （lcd_px）の慣例に合わせた

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
- [x] Phase 2 実装：I2C バス共有化、`sensors.read`（BMI270）、
      `display.text`（上記「Phase 2 実装メモ」参照）。ビルド・
      ホストテスト・回帰ビルド通過、コミット済み。実機は未確認

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
5. **Phase 2 実機確認**（書き込み後、Phase 1 のペアリング済み実機で）
   - 再登録後の `link.register` に `sensors.read` と `display.text` が
     載ること（登録 JSON が 8 KB 制限を超えていないこと）
   - `sensors.read`：起動ログではなく初回呼び出しで
     `link.imu: BMI270 at 0x69: ...`（または 0x68）が出ること。
     画面を上にして平置きしたとき、加速度の重力方向が物理と合うか
     （軸リマップ要否の判断材料）。傾け・回転でジャイロが反応すること
   - `display.text`：短文・長文（折り返し）・`color`・`size` 指定で
     描画を確認。色と上下左右が正しいか（バイト順と反転の確認）
   - バックライトが Phase 1 と変わらず点くこと（I2C バス共有化の
     回帰確認）

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
- Phase 2 の実機リスク（すべて未確認）：
  - BMI270 のアドレス（0x69 優先 / 0x68）と、Bosch コンフィグ
    アップロード後のデータ有効性。駄目なら初回 `sensors.read` が
    `no_readings` を返し続けるので判別できる
  - 軸の向きはチップ座標系のまま。M5Unified 流のリマップ
    （X/Y 入れ替え + Y 反転）を入れるかは実機の重力方向を見て決める
  - `display.text` の色バイト順はタイトル描画の慣例に合わせたが、
    実機で色が入れ替わって見えたら `display_text.c` の
    `parse_color()` のバイトスワップを外す
  - コマンド 2 件追加で `link.register` の JSON が増えた
    （上限 8 KB）。登録自体が失敗していないか再接続時に確認する
