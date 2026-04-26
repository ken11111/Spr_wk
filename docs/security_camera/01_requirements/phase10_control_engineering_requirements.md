# Phase 10 制御工学統合実装 要求仕様・計画書

**作成日**: 2026-02-04 (Spresense制約統合版)
**Phase番号**: Phase 10 (制御工学統合実装)
**ベース**: Phase 8-9.2制御工学分析 + Spresense仕様調査統合
**目的**: Spresense制約内での制御工学理論実装による自律最適化システム構築
**優先方針**: 実装可能性重視、ハードウェア制約適応、段階的実装

---

## 📋 プロジェクト概要

### 🎯 プロジェクト目標
Phase 8-9.2制御工学分析結果に基づき、PID制御・適応制御・予測制御を統合した**自律最適化セキュリティカメラシステム**を実現。従来の手動調整・固定パラメータから脱却し、制御工学理論による**システム性能とロバスト性の革新的向上**を図る。

### 📊 期待成果 (Spresense制約対応修正版)
```yaml
性能改善目標 (実装可能性基準):
  FPS性能:      5.32fps(Phase C) → 7.0fps (+32%改善, 段階的)
  TCP応答時間:  134ms → 110ms (-18%改善, WiFi変動制約考慮)
  キュー深度:    現行深度 → 理想的0→1→0パターン維持
  FPS安定性:    35.5%CV → <15%CV (制御工学効果)
  メモリ効率:   現行 → +20%向上 (RAM 1.5MB制約内)
  優先度制御:   固定 → 動的調整100-120範囲(NuttX制約)
```

### 🏗️ 対象システム構成 (Spresense制約対応)
```
Target Architecture (Spresense制約統合):
Camera (Spresense/NuttX/C) ←→ GS2200M WiFi ←→ PC Viewer (Rust)

Spresense側実装基盤活用:
├─ camera_manager.c: V4L2 ioctl FPS制御 (1-30fps)
├─ frame_queue.c: 動的バッファ管理 (5-15フレーム, RAM制約)
├─ camera_threads.c: NuttX優先度制御 (100-120範囲)
└─ tcp_server.c: GS2200M健全性監視 (134ms±50ms対応)

制約適応設計:
├─ ISX012 JPEG制御不可 → フレームサイズベース品質推定
├─ RAM 1.5MB制限 → 最大1MB制御バッファ
├─ WiFi変動±50ms → 適応的制御ゲイン調整
└─ NuttX優先度255段階 → 100-120範囲動的制御
```

---

## 1. 実装優先順位と段階計画

### 1.1 短期実装 (Phase 10.1: 2ヶ月) - Spresense制約対応

#### **Priority 1: V4L2フレームレート制御** 🔄 修正
**工数**: 15-25時間 (実装基盤活用)
**リスク**: 低 (V4L2 API使用)
**期待効果**: FPS制御精度+90%, 安定性向上

**実装内容** (Spresense実装基盤活用):
```c
// camera_manager.c 拡張
int camera_set_fps_dynamic(int target_fps) {
    struct v4l2_streamparm parm = {0};
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.timeperframe.numerator = 1;
    parm.parm.capture.timeperframe.denominator = clamp(target_fps, 1, 30);
    return ioctl(g_camera_mgr.fd, VIDIOC_S_PARM, (uintptr_t)&parm);
}

// PID制御器統合
typedef struct {
    float kp, ki;         // Kp=0.15, Ki=0.02 (Phase C調整)
    float error_integral; // 積分項 (-5.0 to +5.0)
    float target_fps;     // 7.0fps初期目標
} fps_pid_controller_t;
```

#### **Priority 2: GS2200M健全性監視改善** 🔄 修正
**工数**: 20-30時間
**リスク**: 低 (既存Phase 9.2基盤)
**期待効果**: WiFi変動±50ms対応, 予測精度+60%

