# STAMP/STPA Test Plan — UCA 再現テスト + SC 受け入れ基準

**バージョン**: 1.4 (Minto Pyramid 準拠でエグゼクティブサマリを冒頭に追加)
**作成日**: 2026-05-20 (最終更新: 2026-05-22)
**親文書**: [`STAMP_STPA_ANALYSIS.md`](STAMP_STPA_ANALYSIS.md) v1.7.1

---

## 📋 エグゼクティブサマリ

### 本書を読むと何が分かるか

親文書で抽出した **UCA 45 件** (安全 31 + Sec 14) を **再現可能なテストケース** に落とし込んだ計画書。Phase 12 で対策を実装する前に、現状の UCA を再現テストとして **可視化** し、対策実装後の効果を **回帰検証** できる基盤を提供する。

### 本書の最大の価値 3 つ

1. **Spresense 側テスト 0 件問題への対応案** — HIL シェル + ホスト側 Unit test 組み合わせ案 (§6.1)。NuttX 全機能エミュレータは ROI 低 (mocking 負債大) と判定
2. **STPA-Sec ペネトレ計画** — THREAT_MODEL.md §5 の攻撃手順を STPA-Sec UCA-Def に再構成。`nc` / `tshark` / `ettercap` / `hostapd` の具体コマンドで実施可能 (§4)
3. **カバレッジ KPI 4 種** — `UCA-Reproduce-%` / `SC-Verify-%` / `UCA-Regression-%` / `STPA-Sec-Pentest-%` を Phase 12/13 目標で定義 (§7)。v1.0 ベースライン 0% → Phase 12 で 44-70% を狙う

### Phase 12 着手で必要な対応

| Phase | 含まれるテスト | 工数目安 | 主な対策との連動 |
|---|---|---|---|
| **12.1 即着手** (1-2 週間) | TC-O.2 / TC-C1.2 / TC-VIEW.1 / TC-VIEW.2 / TC-A1.* | **6 人日** | M-5, M-3, M-20 / VIEW.1 は実装確認のみ |
| 12.2-12.3 (4-14 週間) | TC-A1.2, B1.3 / PT-AUTH.1 / PT-RL.* | 4-5 週間 | M-1, M-2, M-8, M-15 |
| 13+ | TC-MEM.1 / PT-FB.1 / TC-O.3 | 別計画 | M-23, M-16, M-9 |

### 試験環境必須セットアップ

| ツール | 用途 | UCA |
|---|---|---|
| `tc / netem` | 遅延・パケットロス注入 | UCA-A1.* / UCA-B1.3 |
| `hostapd` | Evil Twin AP 構築 | UCA-APV.1 |
| `tshark / Wireshark` | パケットキャプチャ | UCA-CRYPTO.1 / UCA-AUTH.1 |
| `ettercap` | MITM 改ざん | UCA-INT.1 |
| `iptables / stress-ng` | DoS 注入 | UCA-RL.* |
| HIL 治具 (リレー制御電源) | ブラウンアウト誘発 | UCA-PWR.1 |

### 詳細目次

