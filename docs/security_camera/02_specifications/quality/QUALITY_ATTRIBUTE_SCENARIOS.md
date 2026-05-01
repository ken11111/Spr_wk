# Quality Attribute Scenarios (QAS) — arc42 §10

**バージョン**: 1.0
**作成日**: 2026-05-01
**形式**: Source / Stimulus / Environment / Artifact / Response / Response Measure (arc42 標準 6 要素)
**目的**: 品質要求を**測定可能なシナリオ**として記述し、目標 vs 実測の達成状況を一義化する

**前提**: 概要は [`QUALITY_REQUIREMENTS.md`](QUALITY_REQUIREMENTS.md), 用語は [`GLOSSARY.md`](GLOSSARY.md) 参照。

---

## QAS-1: GS2200M バッファ枯渇時の予防的再接続 (Reliability)

| 要素 | 内容 |
|---|---|
| **Source** | GS2200M WiFi モジュール内部バッファの構造的天井 #1 (`tx_buff[1] × 1500B`) |
| **Stimulus** | 連続送信負荷で TCP send 平均時間が moving_avg×3 を超過、または >1000 ms が連続 2 回 |
| **Environment** | 通常運用中 (QVGA 30 fps, MJPEG batch 122 KB), Spresense + GS2200M Tier 1 |
| **Artifact** | TCP Health Monitor (`g_tcp_health`, `tcp_health_update`) + Auto-Reconnect FSM |
| **Response** | 切断を待たず予防的に `tcp_server_send_with_reconnect` 経由で再接続を試行 |
| **Response Measure (目標)** | 3 秒以内に再接続完了, max=5 回まで継続 |
| **Response Measure (実測)** | ❌ ADR-002 v1.1: 4 回目以降 RST 拒否で失敗、PC FPS 6.74 → 2.77 (-59%), ドロップ 53.7% → 74.2% |
| **達成状況** | 🔴 **逆効果と判明**。構造的天井 #1 のため設計の前提が崩れている |
| **関連** | [`SPRESENSE_TCP_CONSTRAINTS.md`](../architecture/SPRESENSE_TCP_CONSTRAINTS.md) §3, ADR-002 v1.1 |

---

## QAS-2: QVGA 30 fps での MJPEG batch 送信 (Performance Efficiency)

| 要素 | 内容 |
|---|---|
| **Source** | PC viewer の連続ストリーム要求 |
| **Stimulus** | TCP send 呼び出し (MJPEG batch ~122 KB, 30 回/秒) |
| **Environment** | 構造的天井: SO_SNDBUF=256 KB, IOB プール=1568B, SPI 4 MHz, tx_buff[1] |
| **Artifact** | `usb_thread_func` → `tcp_server_send_with_reconnect` → GS2200M tx_buff pipeline |
| **Response** | 122 KB を 1464B/pkt × 約 84 回の SPI 転送に分割して送信 |
| **Response Measure (目標)** | end-to-end < 100 ms (Want), < 1 秒 (Must) |
| **Response Measure (実測)** | Phase 8: 134 ms 平均 / 2,713 ms 最大. Phase 9: 227 ms 平均 / 5,228 ms 最大 |
| **達成状況** | ✅ 1s Must / 🔴 100ms Want は構造的天井 #1 で物理的に達成不可 |
| **関連** | QAS-1 (天井超過時), QAS-10 (queue 動的調整) |

---

## QAS-3: Phase 10 PID 制御ループ (Performance Efficiency)

| 要素 | 内容 |
|---|---|
| **Source** | Adaptive FPS Controller (`control_thread_func`) |
| **Stimulus** | 100 ms 周期タイマー満了 → queue depth 観測 |
| **Environment** | 通常運用中, Phase 10 PID 制御アクティブ |
| **Artifact** | `fps_controller.c` (Kp=0.15, Ki=0.02, setpoint=3.5) + `camera_set_fps_runtime` |
| **Response** | PID 出力に基づいて `camera_set_fps_runtime(new_fps)` 呼出 (V4L2 ioctl) |
| **Response Measure (目標)** | 周期 100 ms ±10 ms, FPS 出力 [5, 30] にクランプ |
| **Response Measure (実測)** | ✅ 制御周期は安定、setpoint 3.5 への収束は実装済 |
| **達成状況** | ✅ Phase 10 統合済み (commit `2ab7bfc`) |
| **関連** | Phase 11 拡張 (適応 PID) は **未実装** (`enhanced_control.h` の API のみ) |

---

## QAS-4: TCP Health Monitor スパイク検知 (Reliability)