**実装内容** (GS2200M制約対応):
```c
// tcp_server.c 改善
typedef struct {
    uint32_t response_times[60];     // 60秒履歴
    int8_t wifi_signal_strength;     // RSSI値 (-100 to 0)
    uint32_t error_count;
    float health_score;              // GS2200M特性対応
} gs2200m_health_monitor_t;

// GS2200M制約対応計算
static float calculate_health_score_gs2200m(void) {
    float response_factor = 1.0 - (avg_response / 250.0);  // 250ms上限
    float signal_factor = (signal_strength + 100) / 100.0; // RSSI正規化
    return response_factor * 0.6 + signal_factor * 0.4;
}
```

#### **Priority 3: NuttX優先度動的制御** 🔄 修正
**工数**: 15-20時間
**リスク**: 低 (NuttX pthread API使用)
**期待効果**: フレームドロップ-20%, CPU効率+15%

**実装内容** (NuttX制約対応):
```c
// camera_threads.c 拡張
int adjust_thread_priority_nuttx(pthread_t thread, int fps_error) {
    struct sched_param param;
    int new_priority = 110;  // カメラスレッドベース優先度

    if (fps_error > 1.0) {
        new_priority = min(110 + 10, 120);  // 高優先化
    } else if (fps_error < -1.0) {
        new_priority = max(110 - 10, 100);  // 低優先化
    }

    param.sched_priority = new_priority;
    return pthread_setschedparam(thread, SCHED_RR, &param);
}
```

### 1.2 中期実装 (Phase 10.2: 3ヶ月) - Spresense制約対応

#### **Priority 4: 適応的バッファサイズ制御** 🔄 修正
**工数**: 30-40時間 (既存基盤活用)
**リスク**: 中 (RAM制約管理)
**期待効果**: メモリ効率+20%, オーバーフロー-80%

**実装内容** (Spresense RAM制約対応):
```c
// frame_queue.c 拡張
typedef struct {
    uint32_t min_buffers;    // 5 (最小フレーム数)
    uint32_t max_buffers;    // 15 (RAM 1MB制限)
    uint32_t current_buffers; // 現在バッファ数
    uint32_t usage_history[60]; // 使用率履歴 (1分間)
    uint32_t buffer_size;    // 65KB (ISX012 QVGA平均)
} adaptive_buffer_controller_t;

// RAM制約内での動的制御
static int adjust_buffer_count_safe(int target_count) {
    uint32_t required_memory = target_count * 65536;  // 65KB/frame
    if (required_memory > 1048576) return -1;  // 1MB制限
    return frame_queue_reallocate_buffers(target_count);
}
```

#### **Priority 5: インテリジェントフレーム選択** 🔄 新規
**工数**: 25-35時間 (JPEG制約対応)
**リスク**: 低 (既存データ活用)
**期待効果**: 品質維持+効率化+15%

**実装内容** (ISX012 JPEG制約対応):
```c
// frame_queue.c 新機能
typedef struct {
    uint64_t timestamp_us;   // get_timestamp_us()活用
    uint32_t frame_size;     // JPEG圧縮後サイズ
    float drop_score;        // 破棄優先度スコア
} frame_metadata_t;

// JPEG品質制御不可対応の選択アルゴリズム
static float calculate_drop_score(frame_metadata_t* frame) {
    uint64_t current_time = get_timestamp_us();
    float age_factor = (current_time - frame->timestamp_us) / 1000000.0; // 秒
    float size_factor = frame->frame_size / 65536.0;  // 平均サイズ比
    return age_factor * 0.7 + size_factor * 0.3;  // 古い+大きい=破棄優先
}
```

---

## Spresense実装制約マトリックス 🆕

### ハードウェア制約対応まとめ