- [§0 スコープ](#0-スコープと前提) — 対象 UCA 45 件 / テスト種別 / 既存テスト 31 件とのギャップ
- [§1 戦略](#1-テスト戦略) — UCA 4 タイプ別の変換ルール
- [§2 環境構築](#2-試験環境構築) — HW / セットアップ / metrics 取得経路
- [§3 安全 STPA UCA テスト](#3-安全-stpa-uca-テスト計画) — TC-A1.*, B1.*, C1.*, D1.*, FBM/STREAM/VIEW/CAM/MEM/MD/Scene/CAM-AE/OS/DRV/PWR/Operator
- [§4 STPA-Sec ペネトレ](#4-stpa-sec-uca-テスト計画-ペネトレーション) — PT-AUTH/CRYPTO/INT/AUDIT/RL/APV/FB/MEMSAFE/PHYS
- [§5 SC 受け入れマトリクス](#5-safety-constraints-受け入れ基準マトリクス) — SC-1〜SC-15
- [§6 既存基盤ギャップ](#6-既存テスト基盤とのギャップと施策) — Spresense テスト 0 件への対応
- [§7 カバレッジ KPI](#7-カバレッジ-kpi)
- [§8 段階導入](#8-段階導入計画)

---

**位置付け**: [`TEST_COVERAGE_BASELINE.md`](../performance/TEST_COVERAGE_BASELINE.md) v1.1 の拡張。X-8 タスクと連携。

> **本書を読む前に**: STAMP_STPA_ANALYSIS.md レビュー (アーキ/PdM/開発者/テスト 4 視点) で **「テスト計画への落とし込みゼロ」が最重大ギャップ** と判定された。本書はそのギャップを埋める。

---

## §0 スコープと前提

### 0.1 対象 UCA
- 安全 STPA: 31 件 (CTRL-A〜D, Operator, FBM, STREAM, VIEW, CAM, MEM)
- STPA-Sec: 14 件 (UCA-AUTH/CRYPTO/INT/AUDIT/RL/APV/FB/MEMSAFE/PHYS)
- 合計 **45 件** (※親文書 §9 で 44 件と記述、本書では UCA-MEMSAFE.1 を「達成済テスト要」として 1 件加算)

### 0.2 テスト種別の整理

| 種別 | 用途 | 自動化 | 既存基盤 |
|---|---|---|---|
| **Unit** | UCA-VIEW.* (Rust 側) / mjpeg_protocol / fps_controller | ✅ `cargo test`, `pytest` | Rust 31 件 / Spresense **0 件** |
| **Integration** | 制御ループ再現 (UCA-A1.*, UCA-FBM.*) | ✅ ホスト側エミュレータ + Spresense | 一部 (mp4_recorder の `#[ignore]`) |
| **HW-in-loop (HIL)** | 実機 Spresense + WiFi + PC で UCA を物理再現 | 🟡 手動 | 未整備 |
| **Penetration (Pentest)** | STPA-Sec UCA-Def を CA-ADV で再現 | 🟡 手動 (将来 CI) | 未整備 |
| **Soak / Endurance** | UCA-C1.2 (ストレージ枯渇) / UCA-B1.4 (FAILED 固着) | 🟡 長時間ジョブ | 未整備 |
| **Acceptance** | SC-1〜SC-10 の合否判定 | 🟡 metrics 自動収集 | metrics packet 58 B のみ |

### 0.3 既存テストカバレッジとのギャップ ([`TEST_COVERAGE_BASELINE.md`](../performance/TEST_COVERAGE_BASELINE.md) §1)

- PC viewer (Rust): 31 件 `#[test]` (うち `#[ignore]` 3 件)
- **Spresense (C): 0 件** ← 本書 UCA 検証の最大の障壁
- gui_main.rs (1815 行) はテスト 0

→ 本書 §6 で **Spresense 側テスト基盤導入を必須前提** として位置付ける。

---

## §1 テスト戦略

### 1.1 UCA → テストケース変換ルール

UCA の 4 タイプ別にテスト戦略を定型化:

| UCA タイプ | テスト戦略 | 例 |
|---|---|---|
| **NP** (Not Provided) | 「該当条件を発生させ、CA が出ないことを観測」 + 「あるべき後続動作の欠如」を確認 | UCA-D1.1: WiFi 切断 → metrics 送信されない → USB 副経路にも出ない |
| **P** (Provided incorrectly) | 「正常系では出ない誤った CA」を意図的状況で誘発し、結果が hazard 状態に遷移するか観測 | UCA-A1.2: tx_buff 飽和を再現 → PID が高 FPS を維持するか |
| **TL** (Too Early/Late) | 「タイミング閾値の境界条件」を試験 | UCA-B1.3: 100ms / 200ms / 500ms の遅延注入で再接続発火タイミング測定 |
| **SL** (Stopped Soon / Applied Long) | 「持続条件」のロングラン試験 | UCA-B1.4: 強制切断 6 回連続 → FAILED に到達後の挙動を 24h 観測 |

### 1.2 優先度 (親文書 §5.3 ランキングと §8.6 DREAD)

P0 (即時着手): 親文書ランキング Top 10 + DREAD ≥ 35 の UCA-Def
P1 (Phase 12): ランキング 11〜20 + DREAD 25-34
P2 (Phase 13+): 残り

---

## §2 試験環境構築

### 2.1 ハードウェア構成

```
[Spresense + Camera + WiFi] ── WiFi ── [AP] ── WiFi ── [PC viewer]
                            └── USB ──────────────────────┘

  [Test Host (Linux)]
    - tc / netem: 遅延・パケットロス注入
    - hostapd: Evil Twin AP 構築
    - tshark / Wireshark: パケットキャプチャ
    - iptables: rate limit / packet drop
    - stress-ng: PC 側負荷生成
```

### 2.2 必須セットアップ

```bash
# テスト依存 (Linux ホスト)
sudo apt install iproute2 wireshark tshark hostapd iptables stress-ng \
                 ffmpeg netcat-openbsd python3-scapy

# Rust 側
cargo install cargo-llvm-cov
rustup component add llvm-tools-preview

# Spresense 側 — 現状テスト基盤無 (→ §6.1 で構築計画)
```

### 2.3 metrics 取得経路 (受け入れ基準の根拠)

| metrics 源 | 取得手段 | 用途 |
|---|---|---|
| 58 B metrics packet (sync_word `0xCAFEBEEF`) | PC viewer ログ or 別経路 (USB CDC-ACM) | 全 SC の主要観測点 |
| syslog (Spresense) | USB シリアル + `tee` | UCA-MEM.1, UCA-CAM.1, ladder cleanup 確認 |
| `cargo-llvm-cov` HTML | Rust 側カバレッジ | UCA-VIEW.* の合格根拠 |
| tshark pcap | LAN タップ | UCA-AUTH.1, CRYPTO.1, INT.1, RL.* (STPA-Sec) |
| Spresense control_thread ログ | `LOG_INFO` 経由 (UART) | UCA-A1.*, UCA-FBM.*, PID 観測 |
| `tc qdisc show` / `iperf3` | ネットワーク状態 | UCA-B1.3, UCA-FB.1 |

---

## §3 安全 STPA UCA テスト計画

### 3.1 CTRL-A (PID) 系

#### TC-A1.1: UCA-A1.1 (キュー滞留時 FPS 低減指令を出さない)

| 項目 | 内容 |
|---|---|
| **再現手順** | (1) Spresense 起動 (2) PC 受信を 1 fps に絞る (tc で受信側を `rate 100kbit` に制限) (3) Spresense action_queue を 60 秒間観測 |
| **誘発条件** | PC 受信遅延 → action_queue 滞留 |
| **観測 metrics** | metrics packet の `queue_depth` (≥ 5 で滞留), `fps_runtime` の推移 |
| **期待動作 (SC 達成)** | `queue_depth > setpoint (3.5)` 検出後、10 Hz 周期で `fps_runtime` が単調減少 (Kp=0.15, Ki=0.02 計算上 5 周期以内に 5 FPS 以下に低下) |
| **不合格判定** | 60 秒経過後も `fps_runtime ≥ 8` を維持 |
| **対応 SC** | SC-2 (天井を超える品質目標を出さない) |
| **対応対策** | M-1 (多変数化) ベリファイ |
| **自動化** | tc + シリアルログ採取スクリプト (新規, `tests/hil/uca_a1_1.sh`) |

#### TC-A1.2: UCA-A1.2 (tx_buff 飽和中の高 FPS 維持)

| 項目 | 内容 |
|---|---|
| **再現手順** | (1) PC 受信を停止 (tcp_connection.rs で受信ループを停止) (2) Spresense 側の送信完了時刻 (`tcp_send_complete_us`) のジッタ増加を観測 (3) 60 秒継続 |
| **誘発条件** | GS2200M `tx_buff[1]` 占有時間が伸び続ける |
| **観測 metrics** | `tcp_send_time_avg_ms` (期待: 134 → 200+ ms), `fps_runtime`, `frame_drop_count` |
| **期待動作** | (現状) FPS 低減せず → ❌ 失敗 (UCA-A1.2 を再現) ⇒ M-1 実装後は FPS 低減を観測 |
| **対応 SC** | SC-2, SC-7 |
| **対応対策** | M-1 |
| **注意** | 現実装ではこのテストは **「失敗することが正常」** (UCA-A1.2 の存在を可視化)。M-1 実装後にパスする |
| **自動化** | HIL シナリオ `tests/hil/uca_a1_2_txbuf_starvation.sh` |

#### TC-A1.3: UCA-A1.3 (FPS 低減が遅れる)

| 項目 | 内容 |
|---|---|
| **再現手順** | TC-A1.1 の冒頭 5 秒間で `queue_depth` が初めて 5 を超えた時刻 T1、`fps_runtime` が低減方向に動いた時刻 T2 を測定 |
| **合格基準** | T2 - T1 ≤ 100 ms (10 Hz 1 周期分) |
| **対応 SC** | SC-1 |

### 3.2 CTRL-B (Transport) 系

#### TC-B1.2/B1.3: UCA-B1.2/B1.3 (健全状態 or 一時遅延での再接続誤発火)

| 項目 | 内容 |
|---|---|
| **再現手順** | (1) AP 経由で `tc qdisc add dev wlanX root netem delay 200ms 80ms distribution normal` を注入 (2) 平均 134 ms の正常変動内 +50ms に収まる遅延を再現 (3) Spresense の `connection_state_t` 推移をログから抽出 (4) 60 秒継続 |
| **誘発条件** | 134 ms ± 50 ms の正常変動帯を超える一時的遅延 |
| **観測 metrics** | `unexpected_disconnects` counter, `connection_state_t` 推移, PC 側 FPS |
| **期待動作 (現状)** | 不要な reconnect 発火 → PC FPS 6.74 → 2.77 (-59%) を再現 ⇒ ADR-002 v1.1 と一致 |
| **期待動作 (M-2 後)** | RTT 移動平均ベース判定で reconnect 発火回避 |
| **対応 SC** | SC-3 (単位時間あたり 5 回まで) |
| **対応対策** | M-2 |
| **自動化** | `tests/hil/uca_b1_3_netem_jitter.sh` |

#### TC-B1.4: UCA-B1.4 (FAILED 固着)

| 項目 | 内容 |
|---|---|
| **再現手順** | (1) AP を 60 秒停止 → 5 回 reconnect 失敗誘発 (2) AP 再起動後の Spresense 自動回復を確認 (3) 24h ロングラン |
| **合格基準** | FAILED 到達後も周期的に再接続試行 (例: 60s 毎) を行う OR 運用通知 (LED + USB metrics) を発する |
| **対応 SC** | SC-3 |
| **対応対策** | M-2 + M-6 (ランブック) |
| **自動化** | soak テスト `tests/soak/uca_b1_4_failed_recovery.sh` (≥ 24h) |

### 3.3 CTRL-C (Recording) 系

#### TC-C1.2: UCA-C1.2 (ローテーション無で書込み継続)

| 項目 | 内容 |
|---|---|
| **再現手順** | (1) PC ストレージを `dd if=/dev/zero of=filler bs=1M count=$((残量-100MB))` で擬似的に枯渇近く設定 (2) 録画開始 (3) 残量推移を `df` で観測 |
| **誘発条件** | 残量 < 100 MB |
| **観測 metrics** | `df -h` 出力、recording 継続/停止状態、PC viewer ログ |
| **期待動作 (現状)** | ローテーション無 → 書込み停止 or 全停止 → UCA-C1.2 再現 |
| **期待動作 (M-3 後)** | 1 GB 上限到達時に最古ファイル削除、書込み継続 |
| **対応 SC** | SC-6 |
| **対応対策** | M-3 |
| **自動化** | `tests/integration/uca_c1_2_rotation.sh` (新規) |

### 3.4 CTRL-D (Health Monitor) 系

#### TC-D1.1: UCA-D1.1 (WiFi 切断中ヘルス送信不能)

| 項目 | 内容 |
|---|---|
| **再現手順** | (1) USB CDC-ACM 接続を確立した状態で WiFi 切断 (`hostapd` 停止 or RF Jammer 模擬) (2) 60 秒間 metrics の到達経路を観測 |
| **観測 metrics** | TCP/8888 経路と USB CDC-ACM 経路の双方を `tee` で監視 |
| **期待動作 (現状)** | metrics 到達手段なし → UCA-D1.1 再現 |
| **期待動作 (M-7 後)** | USB 経由で metrics 継続到達、LED ステータスも変化 |
| **対応 SC** | SC-10 |
| **対応対策** | M-7 |

### 3.5 v1.1 追加分: FBM / STREAM / VIEW / CAM / MEM

#### TC-FBM.1: UCA-FBM.1 (action_queue 満杯時の振る舞い)

| 項目 | 内容 |
|---|---|
| **再現手順** | (1) PC 受信側を擬似停止 (2) 高 FPS (30) + 大 JPEG (120 KB) を強制 (シーン複雑化で誘発: `cv2-stress-pattern` を被写体に表示) (3) action_queue 到達深度を観測 |
| **観測 metrics** | `frame_drop_count`, `queue_depth_peak`, drop reason (M-18 実装後) |
| **期待動作 (M-18 後)** | drop reason に `QUEUE_FULL_OLDEST_DROPPED` を含む metrics 出力 |
| **対応対策** | M-18 |

#### TC-STREAM.1: UCA-STREAM.1 (JPEG サイズ overflow)

| 項目 | 内容 |
|---|---|
| **再現手順** | (1) シーン複雑度を意図的に最大化 (高密度パターン or ノイズ画像) (2) JPEG サイズ ≥ 60 KB の発生頻度を観測 |
| **観測 metrics** | metrics packet に `OVERSIZE_FRAME` フラグ (M-19) を確認 |
| **期待動作 (M-19 後)** | OVERSIZE 検知で metrics 出力 |
| **対応対策** | M-19 |

#### TC-VIEW.1: UCA-VIEW.1 (CRC 検証失敗時の破棄)

| 項目 | 内容 |
|---|---|
| **再現手順** | Rust 側 `#[test]`: `protocol.rs` の `verify_crc()` に corrupt パケットを与えて破棄が起きるか確認 |
| **既存テストの有無確認** | ✏️ **v1.7.1 確定**: `protocol.rs:107-115, 201-209` で CRC 不一致時に `io::Error::InvalidData` 返却 (破棄実装済)。MJPEG / metrics 両方。**残: 運用者通知 (metrics 計上) の追加 unit test のみ** |
| **対応対策** | (テストが無い場合) Unit test 追加 |
| **注意** | レビュー指摘の通り、**本書執筆時点で実装側の CRC 失敗時の挙動は未確認**。実装/テスト両方を grep する必要あり |

#### TC-VIEW.2: UCA-VIEW.2 (viewer クラッシュ復旧)

| 項目 | 内容 |
|---|---|
| **再現手順** | (1) viewer プロセスを `kill -9` (2) systemd ユニット (`Restart=on-failure`) があれば 5 秒以内に再起動 (3) Spresense 側 accept wait からの回復確認 |
| **観測 metrics** | プロセス PID 推移、再接続成功までの時間 |
| **期待動作 (M-20 後)** | 5 秒以内に viewer 自動再起動 + 接続復帰 |
| **対応 SC** | SC-1 |
| **対応対策** | M-20 |

#### TC-MEM.1: UCA-MEM.1 (IOB プール枯渇の無音化)

| 項目 | 内容 |
|---|---|
| **再現手順** | (1) Spresense で TCP + UDP 並行送信を意図的に発生 (テスト用エコークライアントを 8 個並列接続) (2) IOB 使用率を NuttX デバッグログから抽出 (`iob_stats` コマンドが提供されていれば) |
| **観測 metrics** | IOB プール使用率 (M-23 で metrics 58B 拡張に追加予定), send 失敗回数 |
| **期待動作 (M-23 後)** | IOB ≥ 80% で警告 metrics 発出 |
| **対応対策** | M-23 |

### 3.6 v1.5 追加: Motion Detector / Scene / ISX012 AE 系

#### TC-MD.1: UCA-MD.1 (motion 誤検知 / FP)

| 項目 | 内容 |
|---|---|
| **再現手順** | (1) `tests/motion_corpus/false_positive/` に予め用意した誤検知誘発映像 (木の葉揺れ / 影移動 / ヘッドライト) を 30 件用意 (2) 各映像を viewer に流し motion_detected の出現を集計 |
| **観測 metrics** | `MotionDetectorStats` (検知回数 / 連続検知時間) + RecordingState 遷移 |
| **合格基準** | FP ≤ 5% (30 件中 1-2 件まで許容) |
| **対応対策** | M-25 |
| **自動化** | `cargo test --test motion_fp -- --include-ignored` (実映像コーパスは LFS) |

#### TC-MD.2: UCA-MD.2 (motion 見逃し / FN)

| 項目 | 内容 |
|---|---|
| **再現手順** | (1) `tests/motion_corpus/should_detect/` に明確な侵入動作映像 (人歩行 / 車両通過 / 動物侵入 — 各 照度別) 50 件 (2) 各映像で motion_detected 検知率を測定 |
| **観測 metrics** | 検知率 (true positive rate) |
| **合格基準** | FN ≤ 10% (50 件中 5 件まで許容)、特に低照度シーンで FN ≤ 20% |
| **対応対策** | M-25 |
| **自動化** | 同上 |

#### TC-MD.3: UCA-MD.3 (検知遅れ → post 録画切れ)

| 項目 | 内容 |
|---|---|
| **再現手順** | (1) 動きが急に止まる映像で countdown_frames の動作確認 (2) post_record_seconds × 11 = 330 frames の正確性検証 |
| **合格基準** | 動き終了後 30 秒間録画継続 ± 0.5 秒以内 |
| **対応対策** | (タイミング正確性のみ、追加対策不要) |

#### TC-MD.4: UCA-MD.4 (sensitivity 運用ガイダンス)

| 項目 | 内容 |
|---|---|
| **再現手順** | 被験者に「FP が多すぎる」「FN が多すぎる」状況を提示し、ランブック (M-27) 参照で適切な sensitivity に調整できるか確認 |
| **合格基準** | ランブック読み込み 10 分以内に正しい設定値選択 |
| **対応対策** | M-27 |

#### TC-Scene.1: UCA-A1.5 + UCA-CAM-AE.1 (Scene 急変 → JPEG 膨張 → tx_buff 飽和連鎖)

| 項目 | 内容 |
|---|---|
| **再現手順** | (1) 被写空間で照明 OFF → ON を試行 (or テスト用に LED 模擬装置で 100ms 単位の照度変化を 5 段階で発生) (2) Spresense 側で JPEG サイズ推移 / queue_depth / tcp_send_time / fps_runtime を 60 秒間ログ収集 |
| **観測 metrics** | metrics packet 拡張で取得した瞬間 JPEG サイズ + queue_depth ピーク |
| **期待動作 (現状)** | JPEG が 120 KB → 180 KB 等にスパイク → 2-3 フレーム後に queue ピーク → broken pipe 発生確率 ≥ 30% (現状を可視化、UCA-A1.5 / S-10 シナリオ再現) |
| **期待動作 (M-24 後)** | Scene complexity feedforward により queue ピーク前に fps_runtime 自動低減、broken pipe 発生率 ≤ 5% |
| **対応対策** | M-24 |
| **自動化** | HIL シナリオ + テスト用照度可変 LED 治具 |

#### TC-CAM-AE.1: UCA-CAM-AE.2 (強光 saturate → motion 見逃し)

| 項目 | 内容 |
|---|---|
| **再現手順** | (1) カメラに対し 30 ルクス → 30000 ルクスの強光を当てる (フラッシュライト) (2) ISX012 出力 JPEG の平均輝度を測定 + motion_detector の検知失敗を確認 |
| **観測 metrics** | JPEG ヒストグラム (peak at 255 = saturate), motion_detected フラグ |
| **期待動作 (現状)** | 強光中 motion_detector は検知不能 → CA-ADV.14 で攻撃成立 |
| **期待動作 (M-26 後)** | 起動時 / 定期 セルフテストで AE Safe Range 警告。強光検知時に metrics で `OVERILLUMINATED` フラグ |
| **対応対策** | M-26 |
| **注意** | Tier 1 (ISX012) では完全解消困難。Tier 移行候補のテスト材料 |

#### TC-ADV-SCENE.1: CA-ADV.15 (Scene DoS / 高密度パターン)

| 項目 | 内容 |
|---|---|
| **再現手順** | (1) カメラ前で高密度ランダムパターン (砂嵐ノイズの画面表示) を再生 (2) JPEG サイズ / tx_buff 占有 / broken pipe 発生を 5 分継続 |
| **合格基準 (M-24 後)** | complexity feedforward でストリーム継続 |
| **対応対策** | M-24 |
| **想定攻撃シナリオ** | 攻撃者がカメラ前にスマホ等で意図的にノイズ映像を流し DoS |

#### TC-ADV-SCENE.2: CA-ADV.16 (偽動き再生)

| 項目 | 内容 |
|---|---|
| **再現手順** | (1) カメラ前で侵入者風映像を再生 (2) MotionRecording が起動・録画されるか確認 (3) 録画ファイルを検査し本物の侵入と区別できるか確認 |
| **観測** | 録画ファイル / 動き発生時刻 |
| **期待動作 (現状)** | 偽動きでも録画起動 → 証拠の信頼性が脅かされる |
| **対応対策** | (将来) 機械学習ベース判別 / 環境センサー連動 / Phase 13+ で評価 |

### 3.6b v1.6 追加: OS / Driver / 電源 系

#### TC-OS.1: UCA-OS.2 (watchdog 不在 → kernel panic 後の無音停止)

| 項目 | 内容 |
|---|---|
| **再現手順** | (1) NuttX で `BUG()` マクロ等で意図的に kernel panic を発生させる (デバッグビルド限定) (2) 自動再起動するか確認 (3) metrics packet 再開時刻を測定 |
| **観測 metrics** | metrics packet の停止時刻 → 再開時刻、`uptime_seconds` の reset 確認 |
| **期待動作 (現状)** | 自動再起動しない → 完全停止 → UCA-OS.2 再現 |
| **期待動作 (M-28 後)** | watchdog タイマーで 5 秒以内に再起動、再開後 metrics packet 出力 |
| **対応 SC** | SC-15, SC-1 |
| **対応対策** | M-28 |
| **自動化** | HIL シナリオ `tests/hil/uca_os_2_watchdog.sh` |

#### TC-OS.2: UCA-OS.1 (pthread priority inversion)

| 項目 | 内容 |
|---|---|
| **再現手順** | (1) 低優先度 thread が mutex 取得 (2) 高優先度 control_thread が同じ mutex で待機 (3) 中優先度 thread が CPU を独占 → priority inversion 発生 |
| **観測** | control_thread の周期遅延 / metrics packet 遅延 |
| **合格基準 (M-28 後)** | priority inversion 検出ログ + 警告 metrics |
| **対応対策** | M-28 (watchdog で間接検出) |

#### TC-DRV.1: UCA-DRV-GS2200M.1 (HAL_TIMEOUT=5s 固定)

| 項目 | 内容 |
|---|---|
| **再現手順** | (1) WiFi AP を `hostapd` 停止で物理切断 (2) Spresense 側で SPI 通信のブロック時間を計測 (3) `connection_state_t` 遷移を観測 |
| **観測 metrics** | `tcp_send_time_avg_ms` の最大値、connection state 遷移時刻 |
| **期待動作 (現状)** | 最大 5 秒のブロック (HAL_TIMEOUT) → 切断検出が遅延 |
| **期待動作 (M-29 後)** | RTT 移動平均ベースで 1 秒以内に切断検出 |
| **対応 SC** | SC-3 |
| **対応対策** | M-29 (M-2 と統合) |
| **自動化** | `tests/hil/uca_drv_gs2200m_timeout.sh` |

#### TC-PWR.1: UCA-PWR.1 (ブラウンアウト → MP4 ファイル破損)

| 項目 | 内容 |
|---|---|
| **再現手順** | (1) 録画中の Spresense の電源を **書込み中タイミング** で意図的に切る (USB 抜去 or 電源スイッチ OFF) (2) 録画ファイル (.mp4) を PC で `ffprobe` で検査 |
| **観測** | MP4 ファイルが再生可能 (moov atom 存在) か |
| **期待動作 (現状)** | moov atom 欠落 → 再生不能 → UCA-PWR.1 再現 |
| **期待動作 (M-30 後)** | ブラウンアウト検出 → 100 ms 以内に Safe shutdown → ファイル再生可能 |
| **合格基準** | 電源 OFF タイミング 10 回試行で 9 回以上 再生可能 |
| **対応 SC** | SC-13 |
| **対応対策** | M-30 |
| **自動化** | HIL 治具 (リレー制御電源) で `tests/hil/uca_pwr_1_brownout.sh` |

#### TC-PWR.2: UCA-PWR.2 (バッテリ残量通知無)

| 項目 | 内容 |
|---|---|
| **再現手順** | (1) バッテリ運用に切替え (2) 充電せず連続稼働 (3) バッテリ残量 metrics と切断時刻を観測 |
| **観測** | metrics packet 内の `battery_percent` (M-31 で追加) |
| **合格基準 (M-31 後)** | バッテリ < 20% で `LOW_BATTERY` フラグ通知、< 5% で Safe shutdown 警告 |
| **対応対策** | M-31 |
| **注意** | PMIC からの残量取得手段が無い場合は外部 ADC + 分圧抵抗が必要 |

#### TC-PWR.3: UCA-PWR.3 (電源復帰後 auto-restart 無)

| 項目 | 内容 |
|---|---|
| **再現手順** | (1) Spresense を録画中に電源 OFF (2) 5 秒後に電源 ON (3) 自動再起動 + state リストア確認 |
| **観測** | metrics packet 再開時刻、SD カードに前回 state が保存されているか |
| **合格基準 (M-32 後)** | 5 秒以内に metrics 再開、前回 recording state を継続 (録画再開) |
| **対応 SC** | SC-14, SC-1 |
| **対応対策** | M-32 |

#### TC-ADV-POWER.1: CA-ADV.17 (物理電源切断攻撃)

| 項目 | 内容 |
|---|---|
| **CA-ADV** | 物理電源ケーブル抜去 / ブレーカー OFF |
| **再現コマンド** | (HIL 治具) リレー制御で電源 OFF を任意タイミングで実施 |
| **合格基準** | TC-PWR.1 + TC-PWR.3 を組合せたシナリオで Safe shutdown + auto-restart が機能 |
| **対応対策** | M-30 + M-32 + M-33 (運用文書化) |
| **注意** | 完全な防御 (UPS, 二重化) は本書スコープ外。M-33 ランブックで UPS 推奨を明示 |

### 3.7 Operator (運用) 系

#### TC-O.2: UCA-O.2 (cred を VCS に push)

| 項目 | 内容 |
|---|---|
| **再現手順** | (1) `wifi_config.h` に test SSID を書き込み (2) `git add wifi_config.h && git commit -m test` を実行 (3) pre-commit hook で阻止されるか確認 |
| **期待動作 (M-5 後)** | commit が失敗、メッセージで `wifi_config.h.example を使え` と表示 |
| **対応対策** | M-5 |
| **自動化** | `.git/hooks/pre-commit` (M-5 で導入) を CI でも実行 |

#### TC-O.1: UCA-O.1 (異常通知への対応不能)

| 項目 | 内容 |
|---|---|
| **再現手順** | 運用ランブック (M-6) を未読の被験者に「FAILED 状態」を提示し、対応手順を口頭聞き取り |
| **合格基準** | ランブック参照で 15 分以内に対応着手可能 |
| **対応 SC** | SC-1, SC-10 |
| **対応対策** | M-6 |

---

## §4 STPA-Sec UCA テスト計画 (ペネトレーション)

> THREAT_MODEL.md §5 で示された具体的攻撃手順を、STPA-Sec の UCA-Def 観点で **「防御 Controller が不在のため攻撃が成功するか」** のテストとして再構成。

### 4.1 認証・盗聴系 (高優先)

#### PT-AUTH.1: UCA-AUTH.1 → TS-1 (PC viewer なりすまし)

| 項目 | 内容 |
|---|---|
| **CA-ADV** | `nc spresense_ip 8888` で生の MJPEG ストリーム取得 |
| **再現コマンド** | `nc 192.168.1.x 8888 > capture.bin && file capture.bin` |
| **合格基準 (Option B 以降)** | 認証 trigger が無いと TCP 接続が即時切断される or データが届かない |
| **対応対策** | M-8 (TLS-PSK) |
| **自動化** | `tests/pentest/pt_auth_1.sh` |

#### PT-CRYPTO.1: UCA-CRYPTO.1 → TI-1 (MJPEG 盗聴)

| 項目 | 内容 |
|---|---|
| **CA-ADV** | `tshark -i wlan0 -f 'tcp port 8888' -w stream.pcap` + `mjpeg-extractor stream.pcap` |
| **合格基準 (Option D 以降)** | pcap から JPEG が抽出不能 (TLS で暗号化済) |
| **対応対策** | M-8 拡張 (TLS 全面) |
| **自動化** | `tests/pentest/pt_crypto_1.sh` |

#### PT-INT.1: UCA-INT.1 → TT-1 (MJPEG 中間者改ざん)

| 項目 | 内容 |
|---|---|
| **CA-ADV** | `ettercap -T -M arp:remote` で MITM 構築 → MJPEG ペイロード書換 → CRC 再計算 |
| **合格基準 (M-11 後)** | HMAC 検証で破棄、PC viewer の `crc_fail_count` 増 |
| **対応対策** | M-11 |

### 4.2 DoS 系

#### PT-RL.1/RL.2: UCA-RL.1/RL.2 → TD-1/TD-2

| 項目 | 内容 |
|---|---|
| **CA-ADV.5** | `for i in {1..100}; do nc -z 192.168.1.x 8888 & done` (大量 SYN) |
| **CA-ADV.6** | slow client: `python3 slow_read.py --rate 1B/s` (TCP slow read) |
| **観測 metrics** | Spresense 側 `tcp_send_time_avg_ms` の暴騰, accept rate, GS2200M tx_buff 占有 |
| **合格基準 (M-15 後)** | 同一 IP からの接続が rate limit で拒否、accept queue が枯渇しない |
| **対応対策** | M-15 |

### 4.3 改ざん / 否認 / 物理 (中〜低優先)

| TC-ID | 対象 UCA | CA-ADV | 検証手順 | 対策 |
|---|---|---|---|---|
| PT-INT.2 | UCA-INT.2 → TT-2 | MP4 ファイルを `ffmpeg -i in.mp4 -c copy out.mp4` で再エンコード | ハッシュチェーンで検知 | M-12 |
| PT-AUDIT.1 | UCA-AUDIT.1 → TR-1 | GUI 操作後 syslog grep で記録の有無 | 操作ログ + 署名検証 | M-13 |
| PT-AUDIT.2 | UCA-AUDIT.2 → TR-2 | Spresense 再起動で metrics 履歴消失を確認 | SD カード永続ログ | M-14 |
| PT-APV.1 | UCA-APV.1 → TS-2 | `hostapd` で同一 SSID 偽 AP を立ち上げ Spresense の接続先を観測 | AP MAC 固定検証 | M-10 |
| PT-FB.1 | UCA-FB.1 → TD-3 | `rfkill block wifi` で WiFi 強制停止 → USB 経路への自動切替確認 | Tier C 自動切替 | M-16 |
| PT-PHYS.1 | UCA-PHYS.1 → TE-2 | 物理 SPI bus へのテスト治具接続 (検査用) | 運用文書化 (筐体施錠) | M-17 |

### 4.4 既存防御の境界テスト (✅ MEMSAFE)

#### PT-MEMSAFE.1: UCA-MEMSAFE.1 → TE-1 (PC viewer 権限昇格)

| 項目 | 内容 |
|---|---|
| **検証目的** | 既存「Rust + bounded(3)」防御の境界テスト存在確認 |
| **既存実装** ✏️ v1.7.1 確定 | `pipeline.rs:31` で `PACKET_CHANNEL_CAPACITY: usize = 3` 定義 + `pipeline.rs:210` で `mpsc::sync_channel::<RawPacket>(3)` 使用。ADR-008 言及の bounded(3) は **実装存在を確認** |
| **既存テスト** ✏️ v1.7.1 確定 | `pipeline.rs:637` の `test_pipeline_stats` 1 件のみ。**容量 3 超過時の sync_channel ブロック挙動を直接検証する unit test は存在せず** |
| **追加すべきテスト** | `pipeline.rs::test_sync_channel_blocks_at_capacity_3` (新規): producer が 3 個 send 後、4 個目で blocking, consumer が 1 個 recv で再開を確認 |
| **対応対策** | TEST_PLAN §6.2 で予定済 (新規 unit test 30 行程度) |

---

## §5 Safety Constraints 受け入れ基準マトリクス

各 SC について、合否判定の数値基準と測定手段を一覧化。

| SC | 受け入れ基準 (定量) | 測定経路 | 自動テスト ID |
|---|---|---|---|
| **SC-1** | データパス停止 ≤ 3 秒 で回復 OR 通知 | metrics packet `connection_state_t` 推移 | TC-VIEW.2, TC-B1.4 |
| **SC-2** | `fps_runtime × frame_size_avg` ≤ tx_buff スループット (134 ms 換算上限) | metrics packet `tcp_send_time_avg_ms` + `fps_runtime` + `frame_size_avg` 相関 | TC-A1.2 |
| **SC-3** | 単位時間 60s で reconnect ≤ 5 回、6 回目で FAILED + 通知 | metrics `reconnect_count_60s` | TC-B1.3, TC-B1.4 |
| **SC-4** | 認証されない TCP 接続は受け付けない (Option B 以降) | tshark で TLS handshake 確認 | PT-AUTH.1 |
| **SC-5** | `wifi_config.h` の plaintext push が pre-commit で阻止 | `git commit` の exit code | TC-O.2 |
| **SC-6** | 容量上限 (1 GB) 到達で最古削除、書込み継続 | `df -h` + recording 連続性 | TC-C1.2 |
| **SC-7** | tx_buff 飽和を検知して fps_runtime 自動低減 | `tcp_send_time` 暴騰 + `fps_runtime` 低下相関 | TC-A1.2 |
| **SC-8** | 環境外 (温度) 検知で警告 metrics + Safe Mode | 環境センサ metrics (M-9 実装後) | (Phase 13+) |
| **SC-9** | FW ブート時の完全性検証 (将来) | secure boot log | (Phase 13+) |
| **SC-10** | metrics 連続欠落 ≥ 10 秒で副経路通知 | USB CDC-ACM 経路 + LED | TC-D1.1 |

---

## §6 既存テスト基盤とのギャップと施策

### 6.1 Spresense 側テスト基盤 (現状 0 件)

最大のブロッカ。以下のいずれかを Phase 12 初期で導入する必要がある:

| 案 | 内容 | コスト | 所要 |
|---|---|---|---|
| **A. NuttX ユーザモード simulator** | NuttX を Linux ホストで起動 (`./tools/configure.sh sim:nsh`) し、camera/wifi をモック化 | 中 (mocking が大) | 2 週間 |
| **B. HIL シナリオ (推奨)** | 実機 Spresense + テスト治具で `tests/hil/*.sh` を運用 | 小 (シェル + シリアル) | 1 週間 |
| **C. Unit test on host** | 純粋ロジック (`fps_controller.c` の PID 計算等) を Linux でビルド+テスト | 小 | 数日 |

→ **推奨**: B + C の組み合わせ。A は ROI 低 (mocking 負債)。

### 6.2 PC 側追加テスト

`TEST_COVERAGE_BASELINE.md` の 31 件に以下を追加:

| 追加 unit test | 対象 UCA | 行数推定 |
|---|---|---|
| `protocol.rs::crc_fail_drops_packet` | UCA-VIEW.1 | 20 |
| `tcp_connection.rs::bounded_drops_when_over_3` | UCA-MEMSAFE.1 境界 | 30 |
| `mp4_recorder.rs::rotation_at_1gb` | UCA-C1.2 (M-3 後) | 50 |
| `metrics.rs::oversize_frame_flag` | UCA-STREAM.2 (M-19 後) | 25 |

### 6.3 CI 統合計画

```yaml
# .github/workflows/stamp_stpa.yml (案)
on: [pull_request, push]
jobs:
  unit:
    runs-on: ubuntu-latest
    steps:
      - run: cd Rust_ws/security_camera_viewer && cargo test
      - run: cd Rust_ws/security_camera_viewer && cargo llvm-cov --summary-only
  hil:
    runs-on: self-hosted  # Spresense 実機接続済
    if: contains(github.event.head_commit.message, '[hil]')
    steps:
      - run: tests/hil/run_all.sh
  pentest:
    runs-on: ubuntu-latest
    if: github.event_name == 'workflow_dispatch'
    steps:
      - run: tests/pentest/run_all.sh
```

---

## §7 カバレッジ KPI

### 7.1 UCA カバレッジ指標

| 指標 | 定義 | v1.0 ベースライン | Phase 12 目標 | Phase 13 目標 |
|---|---|---|---|---|
| **UCA-Reproduce-%** | 再現テストがある UCA 数 / 全 UCA 数 | 0 / 45 = **0%** | 20 / 45 ≥ **44%** (P0 UCA) | 45 / 45 = **100%** |
| **SC-Verify-%** | 自動測定で合否判定可能な SC 数 / 全 SC 数 | 0 / 10 = **0%** | 7 / 10 = **70%** (SC-8/9 除く) | 10 / 10 = **100%** |
| **UCA-Regression-%** | 過去再現済 UCA が CI で継続検証されている割合 | 0% | ≥ 80% | 100% |
| **STPA-Sec-Pentest-%** | UCA-Def 14 件中 ペネトレ test がある割合 | 0% | Option B 採用時 ≥ 21% (AUTH/RL) | Option D 採用時 100% |

### 7.2 既存メトリクスとの整合

- `TEST_COVERAGE_BASELINE.md` の **行カバレッジ** は本指標と独立。両方を計測する
- `FUNCTIONAL_SPEC_AUDIT.md` の機能カバレッジは別軸 (機能数 vs 仕様数)

---

## §8 段階導入計画

### 8.1 Phase 12.1 (即着手 / 1〜2 週間)

| TC | 緩和される UCA | 必要対策 | 工数目安 |
|---|---|---|---|
| TC-O.2 | UCA-O.2 (cred 漏洩) | M-5 (pre-commit) | 0.5 日 |
| TC-C1.2 | UCA-C1.2 (ローテ) | M-3 (1GB ローテ) | 2 日 |
| TC-VIEW.1 | UCA-VIEW.1 | unit test 追加 (既存 protocol.rs) | 0.5 日 |
| TC-VIEW.2 | UCA-VIEW.2 | M-20 (systemd) | 0.5 日 |
| TC-A1.1, A1.3 | UCA-A1.* | 現状再現テストのみ (M-1 は後続) | 1 日 |

### 8.2 Phase 12.2〜12.3 (主要対策実装 / 4〜6 週間)

| TC | 緩和される UCA | 必要対策 | 工数目安 |
|---|---|---|---|
| TC-A1.2, B1.3 | UCA-A1.2, B1.3 | M-1 (多変数 PID), M-2 (動的閾値) | 各 1 週間 |
| PT-AUTH.1 | UCA-AUTH.1 | M-8 (TLS-PSK or アプリ層認証) | 2 週間 |
| PT-RL.1, RL.2 | UCA-RL.1, RL.2 | M-15 (rate limit) | 1 週間 |

### 8.3 Phase 13+ (HW 変更 / 構造的天井対応)

| TC | 緩和される UCA | 必要対策 |
|---|---|---|
| TC-MEM.1 | UCA-MEM.1 | M-23 + (究極は Tier 2/3 移行) |
| PT-FB.1 | UCA-FB.1 | M-16 (Tier C 自動切替) |
| TC-O.3 | UCA-O.3 | M-9 (環境センサ) |

---

## §9 リスクと制約

1. **HIL 試験環境構築コスト** — Spresense 実機 + AP + 攻撃用 Linux ホストの 3 点セットを CI 自動化するには self-hosted runner 必須
2. **テスト中の WiFi 環境** — 2.4 GHz 帯の Jamming テスト (PT-FB.1) は電波法令上、シールドルーム必要
3. **UCA-MEMSAFE.1 境界テスト不在** ✏️ v1.7.1 確定 — `pipeline.rs:31` の `PACKET_CHANNEL_CAPACITY=3` + `sync_channel(3)` 実装は確認済。境界テスト追加が必要 (PT-MEMSAFE.1 詳細参照)
4. **観測不可な変数** — UCA-A1.2 の `tx_buff` 占有率は GS2200M ベンダー BB のため直接観測不可。代理指標 (RTT ジッタ / send 完了時刻分散) で間接検証する必要あり (親文書 §6.1 推定手法と連動)
5. **本書実装側未確認項目** — UCA-VIEW.1 (CRC 失敗時破棄) と UCA-MEMSAFE.1 (bounded 境界) は実装/テスト両方の grep 確認が未完。本書実装フェーズ冒頭で確認すること

---

## 付録 A. テストスクリプト雛形

### A.1 HIL テスト雛形 (`tests/hil/_template.sh`)

```bash
#!/usr/bin/env bash
set -euo pipefail
# UCA-* HIL test template
SPRESENSE_SERIAL=${SPRESENSE_SERIAL:-/dev/ttyUSB0}
PC_VIEWER_HOST=${PC_VIEWER_HOST:-localhost}

# 1. Setup
SPRESENSE_LOG=$(mktemp)
( picocom -b 115200 -d 8 -p 1 -f n "$SPRESENSE_SERIAL" > "$SPRESENSE_LOG" 2>&1 ) &
PICOCOM_PID=$!
trap "kill $PICOCOM_PID 2>/dev/null || true" EXIT

# 2. Inject condition (override per-UCA)
# e.g. sudo tc qdisc add dev wlan0 root netem delay 200ms

# 3. Run for N seconds, collect metrics
sleep 60

# 4. Assert (override per-UCA)
# e.g. grep -q "fps_runtime=5" "$SPRESENSE_LOG"
```

### A.2 Pentest テスト雛形 (`tests/pentest/_template.sh`)

```bash
#!/usr/bin/env bash
set -euo pipefail
TARGET=${TARGET:-192.168.1.x}

# CA-ADV.* 実行
# e.g. nc "$TARGET" 8888 > /tmp/capture.bin

# 期待: Option X 採用後は失敗するはず
# Option 未採用時の現状記録
```

---

## 関連文書

- 親: [`STAMP_STPA_ANALYSIS.md`](STAMP_STPA_ANALYSIS.md) v1.1
- 既存テスト: [`TEST_COVERAGE_BASELINE.md`](../performance/TEST_COVERAGE_BASELINE.md) v1.1
- 既知の攻撃手順: [`THREAT_MODEL.md`](../risk_analysis/THREAT_MODEL.md) §5 (DREAD 各項目に再現手順あり)
- 失敗モード根拠: [`FMEA.md`](../risk_analysis/FMEA.md)
- 未着手タスク: [`PENDING_NFR_WORK.md`](../PENDING_NFR_WORK.md) (X-8 と統合)