| 要素 | 内容 |
|---|---|
| **Source** | リアルタイム TCP send 時間計測 |
| **Stimulus** | `tcp_health_moving_avg_ms` フィールド更新 (3 秒粒度) |
| **Environment** | 移動平均ウィンドウ 8 サンプル (約 24 秒), metrics packet 58B 送信 |
| **Artifact** | `g_tcp_health` 構造体 + `tcp_health_update()` + metrics packet (sync_word 0xCAFEBEEF) |
| **Response** | 閾値判定 (avg×3 or >1000ms 連続 2 回) で spike フラグ点火 |
| **Response Measure (目標)** | 偽陽性 < 5%, 検出遅延 < 9 秒 (3×3 秒窓) |
| **Response Measure (実測)** | ✅ Phase 9.2 で計測実装、QAS-1 への入力源として機能 |
| **達成状況** | ✅ 検知機構は機能しているが、それを基にした再接続戦略 (QAS-1) が逆効果 |
| **関連** | Phase 9.2, [`gs2200m_health_state_machine.puml`](../architecture/gs2200m_health_state_machine.puml) |

---

## QAS-5: TLS handshake (⚠ 設計のみ・未実装) (Security)

| 要素 | 内容 |
|---|---|
| **Source** | PC viewer 初回接続要求 |
| **Stimulus** | TLS 1.3 handshake request (**設計上**) |
| **Environment** | TLS 1.3, AES-256-GCM, JWT トークン, デバイス証明書 (**設計のみ**) |
| **Artifact** | `security_coprocessor_t` (SECURITY_ARCHITECTURE.md で言及, **未実装**) |
| **Response** | デバイス証明書 + 公開鍵交換、相互認証 (mTLS) |
| **Response Measure (目標)** | handshake < 1 秒, 認証失敗率 0% |
| **Response Measure (実測)** | ❌ **実装が存在しない**。実装は TCP 直送・クリアテキスト・認証なし |
| **達成状況** | 🔴 **設計のみ・実装乖離**。詳細は [`SECURITY_GAP_ANALYSIS.md`](SECURITY_GAP_ANALYSIS.md) |
| **関連** | [`../architecture/SECURITY_ARCHITECTURE.md`](../architecture/SECURITY_ARCHITECTURE.md) (Phase 9.2 設計) |

---

## QAS-6: Full HD 30 fps 移行判定 (Portability)

| 要素 | 内容 |
|---|---|
| **Source** | Phase 1B/2A 要求 (Full HD 1920×1080 @ 30 fps) |
| **Stimulus** | 必要帯域 = 200 KB+/frame × 30 = 6 MB/s 以上 (SPI 4 MHz 理論 500 KB/s を 12 倍超過) |
| **Environment** | 現状 Tier 1 では構造的天井 #1 #4 で物理的に不可 |
| **Artifact** | ADR-006 GATE-1 ハードウェア評価 + Tier 候補図 4 枚 |
| **Response** | 以下のいずれかを選択: <br>(a) Tier 2 ESP32-S3 (+14 週) <br>(b) Tier 3 RPi CM5 (Spresense 資産放棄, +最大コスト) <br>(c) Tier C USB-only (+2-4 週, 12 Mbps 制約あり) |
| **Response Measure (目標)** | 各 Tier で Full HD 30 fps 達成可否を定量判定 |
| **Response Measure (実測)** | Tier 2 帯域: ESP32-S3 内蔵 WiFi で達成可能性高. Tier 3: 確実達成. Tier C: USB 12 Mbps では Full HD 30fps 不足 |
| **達成状況** | 🟡 **判定材料は揃っている** (現状未移行, Phase 12 以降の意思決定タスク) |
| **関連** | ADR-006, [`SPRESENSE_TCP_CONSTRAINTS.md`](../architecture/SPRESENSE_TCP_CONSTRAINTS.md) §13.5 |

---

## QAS-7: Dead code 削除 (Maintainability)

| 要素 | 内容 |
|---|---|
| **Source** | アーキテクチャ レビュー (本セッション) で識別 |
| **Stimulus** | コードベース整理、ビルド時間短縮、保守容易性向上の必要性 |
| **Environment** | 現行 MJPEG 単一路、`MJPEG_BATCHING_ENABLED=0` |
| **Artifact** | `apps/examples/security_camera/encoder_manager.c` (H.264 path), `protocol_handler.c` (NAL packing + handshake) |
| **Response** | `MJPEG_BATCHING_ENABLED` を再評価し、H.264 path をオプション化または削除 |
| **Response Measure (目標)** | dead code 行数 0, ビルド時間 -10% 程度 |
| **Response Measure (実測)** | 現状 Makefile (CSRCS:48-49) で compile されるが caller 0 件 |
| **達成状況** | 🟡 **識別完了** (本セッション, L2.A/L2.B 図に注記済). 削除実施は別タスク |
| **関連** | [`spresense_main_board_l2a_capture.puml`](../architecture/spresense_main_board_l2a_capture.puml), [`l2b_transport.puml`](../architecture/spresense_main_board_l2b_transport.puml) |

---

## QAS-8: 自動再接続 5 回失敗時の人手介入経路 (Reliability)