| 制約項目 | Spresense制限 | 制御実装対応策 | 実装優先度 |
|---------|-------------|-------------|-----------|
| **CPU制御** | 6コア×156MHz, 32.736MHz低電力 | 優先度制御で代替 | Phase 10.3 |
| **メモリ** | 1.5MB (実用1.2MB) | 最大1MB制御バッファ | **Phase 10.2** |
| **ISX012** | 1-30fps, JPEG品質制御不可 | サイズベース品質推定 | **Phase 10.1** |
| **GS2200M** | 256KB, 134ms±50ms変動 | 適応制御ゲイン調整 | **Phase 10.1** |
| **NuttX** | 優先度1-255, 1μs精度 | pthread制御活用 | **Phase 10.1** |
| **V4L2** | トリプルバッファ, ioctl制御 | 既存API拡張 | **Phase 10.1** |

### 実装可能性評価結果

**✅ 高実装可能性 (Phase 10.1-10.2)**
- V4L2フレームレート制御: 既存camera_manager.c拡張
- キュー深度制御: 既存frame_queue.c拡張
- NuttX優先度制御: 既存camera_threads.c拡張
- TCP健全性監視: 既存Phase 9.2基盤活用

**⚠️ 制約付き実装可能 (Phase 10.2-10.3)**
- 適応バッファサイズ: RAM 1MB制限内実装
- フレーム品質制御: JPEG制御不可→代替手法
- WiFi品質適応: GS2200M機能制限内実装

**❌ 実装困難/延期 (Phase 11以降)**
- JPEG圧縮率制御: ISX012ハードウェア制限
- CPU周波数動的制御: NuttX API制限
- 高精度WiFi制御: GS2200Mドライバ制限

### 段階的実装戦略

**Phase 10実装方針**: Spresenseの**既存実装基盤を最大活用**し、制約回避策を組み合わせた現実的な制御工学統合を実現。JPEG制御不可等の根本的制約は代替技術で補完し、Phase 11での抜本的改良に向けた基盤を構築。
    uint32_t resize_threshold;
} adaptive_buffer_t;

static void adjust_buffer_size(void);
```

### 1.3 長期実装 (Phase 10.3: 2ヶ月)

#### **Priority 5: 非同期ストリーム処理**
**工数**: 60-80時間
**リスク**: 中-高
**期待効果**: +35%スループット、-20%遅延

**実装内容**:
```rust
// tcp_connection.rs 全面改改
pub struct AsyncFrameReceiver {
    stream: Option<TcpStream>,
    frame_sender: mpsc::Sender<Frame>,
    connection_monitor: ConnectionMonitor,
}

impl AsyncFrameReceiver {
    pub async fn start_reception(&mut self) -> Result<(), Box<dyn Error>>;
}
```

#### **Priority 6: エンドツーエンド制御**
**工数**: 50-70時間
**リスク**: 中-高
**期待効果**: +25%統合最適化、+40%安定性

---

## 2. 技術的要求仕様

### 2.1 機能要件

#### **FR-1: TCP健全性監視 (Priority 1)**
```yaml
要求:
  - 応答時間履歴管理 (最低60秒分)
  - 健全性スコア算出 (0.0-1.0)
  - 予兆検出による自動再接続
  - 設定可能な閾値管理

技術仕様:
  - HISTORY_SIZE: 60 (1秒間隔×60)
  - HEALTH_THRESHOLD: 0.7 (デフォルト)
  - MAX_ACCEPTABLE_TIME: 500ms
  - 重み付け: 応答時間60%、エラー率40%
```

#### **FR-2: 適応的再接続間隔 (Priority 2)**
```yaml
要求:
  - 接続状況に応じた動的間隔調整
  - 連続成功/失敗回数の監視
  - 最小/最大間隔の制限
  - 設定可能なパラメータ

技術仕様:
  - BASE_INTERVAL: 3000ms (デフォルト)
  - MIN_INTERVAL: 1000ms
  - MAX_INTERVAL: 10000ms
  - 成功時拡張率: 2倍
  - 失敗時縮小率: 1/2倍
```

#### **FR-3: 動的優先度調整 (Priority 3)**
```yaml
要求:
  - PID制御によるスレッド優先度自動調整
  - FPS目標値の設定・監視
  - 優先度変更の安全な実行
  - パフォーマンス監視・ログ

