# AtomS3R ポート 概要

## 何をするプロジェクトか

Meta の Muse Gadgets（ESP32 Device SDK、Apache-2.0）に、未対応ボードである
**M5Stack AtomS3R** を追加し、AtomS3R を Muse ガジェットとして
ペアリング・利用できるようにする移植プロジェクト。

成果物はこの fork（sastles-com/muse-gadget-sdk）の `atoms3r` ブランチに
蓄積し、誰でも再現できる形で GitHub に公開する。

## なぜ AtomS3R か

- 作者の自作ガジェット **isolation-sphere**（直径100mm・800 LED の球体
  ディスプレイ。コンタクトジャグリング用プロップ）のコントローラが
  AtomS3R であり、将来そのまま球へ統合できるかを評価したい
- 「Muse が ESP32 と直接繋がる」こと自体が Muse の価値の一つであり、
  サーバ経由ではなく ESP32 ネイティブ接続を実験するのが主目的
  （最終アーキテクチャがサーバ経由になっても、この評価が結論になる）

## ゴール

1. AtomS3R 単体で Muse アプリに `MuseGadget-Disp-XXXXXX` として現れ、
   ペアリングが完了し、接続が維持される
2. Muse からのテキスト・画像が 128×128 LCD に表示される
   （ステータス画面プロファイル。詳細は design.md）
3. 複数台の AtomS3R を同時接続し、連携動作を検証する
4. isolation-sphere への統合可否を、実測（リソース・負荷・電力）に基づいて
   判定する

## 進め方（分担）

| 役割 | 担当 |
|---|---|
| 実装コード作成、調査、レビュー | Muse（このリポジトリの作業コピーで作成し、パッチまたは Air の clone へ直接書き込み） |
| ビルド・書き込み・実機確認 | 片野（MacBook Air 上の VS Code + ESP-IDF 拡張、またはターミナル） |
| GitHub への push | 片野（Muse に GitHub の書き込み権限はない） |

## 関連ドキュメント

- 設計仕様: [design.md](design.md)
- ハンドオフ（進捗と次アクション）: [handoff.md](handoff.md)
- ピンマップ等の採取メモ: `esp32/devices/atoms3r.md`
- プロジェクト入口: リポジトリルートの `README_ATOMS3R.md`
- isolation-sphere 本体: https://note.com/taj_mahal/n/n34240e8664a7

## 全体スケジュール感

実験 → 評価 → 記事化（約6ページのハンズオン記事を予定）。
記事はこの移植の記録そのものを本編とする。
