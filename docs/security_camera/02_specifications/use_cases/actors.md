# アクター定義 (Actors)

**目的**: 全ユースケースで参照されるアクターを 1 か所に集約し、責任・期待・アクセス権限を統一。

**作成日**: 2026-05-02

---

## 凡例

| 記号 | 意味 |
|---|---|
| 👤 | 人間アクター (Primary actor) |
| 🤖 | システムアクター (機械、自律的) |
| 🚨 | 敵対的アクター (脅威モデル用) |

---

## 👤 運用者 (Operator)

**役割**: 日常稼働の監視、PC viewer 操作。

| 項目 | 内容 |
|---|---|
| 責任 | システム稼働状態の確認、録画ファイルの確認、必要時の手動録画開始/停止 |
| 期待 | リアルタイム映像が見られる、録画されている、エラーは目立つように表示される |
| アクセス権限 | PC viewer GUI の全機能、録画ファイル参照 (ローカル FS) |
| 想定環境 | 屋内 (Q25)、Linux/WSL2 PC (Q21) |
| 関連 UC | UC-1 (起動)、UC-2 (ストリーミング閲覧)、UC-3 (動き検出時録画)、UC-4 (録画ファイル管理) |

---

## 👤 設置者 (Installer)

**役割**: 初期セットアップ。WiFi 設定、Spresense 配置、PC viewer インストール。

| 項目 | 内容 |
|---|---|
| 責任 | WiFi SSID/Password 設定、Spresense ファームウェア書き込み、PC viewer ビルド |
| 期待 | セットアップ手順が明確、トラブル時の診断情報がある |
| アクセス権限 | Spresense ソースコード (`wifi_config.h` 編集)、ファームウェア書き込み権限、PC でビルド |
| 想定環境 | 開発 PC (Linux/WSL2)、Spresense ハードウェア物理アクセス |
| 関連 UC | UC-1 (起動)、UC-6 (設定変更) |
| 注意 | WiFi 認証情報のハードコード問題 (X-7) — 設置者がリポジトリ汚染しないように注意 |

---

## 👤 保守者 (Maintainer)

**役割**: 障害時の人手介入、ログ解析、改善提案。

| 項目 | 内容 |
|---|---|
| 責任 | システム障害時の診断と復旧、長期稼働中のメンテナンス、Phase 12 以降の進化判断 |
| 期待 | 障害状態が観測可能 (TCP Health Monitor / health state machine)、復旧手順が文書化されている (X-4 運用ランブック) |
| アクセス権限 | Spresense シリアルコンソール、PC viewer ログ、git 履歴 |
| 想定環境 | 開発 PC + 物理アクセス可能 |
| 関連 UC | UC-5 (TCP 切断時の自動復旧)、UC-7 (障害時人手介入) |
| 関連 ADR | ADR-002 v1.1 (再接続が逆効果と判明) |

---

## 🚨 不正侵入者 (Adversary)

**役割**: 同一 LAN 内の盗聴・なりすまし・DoS 試行。脅威モデル用。

| 項目 | 内容 |
|---|---|
| 想定能力 | 同一 WiFi LAN への接続済み (WPA2 突破済 or 招待された Guest)、Wireshark 等のパケットキャプチャ可能 |
| 想定攻撃 | MJPEG パケット盗聴、TCP 8888 への不正接続、改ざん試行、DoS スパム |
| 現実装での防御 | ❌ 殆どなし — 詳細は [`../quality/risk_analysis/SECURITY_GAP_ANALYSIS.md`](../quality/risk_analysis/SECURITY_GAP_ANALYSIS.md) §3 参照 |
| アクセス権限 (与えるべき) | なし (本来) |
| 関連 ADR | (なし — 設計のみ存在の SECURITY_ARCHITECTURE.md, 実装乖離は GAP_ANALYSIS) |
| 関連 QAS | QAS-5 (TLS handshake — 設計のみ・未実装) |
| 関連 UC | (脅威モデル用 — 直接 UC として記述しない、exception_scenarios.md で言及) |

---

## 🤖 Spresense デバイス (System actor)

**役割**: 自律的に動作するハードウェア。電源投入で起動し、カメラキャプチャ → MJPEG 配信を行う。

| 項目 | 内容 |
|---|---|
| 構成 | CXD5602 + ISX012 カメラ + GS2200M WiFi モジュール + 拡張ボード (USB) |
| 自律動作 | 電源投入で `camera_app_main` 起動 (Q18: 自動起動)、5 スレッド並行動作 |
| 状態 | 健全性 6 状態モデル (HEALTHY/CAUTION/WARNING/CRITICAL/RECONNECTING/FAILED) |
| 出力 | TCP/8888 経由の MJPEG ストリーム + 3 秒粒度の metrics packet |
| 制約 | 構造的天井 #1〜#5 ([`../quality/GLOSSARY.md`](../quality/GLOSSARY.md) §2) |
| 関連実装 | `apps/examples/security_camera/` 配下全ファイル |
| 関連 UC | 全 UC でシステム側として登場 |

---

## 🤖 PC viewer プロセス (System actor)

**役割**: TCP/USB 経由でストリームを受信し、表示・録画・動き検出を行う。

| 項目 | 内容 |
|---|---|
| 言語/フレームワーク | Rust + eframe (egui) |
| アーキテクチャ | 3-thread Pipeline (TCP/Serial Reader → JPEG Decoder → GUI Thread)、bounded(3) channel |
| 自律動作 | 運用者による手動起動 (Q18: PC 手動)、後はクラッシュまで自律稼働 |
| 主要機能 | リアルタイム表示 (Q10) / 録画 (Q6, Q7, Q8) / 動き検出 (Q12) / metrics 受信 |
| 制約 | bounded(3) で約 150KB peak (ADR-008)、PC 側ストレージ 1GB 上限 (Q8) |
| 関連実装 | `Rust_ws/security_camera_viewer/src/` (gui_main.rs, pipeline.rs, motion_detector.rs, mp4_recorder.rs) |
| 関連 ADR | ADR-005 (3-thread Pipeline)、ADR-008 (bounded channel) |
| 関連 UC | 全 UC で PC 側として登場 |

---

## アクター間の関係マトリクス (簡易)

| | 運用者 | 設置者 | 保守者 | 不正侵入者 |
|---|---|---|---|---|
| Spresense デバイス | 観測のみ | フル制御 | コンソール経由 | 観測のみ (盗聴) |
| PC viewer プロセス | フル GUI | ビルド + 起動 | ログ解析 | 直接 TCP 接続試行 |
| ネットワーク (LAN) | (透過) | 設定 | 診断 | 盗聴 / 不正接続 |
| 録画ファイル | 閲覧 | (なし) | 解析 | 経路があれば閲覧可 |

---

## 関連文書

- ユースケース: [`primary_use_cases.md`](primary_use_cases.md), [`exception_scenarios.md`](exception_scenarios.md)
- 図示: [`use_case_overview.puml`](use_case_overview.puml)
- 脅威詳細: [`../quality/risk_analysis/SECURITY_GAP_ANALYSIS.md`](../quality/risk_analysis/SECURITY_GAP_ANALYSIS.md)
- 用語集: [`../quality/GLOSSARY.md`](../quality/GLOSSARY.md)
