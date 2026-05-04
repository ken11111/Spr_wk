# Glossary (用語集) — arc42 §12

**バージョン**: 1.0
**作成日**: 2026-05-01
**目的**: 文書群における Phase 番号定義の意味揺れと専門用語を一義化する正規定義集

**読み方**: 用語は § 単位でカテゴリ分け。各エントリは「定義 + 出典 + 関連用語」の固定形式。

---

## §1 Phase 定義 (公式 = 制御工学系)

> **✅ 文書間の定義揺れ解消済み (2026-05-03)**:
> `MASTER_ROADMAP_2026.md` v1.0 では Phase 10 = AI統合・マルチカメラ / Phase 11 = プラットフォーム化 / Phase 12 = 商用化 だったが、**v2.0 (2026-05-02) で Phase 13 / 14 / 15 に再割り当て** (X-2 タスク完了)。本リポジトリの Phase 10 / 11 公式定義は下記**制御工学系**で確定。Phase 12 は「Tier 移行 + セキュリティ判断 + 残課題対応」として再定義。

### Phase 1
**定義**: QVGA MJPEG 基本実装 (USB CDC-ACM 経由)。プロトコル骨格と基本ストリーミング確立。
**出典**: `05_future_actions/phase_completed/PHASE_COMPLETION_RECORDS.md`
**関連**: Phase 1A, Phase 1.5

### Phase 1A
**定義**: Phase 1 のサブフェーズ — カメラ I/F とプロトコルの初期実装。
**出典**: `04_issues_challenges/LESSONS_LEARNED.md`
**関連**: Phase 1, Phase 2

### Phase 1.5
**定義**: VGA (640×480) 最適化フェーズ。実測 37.33 fps 達成。
**出典**: `03_achievements/phase_deliverables/PHASE1.5_VGA_OPTIMIZATION.md`
**関連**: Phase 1, 構造的天井 #1

### Phase 2
**定義**: Rust PC 側 MJPEG ビューワ実装 (`security_camera_viewer`)。
**出典**: `05_future_actions/phase_completed/PHASE_COMPLETION_RECORDS.md`
**関連**: 3-thread Pipeline, bounded(3) channel

### Phase 5
**定義**: ドキュメント構造改革。本 `docs/security_camera/` 階層化。
**出典**: `PHASE5_COMPREHENSIVE_REPORT.md`
**関連**: arc42

### Phase 7.2a
**定義**: MJPEG batching 機能追加 (`MJPEG_BATCH_SIZE = 2`)。バッチサイズ削減履歴: 1→3→2。
**出典**: `apps/examples/security_camera/mjpeg_protocol.h:42`
**関連**: MJPEG batch

### Phase 7.3.3
**定義**: シングルスレッド方式のドロップ統計分析。
**出典**: `04_issues_challenges/PHASE8_BUFFER_QUEUE_COMPREHENSIVE_ANALYSIS.md`
**関連**: Phase 8 (3-thread Pipeline へ移行)

### Phase 8
**定義**: 3-thread Pipeline アーキテクチャ確立 (TCP Reader → JPEG Decoder → GUI Thread)。
**出典**: `03_achievements/architecture_decisions/system_architecture/ADR_005_ARCHITECTURE_THREE_THREAD_PIPELINE.md`
**関連**: bounded(3) channel, ADR-005, ADR-008

### Phase 9
**定義**: Spresense 側 自動再接続 (Auto-Reconnect FSM) 実装。max=5, exponential backoff。
**出典**: `apps/examples/security_camera/tcp_server.c:602` (`tcp_server_send_with_reconnect`)
**関連**: Phase 9.2, ADR-002

### Phase 9.2
**定義**: TCP Health Monitor 統合 (移動平均 + spike 検出)。58B metrics packet 拡張。
**出典**: `03_achievements/architecture_decisions/system_architecture/ADR_002_NETWORKING_TCP_HEALTH_MONITORING.md` v1.1
**関連**: Phase 9, Health State, 予防的再接続

