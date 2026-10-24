# AtomS3R ポート 設計仕様

上流の設計規約は `esp32/AGENTS.md` と `esp32/devices/AGENTS.md`（ボード追加
レシピ）に従う。本書は AtomS3R 固有の決定事項を定める。

## 対象ハードウェア

| 項目 | 仕様 |
|---|---|
| モジュール | ESP32-S3-PICO-1-N8R8（ESP32-S3、Flash 8MB、Octal PSRAM 8MB） |
| ディスプレイ | 0.85" 128×128 IPS、SPI3。パネルは2リビジョン（下記） |
| 入力 | 前面（画面）ボタン GPIO41。マイク・スピーカー・タッチなし |
| USB | Native USB-Serial-JTAG（`303a:1001`、usbmodem/ttyACM） |
| 電源 | USB給電（バッテリなし） |

事実の出典はすべて `esp32/devices/atoms3r.md` に出典付きで記録済み。
ピンは推測で決めない（レシピ step 1）。

## プロファイル選定

**ステータス画面プロファイル**を採用する（フル UI ではない）。

- 根拠: AtomS3R にはマイク/スピーカーがなく、フル UI（アバター/音声）の
  前提を満たさない。一方 PSRAM 8MB があるため、ステータス画面でも
  トンネルと画像表示（`display.*`）は有効にできる
- 表示内容: リンク状態（エッジバー＋アニメーション）、エージェント名、
  接続ドット、Muse からの画像
- BLE アドバタイズ名: `MuseGadget-Disp-XXXXXX`
  （overlay で `CONFIG_HOMEHUB_BLE_NAME_SUFFIX="-Disp"` を設定）

## ディスプレイ設計

### ピン配置（M5GFX の自動判別コードより）

SPI3_HOST、mode 0、3-wire、書込 40MHz / 読出 16MHz。
MOSI=GPIO21、SCLK=GPIO15、DC=GPIO42、CS=GPIO14、RST=GPIO48、MISO なし。

### パネル2リビジョンの自動判別（必須）

起動時に CS=GPIO14 経由でパネル ID を読み、初期化を分岐する
（M5GFX と同じ方式。決め打ちは禁止）。

| パネル | ID | 初期化パラメータ |
|---|---|---|
| GC9107 | `0x079100` | 128×128、offset_y=32。ID が読めないロットがあるため、失敗時は 100kHz で再試行してから諦める |
| ST7735S | `0x7683` / `0x897C` | 128×128、memory height 132、offset_x=2、offset_y=1、invert=true |

検出したパネル ID は起動ログに出す（初回 bring-up の確認項目）。

### バックライト（GPIO ではない）

内部 I2C バス（SDA=GPIO45、SCL=GPIO0。IMU と共用。SCL が BOOT
ストラップピンである点に注意）上のアドレス `0x30` のチップを叩く。

- 初期化: reg `0x00` ← `0x40`、reg `0x08` ← `0x01`、reg `0x70` ← `0x00`
- 輝度: reg `0x0E` ← 値

`main/led_status.c` の `led_hw_init()` は通常「BL ピンを High にする」
実装だが、本ボードでは I2C 書込シーケンスに置き換える。

### ジオメトリ

`LCD_H_RES = LCD_V_RES = 128`。アニメーションは 56×59 セルのため
`LCD_ANIM_SCALE = 2`（112 ≤ 128）。ピクセル転送は SPI パネル慣例の
ビッグエンディアン RGB565（ideaspark と同系）。

### パネルドライバの調達

GC9107 / ST7735 とも SDK 内に既存ドライバなし。ESP-IDF v6.0.1 の
`esp_lcd` 同梱分を確認し、GC9107 はレジストリコンポーネント
`espressif/esp_lcd_gc9107` を `main/idf_component.yml` に追加し、
`main/CMakeLists.txt` の `GADGET_REQUIRES` に本バックエンド条件で加える
（レシピ step 5）。

## Kconfig / overlay 設計

- バックエンド名: `HOMEHUB_LED_BACKEND_M5STACK_ATOMS3R`
  - `main/Kconfig.projbuild` の `HOMEHUB_LED_BACKEND` choice に追加
  - `HOMEHUB_DISPLAY` に `default y if HOMEHUB_LED_BACKEND_M5STACK_ATOMS3R` を追加
- overlay: `esp32/devices/sdkconfig.m5stack-atoms3r`（作成済み・雛形）
  - target=esp32s3、CPU 240MHz
  - `CONFIG_HOMEHUB_BUTTON_GPIO=41`（GPIO0 は I2C SCL のため BOOT を流用不可）
  - Flash 8MB（モード DIO/QIO は**未確定**。工場ファームのダンプか M5 の
    公開設定で確認してから確定する）
  - PSRAM: Octal 80MHz、TLS バッファ確保のため NimBLE 割当を外部メモリへ

## ボタン設計

- 前面ボタン（GPIO41、アクティブ Low、内部プルアップ）:
  短押し=ペアリング承認／セットアップ再開、5秒長押し=セットアップ
  リセット（上流の標準ジェスチャをそのまま使う）

## ビルド / 検証手順（予定）

1. `tools/board.sh` に `m5stack-atoms3r` case を追加（target、overlay、
   PORTS は usbmodem/ttyACM）
2. `tools/board.sh m5stack-atoms3r build`（ESP-IDF v6.0.1）
3. レシピ step 8 の overlay 適用チェック、`check_sizes` の空き確認
4. ホストテスト: `python3 -m unittest discover -s tests -p 'test_*.py'`
5. 実機: 書き込み→起動ログで `link.main: Muse Gadget starting`、
   パネル ID 検出、オレンジ呼吸、BLE スキャンで `MuseGadget-Disp-XXXXXX`
6. ペアリング → テキスト/画像表示 → 複数台同時接続の順に検証

## セキュリティ・運用上の決定

- **SDK トークンをコミットしない**（ビルドディレクトリの sdkconfig は
  gitignore 対象。上流 AGENTS.md の規約）
- Releases にファームウェアを添付する際もトークンを焼き込まない。
  利用者は自らのトークンをシリアル/Web フラッシャで設定する
- NVS 暗号化の有効化を推奨事項として README に明記する（Wi-Fi 資格情報と
  トークンの保護）

## 未確定事項

| # | 項目 | 確定方法 |
|---|---|---|
| 1 | Flash モード（DIO/QIO）と周波数 | 工場ファームのダンプ or M5 公開設定 |
| 2 | 手元の複数台のパネルリビジョン内訳 | 初回起動ログの検出 ID で確認（自動判別するため blocking ではない） |
| 3 | IMU を独自コマンドに使うか | Phase 2 で判断（`sensors.read` への統合が上流の流儀） |
| 4 | isolation-sphere 統合方式（ネイティブ統合/G10ブリッジ/ハイブリッド） | 単体実験の実測後に別途評価 |