| 要素 | 内容 |
|---|---|
| **Source** | TCP 切断イベント (GS2200M 内部リソース枯渇 → RST) |
| **Stimulus** | 再接続試行 5 回連続失敗 → FAILED 状態固着 |
| **Environment** | Auto-Reconnect FSM (max=5, backoff 1s + N×2s), 健全性 6 状態モデル |
| **Artifact** | `tcp_server.c:502` (max retry 判定), Health State FSM |
| **Response** | FAILED 状態に遷移、ログ出力で人手介入を要求 |
| **Response Measure (目標)** | 成功率 (3 回以内) > 95%, 失敗時通知 < 5 秒 |
| **Response Measure (実測)** | ❌ Phase 9 実測: 4 回目で RST 拒否頻発, 連続失敗による品質劣化 |
| **達成状況** | 🔴 **失敗後の人手介入手順が文書化されていない**。運用者は何をすべきか不明 |
| **関連** | QAS-1, ADR-002 v1.1, 構造的天井 #1 |

**改善提案** (本タスク外): 運用ランブック作成 (Spresense 再起動手順, GS2200M リセット, Tier C 切替手順等)

---

## QAS-9: プロトコル versioning (Compatibility)

| 要素 | 内容 |
|---|---|
| **Source** | Spresense ファームウェア更新 (Phase A → B → C 移行) |
| **Stimulus** | metrics packet レイアウト変更 (Phase 4.1: +TCP stats / Phase 9.2: +health 4 fields) |
| **Environment** | PC viewer (Rust) は固定 layout 想定で実装 |
| **Artifact** | sync_word による種別識別 (`0xCAFEBABE/BABF/BEEF`), packet サイズ拡張 |
| **Response** | 拡張時は新しい sync_word を割り当て、または既存を後方互換で拡張 |
| **Response Measure (目標)** | 異版同時運用での互換性エラー 0%, fallback < 1 秒 |
| **Response Measure (実測)** | ✅ Phase 4.1 → 9.2 の拡張は後方互換で実施成功 |
| **達成状況** | 🟡 **後方互換は機能しているが、versioning 戦略の体系化なし** |
| **関連** | [`mjpeg_protocol.h:36, 41`](../../../apps/examples/security_camera/mjpeg_protocol.h) |

**改善提案**: バージョンネゴシエーション機構 (handshake 時の capability 交換) の体系化

---

## QAS-10: action_queue 動的深度調整 (Performance Efficiency)

| 要素 | 内容 |
|---|---|
| **Source** | リアルタイム ドロップ率モニタリング |
| **Stimulus** | `action_queue` 負荷が閾値超過 (depth 5 → 7 → 9 動的調整) |
| **Environment** | `CONFIG_QUEUE_DEPTH_MIN/DEFAULT/MAX = 5/7/9` |
| **Artifact** | `frame_queue.c` (queue resize logic) + `fps_controller.c` (PID 出力) |
| **Response** | キュー深度を動的調整して過負荷を回避 |
| **Response Measure (目標)** | drop rate > 20% 状態を回避, 調整完了 < 500 ms |
| **Response Measure (実測)** | 🟡 実装はあるが実測値未文書化 |
| **達成状況** | 🟡 機構は実装済、効果定量化が今後の課題 |
| **関連** | Phase 10, [`spresense_main_board_l2c_control.puml`](../architecture/spresense_main_board_l2c_control.puml) |

---

## サマリ — 達成マトリクス

| QAS | 品質属性 | 状態 | 主因 |
|---|---|---|---|
| QAS-1 | Reliability | 🔴 | 構造的天井 #1 で再接続戦略が逆効果 |
| QAS-2 | Performance | 🟡 | 1s Must ✅ / 100ms Want 🔴 |
| QAS-3 | Performance | ✅ | Phase 10 PID 統合済 |
| QAS-4 | Reliability | ✅ | 検知機構機能 (但し QAS-1 への接続は問題) |
| QAS-5 | Security | 🔴 | **設計のみ・実装無し** |
| QAS-6 | Portability | 🟡 | 判定材料整備済、Phase 12 で意思決定 |
| QAS-7 | Maintainability | 🟡 | 識別済、削除実施は別タスク |
| QAS-8 | Reliability | 🔴 | 失敗後の人手介入手順未定義 |
| QAS-9 | Compatibility | 🟡 | 後方互換機能、戦略体系化なし |
| QAS-10 | Performance | 🟡 | 機構実装済、効果定量化未 |

**全体所感**:
- **構造的天井 #1** が QAS-1, QAS-2, QAS-8 の達成可否を支配 → Phase 12 で Tier 移行判断が中核
- **設計-実装乖離** (QAS-5) が最大の透明性リスク → SECURITY_GAP_ANALYSIS で開示
- 機構実装は済んでいるが**効果定量化** (QAS-9, QAS-10) が次の改善余地

---

## 改訂履歴

| バージョン | 日付 | 変更内容 |
|---|---|---|
| 1.0 | 2026-05-01 | 初版。10 QAS を arc42 標準形式で記述、目標/実測/達成状況を一義化 |