### Phase 10 (公式)
**定義**: 制御工学統合 — `fps_controller.c` による PID 制御 (Kp=0.15, Ki=0.02, setpoint=3.5, 100ms 周期 / 10 Hz)。`control_thread_func` が独立スレッドで動作。
**出典**: `01_requirements/phase10_control_engineering_requirements.md` / git commit `2ab7bfc`
**関連**: PID 制御, 制御周期, control_thread_func
**📜 旧定義の解消**: `MASTER_ROADMAP_2026.md` v1.0 の旧 Phase 10 (AI統合) は v2.0 で **Phase 13** に再割り当て済 (2026-05-02)

### Phase 11 (公式)
**定義**: 適応制御拡張 — 多変数 (6 変数) + 予測制御の仕様策定段階。`enhanced_control.h` に API 宣言済だが **`.c` 未実装** (caller 0 件)。
**出典**: `01_requirements/phase11_adaptive_control_requirements.md` / git commit `e371f94`
**関連**: 適応制御, 予測制御, enhanced_control
**📜 旧定義の解消**: `MASTER_ROADMAP_2026.md` v1.0 の旧 Phase 11 (プラットフォーム化) は v2.0 で **Phase 14** に再割り当て済 (2026-05-02)

### Phase 12 (公式, 確定方針 2026-05-05)
**定義**: **Tier 1 維持 + 家庭用 運用品質確立** — 新規ハード導入なし、家庭 LAN 内個人運用を最終ターゲットとして実用品質を確立。実測 (CPU / 24h 連続 / カバレッジ) → 自動再接続戦略改定 → セキュリティ Option B 段階実装 → 仕様恒久化 (要求書 v1.1) の 6 サブ Phase。
**出典**: `05_future_actions/phase_planned/Phase12_実施計画書.md` v1.0 / git commit (Phase 12 キックオフ)
**関連**: Tier 1, セキュリティ Option B (LAN 隔離 + アプリ層 PSK + IP allowlist + ログ署名)
**WONT FIX 確定**: Q1 Full HD / Q3 H.264 transport / Q5 RTSP / Q16 Want 100ms / Q24 TLS
**📜 旧定義の解消**: `MASTER_ROADMAP_2026.md` v1.0 の旧 Phase 12 (商用化) は v2.0 で **Phase 15** に再割り当て済 (2026-05-02)

### Phase 13 / 14 / 15 (構想段階, 旧 Phase 10 / 11 / 12)
**定義**:
- Phase 13 = AI統合・マルチカメラ (旧 Phase 10)
- Phase 14 = プラットフォーム化 (旧 Phase 11)
- Phase 15 = 商用化・事業拡大 (旧 Phase 12)

**出典**: `MASTER_ROADMAP_2026.md` v2.0 §3.2, §4.1, §4.2
**関連**: 構想段階のため詳細実装は未着手

---

## §2 構造的天井 (Structural Ceilings)

Spresense + GS2200M 構成における**改変不能な物理/設計上の上限**。性能議論の基礎。

### 構造的天井 #1
**定義**: GS2200M ドライバの送信バッファが **`tx_buff[1] × 1500B` の 1 個のみ**。送信が完全直列化される。
**症状**: 122 KB の MJPEG batch は約 84 回の SPI 転送に分割。同時送信不可。
**出典**: `spresense/nuttx/drivers/wireless/gs2200m.c:76, 189` (`MAX_PKT_LEN`, `tx_buff`)
**関連文書**: `02_specifications/architecture/SPRESENSE_TCP_CONSTRAINTS.md` §3, ADR-002 v1.1

