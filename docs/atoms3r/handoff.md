# AtomS3R ポート ハンドオフ

このファイルは「いま何が終わっていて、次に何をするか」を引き継ぐための
生きたメモ。作業したらその場で更新すること。

最終更新: 2026-10-24（Muse）

## 現在地

**Phase 1 の入口。下調べと雛形は完了、実装は未着手。ビルド・実機は未経験。**

## 完了済み

- [x] 上流 SDK の調査：対応ボード、ボード追加レシピ
      （`esp32/devices/AGENTS.md`）を確認
- [x] ハードウェア事実の採取（M5GFX / M5Unified から、出典付き）
      → `esp32/devices/atoms3r.md`
- [x] overlay 雛形 `esp32/devices/sdkconfig.m5stack-atoms3r`
- [x] GitHub 公開：sastles-com/muse-gadget-sdk に fork、`atoms3r`
      ブランチへ雛形一式を push 済み（2026-10-24）
- [x] ドキュメント整備：本 `docs/atoms3r/`（overview / design / handoff）

## 次のアクション（順番に）

1. **表示バックエンド実装**（Muse 担当）
   - `main/Kconfig.projbuild`：`HOMEHUB_LED_BACKEND_M5STACK_ATOMS3R` 追加
   - `main/led_status.c`：AtomS3R ブロック追加。既存の4つの `#if` 分岐点
     （include、lcd_draw/lcd_from_be、lcd_panel_init/lcd_panel_on、
     ピクセルコピー）を ideaspark / SenseCAP の並びに合わせて拡張
   - パネル ID 判別（GC9107 `0x079100` / ST7735S `0x7683`,`0x897C`）と
     両初期化パス。GC9107 は低速再プローブ付き
   - バックライトの I2C シーケンス（0x30、初期化レジスタ 0x00/0x08/0x70、
     輝度 0x0E）
   - `main/idf_component.yml` に `espressif/esp_lcd_gc9107` を条件付き追加
2. **tools/board.sh に `m5stack-atoms3r` case 追加**（Muse 担当）
3. **ビルド環境構築とビルド**（片野 担当：MacBook Air の VS Code +
   ESP-IDF 拡張で v6.0.1。詰まったらログを Muse へ）
4. **実機 bring-up**（片野 が書き込み、ログを共有）
   - 起動ログ、検出パネル ID、BLE アドバタイズ名を確認
5. ビルドが通ったら `devices/README.md` と上位 README/AGENTS の表へ追加し、
   `atoms3r` ブランチへ push

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
  ビルド前に工場ファームの esptool ダンプ（flash mode の表示）で確認する
- 上流 main は進行が速い。`atoms3r` ブランチのベースは
  commit `e6fe517`（2026-10-21）。上流追従時は差分を明示すること
- パッチ適用は `git am`。コミットは小さく分け、メッセージに
  「未ビルド/未実機」を正直に書く（上流 AGENTS.md の作法）