技術仕様:
  - TARGET_FPS: 30 (設定可能)
  - PID_KP: 1.2, PID_KI: 0.1, PID_KD: 0.05
  - PRIORITY_RANGE: 90-120
  - UPDATE_INTERVAL: 100ms
```

#### **FR-4: 適応的バッファサイズ (Priority 4)**
```yaml
要求:
  - 使用率に応じた動的サイズ調整
  - メモリ使用量の最適化
  - バッファオーバーフロー防止
  - 使用率履歴の管理

技術仕様:
  - MIN_SIZE: 5, MAX_SIZE: 20
  - RESIZE_THRESHOLD: 80% (拡張)
  - SHRINK_THRESHOLD: 30% (縮小)
  - HISTORY_MINUTES: 60
```

### 2.2 非機能要件

#### **NFR-1: 性能要件**
```yaml
応答性:
  - TCP応答時間: 95ms以下 (現在134ms)
  - スレッド優先度調整: 100ms以内
  - バッファサイズ調整: 1秒以内

スループット:
  - FPS性能: 9.0fps以上 (現在6.74fps)
  - データ転送: +35%向上目標

リソース効率:
  - CPU使用率: +40%効率向上
  - メモリ使用量: +30%効率向上
```

#### **NFR-2: 信頼性要件**
```yaml
可用性:
  - システム稼働率: 99.5%以上
  - 自動復旧時間: 3秒以内
  - 予兆検出精度: 80%以上

安定性:
  - メモリリーク: 0件/24時間
  - 異常終了: 0件/24時間
  - データロス: 0件/1000フレーム
```

#### **NFR-3: 保守性要件**
```yaml
設定管理:
  - パラメータの外部設定対応
  - ランタイム調整機能
  - 設定値の検証機能

監視・診断:
  - ログ出力の充実
  - パフォーマンス監視
  - デバッグ情報の提供

コード品質:
  - 可読性の向上
  - 適切なコメント
  - エラーハンドリングの強化
```

---

## 3. リスク評価と対策

### 3.1 技術リスク

#### **Risk-1: 優先度調整による不安定化**
```yaml
リスク: 中
影響: システム全体の不安定化
対策:
  - 段階的実装とテスト
  - セーフティネット機能
  - ロールバック機能の実装
  - 詳細なログ監視
```

#### **Risk-2: バッファサイズ変更によるメモリ断片化**
```yaml
リスク: 中
影響: メモリ効率の悪化
対策:
  - メモリプール事前確保
  - 断片化検出機能
  - 適切なサイズ制限
  - 定期的なメモリ監視
```

#### **Risk-3: 非同期処理の複雑化**
```yaml
リスク: 高
影響: 実装・デバッグの困難化
対策:
  - 段階的実装
  - 包括的テストケース
  - 適切なログ機能
  - プロトタイプでの検証
```

### 3.2 プロジェクトリスク

#### **Risk-4: 工数見積もり誤差**
```yaml
リスク: 中
影響: スケジュール遅延
対策:
  - 20%バッファの確保
  - 段階的マイルストーン
  - 定期的な進捗レビュー
  - 優先順位の柔軟な調整
```

#### **Risk-5: 既存システムへの影響**
```yaml
リスク: 中
影響: 既存機能の劣化
対策:
  - 十分な回帰テスト
  - A/Bテスト環境
  - ロールバック計画
  - 段階的デプロイ
```

---

## 4. 実装計画・スケジュール

### 4.1 Phase 9.1 (短期: 1-2週間)

#### **Week 1**
```
Day 1-3: TCP健全性監視改善
  - 設計詳細化
  - 実装 (tcp_server.c)
  - 単体テスト

Day 4-5: 適応的再接続間隔
  - 実装
  - 統合テスト