### 構造的天井 #2
**定義**: NuttX I/O Buffer (IOB) プールが **8 個 × 196B = 1568B のみ**, `IOB_THROTTLE=0` で流量制御無し。
**症状**: 同時並行 TCP/UDP I/O が枯渇しやすい。MJPEG 送信路で内部キューイング不可。
**出典**: `spresense/nuttx/.config` (CONFIG_IOB_NBUFFERS, IOB_BUFSIZE)
**関連文書**: `02_specifications/architecture/SPRESENSE_TCP_CONSTRAINTS.md` §5

### 構造的天井 #3
**定義**: usrsock の `PREALLOC_CONNS = 6` でユーザースペースソケット同時利用上限が 6。
**症状**: 多接続シナリオで socket 確保失敗。本プロジェクトは 1 接続なので影響軽微。
**出典**: `spresense/nuttx/.config` (CONFIG_NET_USRSOCK_*)
**関連文書**: `02_specifications/architecture/SPRESENSE_TCP_CONSTRAINTS.md` §6

### 構造的天井 #4
**定義**: CXD5602 RAM 1.5 MB の固定上限。アプリ + V4L2 RING (192KB) + action_queue (~360KB) + MJPEG batch (~122KB) + SO_SNDBUF (256KB 要求) で約 60% 占有。
**症状**: Full HD (200KB+/frame) 移行時に action_queue 1.4MB+ で破綻。
**出典**: `02_specifications/architecture/SPRESENSE_TCP_CONSTRAINTS.md` §2 メモリ予算表
**関連**: Tier 1, Tier 3

### 構造的天井 #5
**定義**: GS2200M モジュール内部 (ベンダーファームウェア / TCP/IP スタック) は**非公開・改変不能**。
**症状**: 性能チューニングは driver/上位の側でしか行えない。バッファや TCP 挙動の調整不可。
**出典**: `02_specifications/architecture/spresense_gs2200m_module_components.puml`
**関連**: NET_TCP_NO_STACK, usrsock RPC

---

## §3 制御工学 用語

### PID 制御
**定義**: Proportional-Integral-Derivative 制御。本プロジェクトは P+I のみ (Kd=0)。
**出典**: `apps/examples/security_camera/fps_controller.c`
**関連**: 適応制御, 制御周期

### 適応制御
**定義**: 入力条件 (複雑度, queue 深度) に応じて PID gain を動的調整する制御方式。Phase 11 仕様に含まれるが**未実装**。
**出典**: `apps/examples/security_camera/enhanced_control.h` (API 宣言のみ)
**関連**: Phase 11, 予測制御

### 予測制御
**定義**: 5-point linear trend で次フレームのサイズ/負荷を予測し、先回り制御する方式。Phase 11 仕様に含まれるが**未実装**。
**出典**: `enhanced_control.h` (predictive_controller_t 型定義のみ)
**関連**: Phase 11, 適応制御

### 制御周期
**定義**: PID ループの実行周期。Phase 10 では 100 ms (10 Hz)。
**出典**: `apps/examples/security_camera/fps_controller.h:61` (FPS_CONTROLLER_PERIOD_MS)
**関連**: Phase 10, control_thread_func

### control_thread_func
**定義**: Phase 10 PID 制御を実行する独立アプリスレッド。`g_control_thread` で起動。
**出典**: `apps/examples/security_camera/camera_threads.c:1330, 1516`
**関連**: PID 制御, Phase 10

### 予防的再接続
**定義**: TCP Health Monitor がスパイクを検知したら、切断を待たず先回りで再接続を試みる戦略。Phase 9.2 で設計、ADR-002 v1.1 で「逆効果」と判明。
**出典**: ADR-002 v1.1 §4
**関連**: Phase 9.2, Auto-Reconnect, Health State

---

## §4 通信・実装用語

### NET_TCP_NO_STACK
**定義**: NuttX kernel の TCP スタックをバイパスし、ユーザー空間 (usrsock) 経由で外部に委譲するビルド設定。本プロジェクトでは **`y` (有効)**。
**出典**: `spresense/nuttx/.config` (CONFIG_NET_TCP_NO_STACK)
**関連**: usrsock RPC, GS2200M

