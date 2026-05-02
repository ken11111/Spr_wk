# Exception Scenarios (異常系シナリオ)

**バージョン**: 1.0
**作成日**: 2026-05-02
**目的**: [`primary_use_cases.md`](primary_use_cases.md) の各 UC 例外を**横断的**に整理し、構造的天井 #1〜#5 や運用上の典型的失敗パスを 1 か所で俯瞰できるようにする
**形式**: 各シナリオを **トリガー → 検知 → 対応 → 復旧** の 4 段階で記述

---

## ES-1: GS2200M バッファ枯渇 (構造的天井 #1)

**支配的影響**: UC-2 ストリーミング劣化 → UC-5 切断復旧へ遷移

| 段階 | 内容 |
|---|---|
| **トリガー** | 連続送信負荷で GS2200M `tx_buff[1] × 1500B` が完全直列化される。MJPEG batch 122 KB が 84 回 SPI 転送に分割され、tx_buff の confidence が低下 |
| **検知** | TCP Health Monitor (`g_tcp_health`) が moving_avg×3 を超過、または >1000 ms が連続 2 回 → spike 累積 |
| **対応 (現実装)** | usb_thread が `tcp_server_send_with_reconnect` で再接続を試みる (max=5, exp backoff) |
| **対応の評価 (ADR-002 v1.1)** | 🔴 **逆効果と判明**: PC FPS 6.74 → 2.77 (-59%), ドロップ率 53.7% → 74.2%。GS2200M 内部リソース枯渇が原因のため再接続では解消不可 |
| **復旧 (現状)** | 1-3 回目で復旧する場合あり。4 回目以降 RST 拒否、5 回失敗で FAILED 固着 → ES-7 (人手介入) |
| **真の解決** | Tier 移行 (ESP32-S3 / RPi CM5 / USB-only) のみ |