```

#### **Week 2**
```
Day 1-2: 最終統合テスト
Day 3-4: 性能測定・調整
Day 5: ドキュメント整備・レビュー
```

### 4.2 Phase 9.2 (中期: 1-2ヶ月)

#### **Month 1**
```
Week 1-2: 動的優先度調整
  - PID制御アルゴリズム実装
  - スレッド優先度制御機能
  - 安全性検証

Week 3-4: 適応的バッファサイズ
  - バッファ管理機能実装
  - メモリ効率監視
  - 性能測定
```

#### **Month 2**
```
Week 1-2: 統合テスト・最適化
Week 3-4: 性能評価・ドキュメント
```

### 4.3 Phase 9.3 (長期: 2-3ヶ月)

#### **Month 1-2: 非同期ストリーム処理**
```
大規模リファクタリング:
  - 設計・プロトタイピング
  - Rust側実装
  - 非同期処理統合
```

#### **Month 3: エンドツーエンド制御**
```
システム統合:
  - フィードバック制御実装
  - 全体最適化
  - 最終性能評価
```

---

## 5. 品質保証・テスト計画

### 5.1 テスト戦略

#### **単体テスト**
```yaml
対象:
  - 各改善機能の個別テスト
  - アルゴリズム正確性検証
  - エラーハンドリング検証

実施方針:
  - TDD (テスト駆動開発)
  - カバレッジ80%以上
  - 自動化テストスイート
```

#### **統合テスト**
```yaml
対象:
  - システム全体の動作検証
  - 性能要件の確認
  - 負荷テスト

実施方針:
  - CI/CD自動実行
  - 24時間連続稼働テスト
  - 様々な負荷条件での検証
```

#### **性能テスト**
```yaml
測定項目:
  - FPS性能測定
  - TCP応答時間測定
  - CPU・メモリ使用量監視
  - 安定性評価

実施方針:
  - ベンチマーク自動実行
  - 改善前後比較
  - 統計的有意性検証
```

### 5.2 品質基準

#### **コード品質**
```yaml
基準:
  - 関数の最大行数: 50行
  - 循環複雑度: 10以下
  - 適切なコメント率: 30%以上
  - 命名規則の統一

検証方法:
  - 静的解析ツール使用
  - コードレビュー実施
  - ペアプログラミング
```

#### **性能基準**
```yaml
必須要件:
  - FPS: 8.0fps以上 (目標9.2fps)
  - TCP応答: 110ms以下 (目標95ms)
  - CPU効率: +30%以上 (目標+40%)

評価方法:
  - 統計的性能測定
  - 複数回測定の平均
  - 信頼区間の算出
```

---

## 6. 成功判定基準

### 6.1 技術的成功基準

#### **Phase 9.1 成功基準**
```yaml
✅ TCP予兆検出精度: 70%以上
✅ 再接続最適化: -30%不要接続削減
✅ システム安定性: 24時間無停止稼働
✅ 回帰なし: 既存機能性能維持
```

#### **Phase 9.2 成功基準**
```yaml
✅ FPS改善: 8.0fps以上 (現在6.74fps)
✅ CPU効率: +30%以上向上
✅ メモリ効率: +25%以上向上
✅ 優先度制御: 安全な動作確認
```

#### **Phase 9.3 成功基準**
```yaml
✅ 総合性能目標: FPS 9.0fps以上
✅ TCP応答: 100ms以下
✅ システム統合: エンドツーエンド最適化
✅ 安定性: Grade A+ 達成
```

### 6.2 プロジェクト成功基準

#### **品質基準**
```yaml
✅ 不具合率: 1件/1000時間以下
✅ 性能劣化: 0件
✅ メモリリーク: 0件
✅ セキュリティ問題: 0件
```

#### **プロセス基準**
```yaml
✅ スケジュール遵守: 各Phase完了期限内
✅ 品質プロセス: 全テストケース合格
✅ ドキュメント: 適切な技術文書作成
✅ 知識共有: 実装ノウハウの文書化
```

---

## 7. リソース要件・体制

### 7.1 必要リソース

#### **開発環境**
```yaml
ハードウェア:
  - Spresense開発ボード
  - PC開発環境 (Linux/Windows)
  - 性能測定用機材