### usrsock RPC
**定義**: NuttX kernel が socket システムコールをユーザー空間プロセスに委譲する仕組み。本プロジェクトでは GS2200M ドライバへの委譲経路。
**出典**: NuttX docs (Userspace Socket)
**関連**: NET_TCP_NO_STACK, GS2200M driver task

### IOB
**定義**: NuttX I/O Buffer。ネットワーク・通信路で使う固定サイズのチャンクバッファ。
**出典**: `spresense/nuttx/.config` (CONFIG_IOB_*)
**関連**: 構造的天井 #2

### tx_buff
**定義**: GS2200M ドライバの送信バッファ。**1 個のみ × 1500B**。
**出典**: `spresense/nuttx/drivers/wireless/gs2200m.c:189`
**関連**: 構造的天井 #1

### BULK_THRESHOLD
**定義**: GS2200M ドライバの bulk モード切替閾値 (8 KB)。これを超える送信で bulk モードに切替。
**出典**: `spresense/nuttx/drivers/wireless/gs2200m.c:99`
**関連**: tx_buff

### MJPEG batch
**定義**: 複数 JPEG フレームを 1 パケットに連結する送信単位。`MJPEG_BATCH_SIZE = 2` (約 122 KB peak)。
**出典**: `apps/examples/security_camera/mjpeg_protocol.h:43, 47`
**関連**: Phase 7.2a, sync_word

### CRC-16-CCITT
**定義**: 16-bit Cyclic Redundancy Check (多項式 0x1021)。MJPEG / metrics packet で整合性検証に使用。逐次計算 (LUT は ADR-004 で計画されたが未実装)。
**出典**: `apps/examples/security_camera/mjpeg_protocol.c` (`mjpeg_crc16_ccitt`)
**関連**: ADR-004, sync_word

### sync_word
**定義**: パケット種別を示すマジックナンバー。`0xCAFEBABE` (MJPEG 単発), `0xCAFEBABF` (MJPEG batch), `0xCAFEBEEF` (metrics)。
**出典**: `apps/examples/security_camera/mjpeg_protocol.h:27, 36, 41`
**関連**: MJPEG batch, metrics packet

### 3-thread Pipeline
**定義**: PC 側 viewer の処理段分離アーキテクチャ。TCP/Serial Reader → JPEG Decoder → GUI thread。bounded channel で接続。
**出典**: `Rust_ws/security_camera_viewer/src/pipeline.rs`
**関連**: Phase 8, ADR-005, bounded(3) channel

### bounded(3) channel
**定義**: PC 側 Pipeline の段間チャネル容量 = 3 (約 150KB peak)。`mpsc::sync_channel(3)`。ADR-005 が当初想定した unbounded を意図的変更。
**出典**: `Rust_ws/security_camera_viewer/src/pipeline.rs:31` (PACKET_CHANNEL_CAPACITY)
**関連**: ADR-008, 3-thread Pipeline

### 自動再接続 (Auto-Reconnect)
**定義**: TCP 切断検知後、exponential backoff (1s + N×2s) で max=5 回再接続を試みる FSM。Phase 9 で実装。
**出典**: `apps/examples/security_camera/tcp_server.c:602` (`tcp_server_send_with_reconnect`)
**関連**: Phase 9, ADR-002 v1.1, 予防的再接続

### TCP Health Monitor
**定義**: 送信時間の移動平均 (8 サンプル) と spike 累積を計測し、metrics packet に同梱して PC 側に送る仕組み。Phase 9.2 で実装。
**出典**: `apps/examples/security_camera/tcp_server.c` (`g_tcp_health`, `tcp_health_update`)
**関連**: Phase 9.2, Health State

