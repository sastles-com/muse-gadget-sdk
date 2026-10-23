# AtomS3R port of the Muse Gadgets ESP32 SDK（作業中）

M5Stack **AtomS3R** を Muse Gadgets（ESP32 Device SDK）に接続するための
移植作業リポジトリ。上流は Meta の公式SDK
[facebookincubator/muse-gadget-sdk](https://github.com/facebookincubator/muse-gadget-sdk)
（Apache-2.0）で、このリポジトリはその fork として公開し、誰でも同じ手順で
AtomS3R を Muse ガジェット化できるようにすることを目的とする。

## ステータス

**下調べと雛形の段階。まだビルドも実機動作もしていない。**

- [x] 上流リポジトリのボード追加手順（`esp32/devices/AGENTS.md`）を確認
- [x] AtomS3R のピンマップ・パネル仕様をベンダ実装（M5GFX / M5Unified）から採取
  → [`esp32/devices/atoms3r.md`](esp32/devices/atoms3r.md)
- [x] ボード定義 overlay の雛形 → [`esp32/devices/sdkconfig.m5stack-atoms3r`](esp32/devices/sdkconfig.m5stack-atoms3r)
- [ ] `main/led_status.c` / `main/Kconfig.projbuild` に表示バックエンドを実装
- [ ] ESP-IDF v6.0.1 でビルド通過
- [ ] 実機でペアリング確認（複数台での同時接続・連携も検証予定）
- [ ] isolation-sphere（AtomS3R を内蔵する自作球体ディスプレイ）への統合可否を評価

## 方針

- **ベース固定**: 上流 commit `e6fe517`（2026-10-21 時点の main）に `atoms3r`
  ブランチを切って作業する。上流の更新を取り込むときは差分を明示する。
- **ステータス画面プロファイル**: AtomS3R にはマイク/スピーカーがないため、
  フルUI（アバター/音声）ではなく、128×128 LCD にリンク状態・エッジバー・
  アニメーション・画像を表示するプロファイルにする。PSRAM 8MB があるので
  トンネルと画像表示は有効なまま。
- **パネル2リビジョン対応**: AtomS3R の LCD は GC9107 と ST7735S の
  2リビジョンが存在する（M5GFX はパネルIDで自動判別）。片方決め打ちにしない。
- **バックライトは GPIO ではない**: I2C（SDA=GPIO45 / SCL=GPIO0）上の
  アドレス 0x30 のチップ経由。詳細は `atoms3r.md`。

## GitHub 公開の手順（予定）

1. 自分の GitHub アカウントで上流を fork（またはこの clone を新規リポジトリとして push）
2. `atoms3r` ブランチを公開し、README（本ファイル）を入口にする
3. 実機で動いた段階で、GitHub Actions（ESP-IDF v6.0.1）でビルドした
   ファームウェアを Releases に添付する
   - **SDK トークンはバイナリに含めない**。利用者は各自のトークンを
     シリアル経由または Web フラッシャで書き込む（上流 README の手順）
4. 安定したら上流への PR も検討する

## ライセンス

上流の Apache License 2.0 を継承する。各ファイルの著作権ヘッダは上流の
形式を維持する（この fork で追加したファイルも同様）。