**関連**: UC-2, UC-5, ADR-002 v1.1, QAS-1, QAS-8, [`../quality/GLOSSARY.md`](../quality/GLOSSARY.md) §2 (構造的天井 #1)

---

## ES-2: WiFi 切断 (Spresense 起点)

**支配的影響**: UC-2 ストリーミング完全停止

| 段階 | 内容 |
|---|---|
| **トリガー** | WiFi AP の電源 off / 距離増加で電波弱化 / 認証情報変更 |
| **検知** | GS2200M driver が disassociation event を notif_q に投入、wifi_manager が監視 |
| **対応 (現実装)** | 自動再 association を試みる (`wifi_manager.c`)、失敗時はシリアルログにエラー出力 |
| **復旧** | AP 復旧後に自動再 associate → DHCP → TCP listen 再開 |
| **代替経路** | 同経路上の Tier C (USB-only) フォールバックは現状自動切替なし — 運用者が物理 USB 接続に切替 |

**関連**: UC-1, UC-2, Tier C デプロイ図 (`spresense_deployment_candidate_c_usb.puml`)

---

## ES-3: USB 切断 (USB 路使用時)

**支配的影響**: USB 経由のストリーミング停止 (TCP 路は影響なし)

| 段階 | 内容 |
|---|---|
| **トリガー** | USB ケーブル抜去 / PC viewer プロセス終了 / `/dev/ttyACM0` クローズ |
| **検知** | usb_thread の `write()` が EPIPE / EIO を返す (`usb_transport.c:186`) |
| **対応 (現実装)** | `CONFIG_MAX_RECONNECT_RETRY=3` 回まで write リトライ → 失敗で usb_transport_init を再試行 (再 open) |
| **復旧** | USB 物理再接続 → `/dev/ttyACM0` 再 open → ストリーミング再開 |
| **注意** | TCP 路は影響なし、つまり **USB は補助路** であり主路 TCP の切断は ES-1/ES-5 で扱う |

**関連**: UC-1, UC-2, [`../functional/STREAMING_SPEC.md`](../functional/STREAMING_SPEC.md)

---

## ES-4: PC viewer プロセスクラッシュ

**支配的影響**: UC-2 表示停止 → Spresense 側は再接続 wait 状態に遷移

| 段階 | 内容 |
|---|---|
| **トリガー** | PC viewer (`security_camera_viewer`) のクラッシュ (panic / OS による kill / OOM) |
| **検知 (Spresense 側)** | TCP send で broken pipe 検知 → handle_disconnect 発火 |
| **対応 (Spresense 側)** | UC-5 (TCP 切断時の自動復旧) パスに進入。reconnect_count++、`tcp_server_wait_reconnect` で client 再接続を待機 |
| **復旧** | 運用者が PC viewer を手動再起動 (`cargo run` / 実行ファイル) → 自動再接続 (RECONNECT_MAX_ATTEMPTS=10) |
| **クラッシュ原因の典型** | Rust panic (bounded(3) 超過 — ADR-008 で考慮済 / unwrap on None) / OS の OOM killer / 録画ファイル書き込み失敗 |

**関連**: UC-2, UC-5, ADR-008 (bounded(3) で防御), [`Rust_ws/security_camera_viewer/src/pipeline.rs`](file:///home/ken/Rust_ws/security_camera_viewer/src/pipeline.rs)

---

## ES-5: 設定不整合 (WiFi SSID/Password 誤り or 認証情報変更)

**支配的影響**: UC-1 起動失敗、運用開始不可

| 段階 | 内容 |
|---|---|
| **トリガー** | `wifi_config.h` の WIFI_SSID/PASSWORD と AP 側設定の不一致、または AP 側で認証情報変更 |
| **検知** | `wifi_manager_connect` が失敗 → シリアルログに「Failed to associate with WPA」(`wifi_manager.c:166`) |
| **対応 (現実装)** | 起動失敗 → ladder cleanup で全リソース解放 (`camera_app_main.c:218-281`) → main task exit |
| **復旧** | 設置者が `wifi_config.h` を正しい SSID/Password に修正 → 再ビルド → 再書き込み (UC-6 主シナリオ A) |
| **派生問題** | WiFi 認証情報が git track されている問題 (X-7) — 設置者が誤って認証情報をコミットし公開リスク |

**関連**: UC-1, UC-6, X-7 (PENDING_NFR_WORK), [`../quality/CROSS_CUTTING_CONCERNS.md`](../quality/CROSS_CUTTING_CONCERNS.md) §3

---

## 構造的天井との関係マトリクス

| 構造的天井 | 顕在化する例外シナリオ | 解消手段 |
|---|---|---|
| #1 GS2200M `tx_buff[1] × 1500B` | ES-1 (バッファ枯渇) | Tier 2/3/C 移行 |
| #2 NuttX IOB プール 1568B | ES-1 と複合 | Tier 2/3 移行 (Tier C は IOB 制約も回避できる可能性) |
| #3 usrsock PREALLOC_CONNS=6 | (本プロジェクトは 1 接続なので顕在化せず) | (現状不要) |
| #4 RAM 1.5 MB | UC-6 設定変更時の TLS 実装不可 (Q24 関連) | Tier 2/3 移行 |
| #5 GS2200M 内部 (改変不能) | ES-1 と一体 | Tier 2/3/C 移行 |

---

## 異常系の検知手段サマリ

| 検知手段 | 検知対象 |
|---|---|
| TCP Health Monitor (`g_tcp_health`) | ES-1 (バッファ枯渇による spike) |
| GS2200M driver `notif_q` | ES-2 (disassociation event) |
| USB write errno (EPIPE/EIO) | ES-3 (USB 切断) |
| TCP send broken pipe | ES-4 (PC viewer クラッシュ), ES-1 (天井超過後の RST) |
| `wifi_manager_connect` 戻り値 | ES-5 (設定不整合) |

**観察**: 検知手段は揃っているが、**検知から対応 (人手介入含む) への運用フロー**が未文書化。これは X-4 運用ランブック作成タスクで埋める。

---

## 関連文書

- 主要 UC: [`primary_use_cases.md`](primary_use_cases.md) (UC-1〜7)
- アクター: [`actors.md`](actors.md)
- 構造的天井: [`../quality/GLOSSARY.md`](../quality/GLOSSARY.md) §2
- 制約根拠: [`../architecture/SPRESENSE_TCP_CONSTRAINTS.md`](../architecture/SPRESENSE_TCP_CONSTRAINTS.md)
- 健全性状態: [`../architecture/gs2200m_health_state_machine.puml`](../architecture/gs2200m_health_state_machine.puml)
- 残タスク: [`../quality/PENDING_NFR_WORK.md`](../quality/PENDING_NFR_WORK.md) (X-4 運用ランブック, X-7 WiFi 認証情報分離)
