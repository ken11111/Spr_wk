# Primary Use Cases (主要ユースケース)

**バージョン**: 1.0
**作成日**: 2026-05-02
**形式**: Cockburn UC フォーマット (簡略化) — 主アクター / 前提条件 / 成功保証 / 主シナリオ / 例外シナリオ / 関連 (要求/ADR/QAS/実装) の 5 軸固定

**前提**: アクター定義は [`actors.md`](actors.md), 異常系横断は [`exception_scenarios.md`](exception_scenarios.md), 全体図は [`use_case_overview.puml`](use_case_overview.puml) を参照。

---

## UC-1: システム起動

| 項目 | 内容 |
|---|---|
| **主アクター** | 👤 設置者 (PC) + 👤 運用者 (PC), 🤖 Spresense デバイス (自動) |
| **前提条件** | Spresense にファームウェア書き込み済、wifi_config.h に正しい SSID/Password、WiFi AP 動作中、PC viewer ビルド済 |
| **成功保証** | Spresense が WiFi 接続後 TCP/8888 で listen 開始、PC viewer が接続成功してリアルタイム映像表示 |

**主シナリオ**:
1. 設置者が Spresense に電源を投入 (Q18: 自動起動)
2. NuttX が `camera_app_main` を main task として起動
3. main task が camera_manager / wifi_manager / tcp_server を順に init
4. 5 スレッドを spawn (camera / usb / control / + main + gs2200m driver)
5. WiFi AP に WPA2-PSK で associate (`wifi_manager.c:152`, IW_AUTH_WPA_VERSION_WPA2)
6. DHCP で IP 取得、TCP/8888 で listen 開始
7. 運用者が PC viewer を手動起動 (Q18: 手動)
8. PC viewer が TCP 接続、handshake (sync_word 検証) を経てストリーム受信開始

**例外シナリオ**:
- WiFi 接続失敗 (E1): SSID 誤り or AP 不在 → 起動失敗、シリアルログにエラー表示
- DHCP タイムアウト (E2): `DHCP_TIMEOUT_SEC=10` で諦める → 起動失敗
- カメラ初期化失敗 (E3): ISX012 hardware error → ladder cleanup で全リソース解放

**関連要求 (Q番号)**: Q4 (通信 I/F), Q18 (起動モード), Q21 (PC 環境)
**関連 ADR**: なし
**関連 QAS**: なし (UC-1 は前提のため QAS 対象外)
**関連実装**: `camera_app_main.c` (main task), `wifi_manager.c` (WPA2-PSK), `tcp_server.c:tcp_server_init`

---

## UC-2: 常時 MJPEG ストリーミング (主シナリオ)

| 項目 | 内容 |
|---|---|
| **主アクター** | 👤 運用者 (PC viewer 閲覧), 🤖 Spresense デバイス (主動作) |
| **前提条件** | UC-1 完了、PC viewer 接続中、TCP Health Monitor 健全 |
| **成功保証** | リアルタイム映像が PC viewer に継続表示 (~6.74 fps Phase 8 実測ベース)、平均遅延 < 1s |