### Health State
**定義**: TCP 健全性の 6 状態モデル: HEALTHY → CAUTION → WARNING → CRITICAL → RECONNECTING → FAILED。
**出典**: `02_specifications/architecture/gs2200m_health_state_machine.puml`
**関連**: Phase 9.2, TCP Health Monitor

### metrics packet
**定義**: Spresense → PC へ 3 秒ごとに送る 58-byte 計測パケット。Phase 9.2 で health 4 fields 追加。
**出典**: `apps/examples/security_camera/mjpeg_protocol.h:74` (`metrics_packet_t`)
**関連**: Phase 9.2, sync_word (0xCAFEBEEF)

---

## §5 ハードウェア・デプロイ用語

### Tier 1 (現状)
**定義**: Spresense + GS2200M (現状トポロジ)。本プロジェクトの実証済みベース。
**出典**: `02_specifications/architecture/spresense_deployment_current.puml`
**関連**: 構造的天井 #1, #4

### Tier 2 (ESP32-S3)
**定義**: WiFi 内蔵 SoC (ESP32-S3) への移行候補。SPI 制約解消。Spresense 資産は部分継承。
**出典**: `02_specifications/architecture/spresense_deployment_candidate_a_esp32s3.puml`
**関連**: 移植コスト 14 週

### Tier 3 (RPi CM5)
**定義**: Linux + V4L2 + HW JPEG/H.264 エンコーダ + Gigabit Ethernet。Full HD 30 fps 対応可能。Spresense 資産はほぼ放棄。
**出典**: `02_specifications/architecture/spresense_deployment_candidate_b_rpi_cm5.puml`
**関連**: ADR-006 GATE-1

### Tier C (USB-only)
**定義**: ハード変更なし、GS2200M 経由を論理切断し USB CDC-ACM (12 Mbps) を主経路に昇格。PC viewer 改修のみ (+2-4 週)。
**出典**: `02_specifications/architecture/spresense_deployment_candidate_c_usb.puml`
**関連**: ADR-001

---

## §6 アーキテクチャ手法用語

### arc42
**定義**: アーキテクチャ文書化テンプレート (12 章構成)。本プロジェクトは §10 (Quality Requirements) と §12 (Glossary) に準拠。
**出典**: arc42.org
**関連**: Quality Attribute Scenario, Building Block

### 4+1 View
**定義**: Logical / Process / Development / Physical / Scenarios の 5 視点でアーキテクチャを記述する手法 (Kruchten 1995)。本プロジェクトは Process View と Building Block View を採用。
**出典**: `02_specifications/architecture/spresense_main_board_process_view.puml`
**関連**: arc42, L1/L2

### L1 / L2.A / L2.B / L2.C
**定義**: arc42 階層化ビュー — L1 = メインボード全体 (white-box, 7 building block), L2.A = Capture Pipeline 詳細, L2.B = Streaming + Transport 詳細, L2.C = Adaptive Controller 詳細。
**出典**: `02_specifications/architecture/spresense_main_board_l1_buildingblocks.puml` ほか
**関連**: arc42, Building Block

### Building Block
**定義**: arc42 §5 で扱う黒箱化されたコンポーネント単位。各 Block は責務 + provides/requires インタフェースで記述。
**出典**: arc42 §5
**関連**: L1, L2

### Quality Attribute Scenario (QAS)
**定義**: arc42 §10 標準形式の品質シナリオ (Source / Stimulus / Environment / Artifact / Response / Response Measure)。
**出典**: `QUALITY_ATTRIBUTE_SCENARIOS.md`
**関連**: ISO/IEC 25010

### ISO/IEC 25010
**定義**: ソフトウェア品質モデル国際規格。8 品質属性 (機能適合性 / 性能効率性 / 互換性 / 使用性 / 信頼性 / セキュリティ / 保守性 / 移植性) を定義。
**出典**: ISO/IEC 25010:2011
**関連**: QUALITY_REQUIREMENTS.md