ソフトウェア:
  - NuttX開発環境
  - Rust開発環境
  - 測定・分析ツール
```

#### **人的リソース**
```yaml
推奨体制:
  - プロジェクトリーダー: 1名
  - 組み込みエンジニア: 1名
  - システムエンジニア: 1名
  - QAエンジニア: 1名 (兼任可)

必要スキル:
  - C言語 (NuttX/組み込み)
  - Rust言語
  - 制御工学基礎知識
  - システム性能解析
```

### 7.2 工数見積もり

#### **総工数見積もり**
```yaml
Phase 9.1 (短期): 35-50時間 (1-2週間)
Phase 9.2 (中期): 75-95時間 (1-2ヶ月)
Phase 9.3 (長期): 110-150時間 (2-3ヶ月)

総計: 220-295時間 (約6-8人月)
```

#### **工数配分**
```yaml
設計・分析: 20%
実装: 50%
テスト・検証: 20%
ドキュメント: 10%
```

---

## 8. 期待成果とROI

### 8.1 技術的成果

#### **システム性能向上**
```yaml
定量的成果:
  FPS: +36%向上 (6.74→9.2fps)
  応答性: +29%向上 (134→95ms)
  安定性: 2段階向上 (A→A+)
  効率性: CPU+40%, Memory+30%

技術的価値:
  - 制御工学理論の実証
  - 組み込みシステム最適化手法
  - エンドツーエンド制御技術
```

#### **ノウハウ・知見獲得**
```yaml
獲得技術:
  - 制御工学的システム設計
  - 性能最適化手法
  - リアルタイムシステム制御
  - 適応制御実装技術

応用可能性:
  - 他IoTシステムへの展開
  - エッジコンピューティング応用
  - 産業用制御システム
```

### 8.2 投資対効果 (ROI)

#### **コスト効率分析**
```yaml
投資コスト: 約6-8人月 (開発工数)
期待リターン:
  - 性能向上による価値: +30-40%
  - 保守性向上: +50%
  - 技術ノウハウ価値: プライスレス
  - 他プロジェクト応用価値: 3倍以上

ROI評価: 非常に高い (3-5倍リターン)
```

---

## 9. 次ステップとアクション

### 9.1 即座実施事項

#### **準備作業**
```yaml
今週実施:
  ✅ 開発環境セットアップ確認
  ✅ ベースライン性能測定
  ✅ テストケース詳細設計
  ✅ 実装詳細設計レビュー
```

#### **Phase 9.1 開始準備**
```yaml
来週開始:
  ✅ TCP健全性監視実装開始
  ✅ 毎日進捗レビュー体制
  ✅ 品質確保プロセス確立
  ✅ リスク監視開始
```

### 9.2 承認・合意事項

#### **承認が必要な項目**
```yaml
技術仕様承認:
  - 各Priority実装スペック
  - 性能目標値設定
  - 品質基準設定

プロジェクト承認:
  - 工数・スケジュール
  - リソース配分
  - リスク受容レベル
```

---

## 🏆 まとめ

### プロジェクト価値
Phase 9コード改善実装は、**制御工学理論を実システムに適用する先進的プロジェクト**として、技術的・教育的・実用的価値を兼ね備えた重要な取り組みです。

### 実装アプローチ
**段階的かつリスクを抑制した実装戦略**により、確実な成果創出と継続的な価値向上を実現します。

### 期待成果
**+30-40%の性能向上と制御工学的システム設計ノウハウの獲得**により、技術革新とエンジニアリング能力向上の両方を達成します。

---

**📋 Status**: 要求・計画整理完了 ✅
**📅 Next**: Phase 9.1 実装開始準備
**🎯 Goal**: 革新的制御工学システムの実現