**主シナリオ**:
1. camera_thread が ISX012 から V4L2 経由で JPEG フレーム取得 (~33 ms 周期 @ 30 fps target)
2. `frame_statistics_update` で移動窓統計を更新 (Phase 11 入力源)
3. `frame_queue_push(g_action_queue)` でキューに投入
4. usb_thread が `frame_queue_pull` で取得 → `mjpeg_pack_frame` で MJPEG パケット化 + CRC-16-CCITT
5. `tcp_server_send_with_reconnect` で TCP/8888 へ送信 (構造的天井 #1 で 134ms 平均)
6. PC viewer 側 TCP Reader thread が受信、bounded(3) channel に投入
7. JPEG Decoder thread がデコード → DecodedFrame
8. GUI thread が egui texture upload + 描画
9. control_thread (Phase 10 PID, 10Hz) が queue 深度を観測し、必要なら FPS 動的調整 (`camera_set_fps_runtime`)

**例外シナリオ**:
- TCP send 遅延スパイク (E1): `tcp_avg_send_us` が閾値超過 → TCP Health Monitor が記録 → UC-5 へ遷移可能性
- queue overflow (E2): camera_thread の生成 > usb_thread の消費 → 古いフレームをドロップ (Phase 8/9 実測ドロップ率 53.7%/74.2%)

**関連要求 (Q番号)**: Q1 (解像度/FPS), Q4 (通信 I/F), Q5 (プロトコル), Q16 (許容遅延), Q17 (ドロップ許容)
**関連 ADR**: ADR-005 (3-thread Pipeline), ADR-008 (bounded channel)
**関連 QAS**:
- QAS-2 (QVGA 30fps での MJPEG batch 送信) — 1s Must ✅ / 100ms Want 🔴
- QAS-3 (Phase 10 PID 制御) — ✅ 統合済
- QAS-10 (action_queue 動的調整) — 🟡 効果定量化未
**関連実装**: `camera_threads.c:camera_thread_func / usb_thread_func`, `mjpeg_protocol.c:mjpeg_pack_frame`, `tcp_server.c:tcp_server_send_with_reconnect`, `Rust_ws/security_camera_viewer/src/pipeline.rs`

---

## UC-3: 動き検出時の録画

| 項目 | 内容 |
|---|---|
| **主アクター** | 👤 運用者 (受動的に確認), 🤖 PC viewer プロセス (主動作) |
| **前提条件** | UC-2 ストリーミング動作中、PC viewer の動き検出機能有効、ring_buffer に過去フレーム保持 |
| **成功保証** | 動き検出時に MP4 録画開始、ring_buffer の過去数秒も含めて保存 |

**主シナリオ**:
1. PC viewer の `motion_detector.rs` が連続フレームの差分を計算
2. ヒステリシス制御で「動きあり」と判定 → motion event 発火
3. `mp4_recorder.rs` が起動、ring_buffer の保持フレーム (動き検出前数秒) を先頭に追加
4. ffmpeg subprocess (`libx264 -preset medium`) で MP4 エンコード開始
5. 動き継続中はリアルタイムで MP4 に追記
6. 「動きなし」が一定時間続いたら録画停止 → ファイル close
7. 1GB 上限到達でローテーション (※ 現状未実装、技術負債 — Q8)

**例外シナリオ**:
- ffmpeg 起動失敗 (E1): subprocess エラー → ログ記録、録画スキップ
- 1GB 到達 (E2): 現状 **ローテーション未実装** で書き込み停止 (技術負債)
- ディスク容量不足 (E3): write エラー → 録画失敗ログ

**関連要求 (Q番号)**: Q6 (録画トリガー: 動き検出), Q7 (MP4), Q8 (1GB 上限), Q12 (動き検出は PC 側), Q14 (タイムスタンプメタデータ)
**関連 ADR**: なし
**関連 QAS**: なし
**関連実装**: `Rust_ws/security_camera_viewer/src/motion_detector.rs`, `mp4_recorder.rs`, `ring_buffer.rs`

---

## UC-4: 録画ファイル管理

| 項目 | 内容 |
|---|---|
| **主アクター** | 👤 運用者 (確認・削除), 🤖 PC viewer プロセス (容量監視) |
| **前提条件** | UC-3 で録画ファイル蓄積中、ローカル FS にアクセス権限 |
| **成功保証** | 容量上限 (1GB) 到達前に運用者がファイル管理可能、外部プレーヤー (VLC等) で再生可能 |

**主シナリオ**:
1. 運用者が録画ディレクトリの MP4 ファイル一覧を確認 (FS 経由)
2. 必要なファイルを外部プレーヤー (VLC) で再生 (Q11: アプリ内再生は未実装)
3. 不要ファイルを手動削除
4. PC viewer の `MAX_RECORDING_SIZE = 1_000_000_000` (1GB) 上限が近づいたら手動で容量管理

**例外シナリオ**:
- 1GB 到達 (E1): **自動ローテーション未実装** → 書き込み停止 (重大な技術負債、Q8 確認推奨)
- 時間分割未実装 (E2): イベント分割のみ実装 (Q9 確認、現状運用は問題ないが将来要改善)

**関連要求 (Q番号)**: Q7 (MP4 形式), Q8 (容量制限 1GB), Q9 (時間分割未実装), Q11 (アプリ内再生未実装)
**関連 ADR**: なし
**関連 QAS**: なし
**関連実装**: `Rust_ws/security_camera_viewer/src/gui_main.rs:MAX_RECORDING_SIZE`, `mp4_recorder.rs`

---

## UC-5: TCP 切断時の自動復旧

| 項目 | 内容 |
|---|---|
| **主アクター** | 🤖 Spresense デバイス (Auto-Reconnect FSM 実行), 🤖 PC viewer プロセス (再接続待ち) |
| **前提条件** | UC-2 ストリーミング動作中、TCP 切断イベント発生 (RST or タイムアウト) |
| **成功保証 (設計)** | 3 秒以内に再接続完了、ストリーミング再開 |
| **成功保証 (現実)** | 🔴 ADR-002 v1.1 で「逆効果」判明 — 多くの場合再接続失敗 or 失敗後 FAILED 固着 |

**主シナリオ**:
1. usb_thread が `tcp_server_send_with_reconnect` 経由で send → エラー検知 (broken pipe / EPIPE)
2. `tcp_server_handle_disconnect` が発火、reconnect_count++ (`tcp_server.c:502`)
3. exponential backoff: `wait_ms = 1000 + (reconnect_count - 1) × 2000`
4. backoff 後、`tcp_server_wait_reconnect` で client の再接続を accept 待ち
5. PC viewer が `tcp_connection.rs` 経由で自動再接続試行 (RECONNECT_MAX_ATTEMPTS=10)
6. 接続成立 → ストリーミング再開

**例外シナリオ (現実の失敗パス)**:
- E1: 4 回目以降 RST 拒否 → reconnect 失敗
- E2: `reconnect_count >= TCP_RECONNECT_MAX (=5)` → FAILED 状態固着 → UC-7 へ遷移
- E3: 再接続中も usb_thread は他処理を継続 → PC FPS が悪化 (Phase 9 実測 6.74 → 2.77)

**関連要求 (Q番号)**: Q19 (エラー回復: 自動再接続)
**関連 ADR**: ADR-002 v1.1 (「再接続が逆効果」発見と全面改訂)
**関連 QAS**:
- QAS-1 (GS2200M バッファ枯渇時の予防的再接続) — 🔴 逆効果判明
- QAS-4 (TCP Health Monitor スパイク検知) — ✅ 検知機構機能
- QAS-8 (5 回失敗後の人手介入) — 🔴 手順未定義
**関連実装**:
- `tcp_server.c:602` (`tcp_server_send_with_reconnect`)
- `tcp_server.c:483, 544` (`handle_disconnect`, `wait_reconnect`)
- 健全性状態遷移: `gs2200m_health_state_machine.puml`

---

## UC-6: 設定変更

| 項目 | 内容 |
|---|---|
| **主アクター** | 👤 設置者 (再ビルド時) or 👤 保守者 (Phase 10 FPS 動的時) |
| **前提条件** | (再ビルド時) Spresense ソースアクセス + ビルド環境 / (動的時) UC-2 動作中 |
| **成功保証** | 設定変更が次回起動 (再ビルド時) or 即座 (動的時) に反映 |

**主シナリオ A (再ビルド設定変更)**:
1. 設置者が `apps/examples/security_camera/config.h` (or wifi_config.h, mjpeg_protocol.h) 編集
2. Spresense SDK でビルド (~数分)
3. ファームウェアを Spresense に書き込み
4. 電源再投入 → UC-1 で新設定反映

**主シナリオ B (Phase 10 動的 FPS 変更)**:
1. control_thread が PID 制御で FPS 出力決定 (10 Hz)
2. `camera_set_fps_runtime(new_fps)` 経由で V4L2 ioctl 発行
3. ISX012 が次フレームから新 FPS で動作

**例外シナリオ**:
- E1 (再ビルド): WiFi 認証情報の git track 問題 (X-7) — 設置者がコミットすると認証情報が公開される
- E2 (動的): FPS 変更が短時間に頻発 → 制御の不安定化、QAS-3 stability 監視で検出

**関連要求 (Q番号)**: Q23 (Rust クレート), Q1 (FPS 動的範囲 5-30), Q22 (Phase 分け)
**関連 ADR**: なし
**関連 QAS**: QAS-3 (PID 制御 — ✅ 統合済)
**関連実装**:
- 設定: `config.h`, `wifi_config.h`, `mjpeg_protocol.h`
- Phase 10 動的: `camera_manager.c:camera_set_fps_runtime`, `fps_controller.c:fps_controller_update`

---

## UC-7: 障害時の人手介入

| 項目 | 内容 |
|---|---|
| **主アクター** | 👤 保守者, 🤖 Spresense デバイス (FAILED 状態) |
| **前提条件** | UC-5 で再接続 5 回失敗後 FAILED 状態固着、または重大ハード障害 |
| **成功保証** | 保守者が介入手順を把握し、システム復旧 (再起動 / Tier 切替判断) を実施 |

**主シナリオ**:
1. 保守者がアラート (PC viewer の FAILED 表示 or syslog 監視) で気付く
2. シリアルコンソール接続でログ確認 (Spresense)
3. 状況判定:
   - 一時的: Spresense 再起動で復旧試行
   - 慢性的 (構造的天井 #1 が原因): Tier C (USB-only) または Tier 2/3 への移行検討
4. 必要なら ADR-006 GATE-1 評価で Tier 移行判断
5. 復旧後、本イベントを TECHNICAL_DEBT_REGISTER に記録

**例外シナリオ**:
- E1: **運用ランブックが文書化されていない** (X-4 タスク) — 保守者は手順を毎回再構築する必要
- E2: 保守者不在 (深夜・週末) → サービス長時間停止

**関連要求 (Q番号)**: Q19 (エラー回復、5 回失敗後の対応は要求書 v1.0 で「未定義」)
**関連 ADR**: ADR-002 v1.1, ADR-006 (Tier 移行判定)
**関連 QAS**:
- QAS-8 (5 回失敗後の人手介入) — 🔴 手順未定義
- QAS-6 (Full HD 移行判定) — Tier 切替判断時に活用
**関連実装**: なし (人手プロセス、X-4 運用ランブック作成タスクで埋める)

---

## UC マッピング (要求 → 機能仕様)

| UC | 主要要求 (Q) | 関連 functional/ SPEC |
|---|---|---|
| UC-1 起動 | Q4, Q18, Q21 | (起動シーケンス未文書化) |
| UC-2 ストリーミング | Q1, Q4, Q5, Q16, Q17 | CAMERA_CAPTURE_SPEC, STREAMING_SPEC, ADAPTIVE_CONTROL_SPEC |
| UC-3 動き検出録画 | Q6, Q7, Q12, Q14 | RECORDING_SPEC |
| UC-4 ファイル管理 | Q7, Q8, Q9, Q11 | RECORDING_SPEC |
| UC-5 切断復旧 | Q19 | (ADR-002 で代替) |
| UC-6 設定変更 | Q23, Q22 | CONTROL_ENGINEERING_SPEC (Phase 10 部分) |
| UC-7 人手介入 | Q19 (空白) | (X-4 運用ランブック予定) |

---

## 関連文書

- 全体図: [`use_case_overview.puml`](use_case_overview.puml)
- アクター: [`actors.md`](actors.md)
- 異常系横断: [`exception_scenarios.md`](exception_scenarios.md)
- 上位要求: [`../../01_requirements/FUNCTIONAL_REQUIREMENTS.md`](../../01_requirements/FUNCTIONAL_REQUIREMENTS.md) v1.0
- 機能仕様: [`../functional/`](../functional/)
- 品質シナリオ: [`../quality/QUALITY_ATTRIBUTE_SCENARIOS.md`](../quality/QUALITY_ATTRIBUTE_SCENARIOS.md)
- ADR: [`../../03_achievements/architecture_decisions/`](../../03_achievements/architecture_decisions/)
