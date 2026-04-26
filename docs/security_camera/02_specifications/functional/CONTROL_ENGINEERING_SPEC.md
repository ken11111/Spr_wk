# 制御工学統合機能仕様

**バージョン**: 1.1 (Spresense制約統合版)
**日付**: 2026-02-04
**対象システム**: Phase 10 制御工学統合実装
**ベース**: Phase 8-9.2制御工学分析結果 + Spresense仕様調査結果

## 概要

Phase 10で新規導入する制御工学理論統合による自律最適化機能。PID制御、適応制御、予測制御を組み合わせた包括的システム最適化により、従来のスタティック制御からダイナミック制御へのパラダイムシフトを実現する。Phase 11 AI統合への基盤も同時に構築。

### 制御工学統合の技術価値
- **理論的裏付け**: 数学的モデルG₁(s), G₂(s)による設計根拠
- **自律最適化**: PID制御による動的パラメータ調整
- **予測制御**: 機械学習統合による先行制御
- **システム統合**: エンドツーエンド全体最適化

## Spresenseハードウェア制約統合

### ハードウェア仕様制約
- **CPU**: CXD5602 (ARM Cortex-M4F × 6コア @ 156MHz)
- **RAM**: 1.5MB利用可能 (バッファ制御最大1.3MB)
- **カメラ**: ISX012 (1-30 FPS, V4L2制御)
- **WiFi**: GS2200M (256KBバッファ, TCP送信制御)
- **OS**: NuttX (優先度範囲 0-255, リアルタイム制御対応)

### 実装可能性評価結果
```yaml
制御箇所別実装可能性:
  フレームレート制御: ✅ 高 (V4L2 ioctl実装済み)
  キュー深度制御: ✅ 高 (frame_queue.c基盤実装済み)
  TCP送信間隔制御: ✅ 中 (WiFi遅延変動±50ms制約)
  スレッド優先度制御: ✅ 高 (NuttX pthread実装済み)
  メモリ制御: ✅ 高 (動的バッファ割り当て実装済み)
  E2E遅延制御: ✅ 高 (マイクロ秒精度計測可能)
  解像度制御: ✅ 中 (ストリーミング停止200-500ms必要)
  JPEG圧縮制御: ❌ 不可 (ISX012センサー内蔵制御なし)
  WiFi品質適応: ⚠️ 限定 (GS2200M制約)
  CPU周波数制御: ✅ 中 (156MHz⇔32.736MHz切り替え)
```

## 機能要件

### FR-CE-001: PID制御によるフレームレート制御 🔄 Spresense対応版
```
要件ID: FR-CE-001
優先度: 高
内容: V4L2 ioctl経由のカメラフレームレート制御

制御仕様 (Spresense制約適用):
- 目標値: TARGET_FPS = 7.0fps (Phase C実性能基準)
- 制御変数: camera_fps setting (1-30fps範囲, ISX012制約)
- 制御周期: 100ms (NuttX clock_gettime精度内)
- PIDパラメータ: Kp=0.15, Ki=0.02, Kd=0.0 (Phase C調整版)

制御アルゴリズム:
error(t) = TARGET_FPS - measured_fps
integral += error(t) * dt
integral = clamp(integral, -5.0, 5.0)  // 積分飽和防止
output = Kp*error + Ki*integral
new_fps = clamp(current_fps + output, 1.0, 30.0)

実装方法:
camera_set_fps((int)new_fps) → V4L2 VIDIOC_S_PARM ioctl
応答時間: <1ms (ioctl即座変更)
制御精度: 1fps単位 (V4L2ドライバ制限)
```

### FR-CE-002: 適応的バッファサイズ制御 🔄 Spresense対応版
```
要件ID: FR-CE-002
優先度: 高
内容: フレームバッファの動的サイズ調整

適応制御仕様 (Spresense制約適用):
- サイズ範囲: 5-15フレーム (RAM制約: 1.5MB → 最大1MB使用)
- 単位バッファサイズ: 65KB (ISX012 QVGA JPEG)
- 監視期間: 60秒間の使用率履歴
- 拡張閾値: 使用率80%以上
- 縮小閾値: 使用率30%以下
- 調整単位: ±1フレーム (安全性優先)

実装基盤活用:
- frame_queue_allocate_buffers() 拡張
- frame_queue_depth() リアルタイム監視
- pthread_mutex_lock() による安全操作

制御ロジック:
current_depth = frame_queue_depth(g_action_queue)
memory_usage = current_depth * 65536  // bytes
if (avg_usage > 80% && memory_usage < 1048576)  // <1MB
    allocate_additional_buffer(1)
else if (avg_usage < 30% && current_depth > 5)
    deallocate_buffer(1)
```

### FR-CE-003: TCP健全性予兆検出制御 🔄 Spresense対応版
```
要件ID: FR-CE-003
優先度: 必須
内容: GS2200M WiFiモジュール健全性による予防的接続制御

予兆検出仕様 (Spresense制約適用):
- 健全性スコア範囲: 0.0-1.0
- 監視項目: TCP応答時間(134ms基準±50ms)、WiFi信号強度
- 履歴期間: 60秒間 (NuttX clock_gettime精度)
- 予防的再接続閾値: 0.7以下
- 重み付け: 応答時間50%, 信号強度30%, エラー率20%

計算式 (GS2200M特性対応):
response_factor = 1.0 - (avg_response_time / 250.0)  // 250ms上限
wifi_signal_factor = (signal_strength + 100) / 100.0  // RSSI正規化
error_factor = 1.0 - (error_count / total_requests)
health_score = response_factor*0.5 + wifi_signal_factor*0.3 + error_factor*0.2

GS2200M制約考慮:
- WiFiバッファ: 256KB制限監視
- パケット分割対応: 4KB単位
- 応答時間変動: ±50ms許容範囲
```

### FR-CE-004: 適応的再接続間隔制御
```
要件ID: FR-CE-004
優先度: 中
内容: 接続状況に応じた動的再接続間隔調整

適応制御仕様:
- 基本間隔: 3秒
- 間隔範囲: 1-10秒
- 成功時拡張率: 2倍
- 失敗時縮小率: 1/2倍
- 最大連続成功: 10回で最大間隔
- 最大連続失敗: 3回で最小間隔

アルゴリズム:
if (success_count > 10)
    current_interval = min(base_interval * 2, 10000ms)
else if (failure_count > 3)
    current_interval = max(base_interval / 2, 1000ms)
```

### FR-CE-005: インテリジェントフレーム破棄 🔄 Spresense対応版
```
要件ID: FR-CE-005
優先度: 中
内容: ISX012 JPEG圧縮制約下でのフレーム選択最適化

インテリジェント制御仕様 (Spresense制約適用):
- 評価要素: タイムスタンプ、フレームサイズ、キュー深度
- 破棄優先度: 古いフレーム > 大きなフレーム > キュー過多時
- ISX012制約: JPEG品質制御不可 → サイズベース評価
- メモリ制約: RAM 1.5MB範囲内での効率的破棄

選択アルゴリズム (JPEG品質制御なし):
frame_age = current_time - frame->timestamp_us
size_factor = frame->size / AVERAGE_FRAME_SIZE  // 65KB基準
queue_pressure = current_queue_depth / MAX_QUEUE_DEPTH
drop_score = frame_age*0.5 + size_factor*0.3 + queue_pressure*0.2

実装方法:
- get_timestamp_us() 活用 (マイクロ秒精度)
- frame_queue_depth() リアルタイム監視
- frame->size フィールド活用
```

### FR-CE-006: Spresense専用スレッド優先度制御 🆕 新規
```
要件ID: FR-CE-006
優先度: 高
内容: NuttX優先度範囲を活用した動的スレッド制御

NuttX優先度制御仕様:
- 優先度範囲: 1-255 (SCHED_PRIORITY_MIN-MAX)
- カメラスレッド: 110 (高優先度) → 100-120動的調整
- TCPスレッド: 100 (中優先度) → 90-110動的調整
- 制御周期: 100ms (NuttXスケジューラ対応)
- 優先度逆転防止: PTHREAD_PRIO_INHERIT実装済み

実装基盤活用:
- pthread_setschedparam() 使用
- 既存優先度差(110 vs 100)維持
- camera_threads.c優先度設定拡張

制御アルゴリズム:
fps_error = target_fps - current_fps
if (fps_error > 1.0)
    camera_priority = min(110 + 10, 120)  // 高優先化
else if (fps_error < -1.0)
    camera_priority = max(110 - 10, 100)  // 低優先化
pthread_setschedparam(g_camera_thread, SCHED_RR, &param)
```

### FR-CE-007: Spresense電力管理統合制御 🆕 新規
```
要件ID: FR-CE-007
優先度: 中
内容: CXD5602電力管理機能統合による効率最適化

電力管理制御仕様:
- CPU周波数: 156MHz⇔32.736MHz切り替え
- コア制御: 6コア中1-2コア活用 (残りスリープ)
- 低電力モード: バッテリー残量30%以下で自動移行
- 温度制御: 過熱時の周波数ダウンクロック

制御条件:
- 高性能モード: FPS > 10, CPU使用率 > 70%
- 省電力モード: FPS < 5, CPU使用率 < 30%
- 緊急モード: 温度 > 85°C, バッテリー < 10%

実装制約:
- NuttX電力管理API依存
- 周波数変更時のカメラタイミング影響要検証
- Phase 10.3での実装予定 (安定化後)
```

## 技術制約マトリックス

### 実装優先度と制約レベル
| 機能要件 | Spresense対応 | 実装難易度 | Phase 10優先度 |
|---------|-------------|-----------|-------------|
| FR-CE-001 フレームレート制御 | ✅ V4L2対応済み | 低 | **P1 (10.1)** |
| FR-CE-002 バッファサイズ制御 | ✅ RAM制約内実装 | 中 | **P1 (10.2)** |
| FR-CE-003 TCP健全性制御 | ✅ GS2200M対応 | 中 | **P1 (10.1)** |
| FR-CE-004 再接続間隔制御 | ✅ WiFi制約対応 | 低 | P2 (10.2) |
| FR-CE-005 フレーム破棄制御 | ✅ JPEG制約対応 | 中 | P2 (10.2) |
| FR-CE-006 スレッド優先度制御 | ✅ NuttX対応 | 低 | **P1 (10.1)** |
| FR-CE-007 電力管理制御 | ⚠️ API制約 | 高 | P3 (10.3) |

### Spresense制約回避策
```yaml
JPEG圧縮制御不可問題:
  回避策: フレームサイズベース品質推定
  代替手段: 解像度動的変更 (200-500ms遅延許容)

WiFi変動制約問題:
  回避策: 134ms±50ms範囲での適応制御
  代替手段: バッファリング強化による吸収

メモリ制約問題:
  回避策: 最大1MB以内でのバッファ制御
  代替手段: 動的メモリ解放・再割り当て

CPU周波数制御問題:
  回避策: 段階的実装 (Phase 10.3)
  代替手段: スレッド優先度制御での代替
for each frame in buffer:
    if (!frame.is_key_frame)
        motion_score = motion_detector.analyze(frame)
        quality_score = frame.quality_assessment
        drop_score = (1.0 - motion_score) * 0.6 + (1.0 - quality_score) * 0.4
        if (drop_score < lowest_score)
            candidate = frame
```

### FR-CE-006: エンドツーエンドフィードバック制御
```
要件ID: FR-CE-006
優先度: 高
内容: PC側からSpresense側への最適化コマンド送信

フィードバック制御仕様:
- 制御周期: 1秒
- 制御パラメータ: FPS目標、品質レベル、優先度調整
- 通信方式: 制御コマンドパケット
- 応答監視: コマンド実行確認

制御ループ:
measure_performance() -> calculate_control_output() ->
send_control_command() -> monitor_response() -> update_model()
```

## 非機能要件

### NFR-CE-001: 制御系性能要件
```
制御応答性:
- PID制御周期: 100ms以内
- 健全性評価: 1秒以内
- バッファ調整: 1秒以内
- フィードバック制御: 1秒以内

制御精度:
- FPS制御精度: ±5%以内
- 健全性予測精度: 80%以上
- バッファ最適化: 使用率誤差±10%以内
```

### NFR-CE-002: システム安定性要件
```
制御安定性:
- PID制御オーバーシュート: 10%以内
- システム発振: 検出時自動ゲイン調整
- 制御パラメータ収束時間: 5秒以内

異常時動作:
- 制御失敗時: 安全な固定値へフォールバック
- センサ異常時: 代替制御モードへ移行
- 通信断時: ローカル制御継続
```

### NFR-CE-003: リソース効率要件
```
計算負荷:
- 制御アルゴリズムCPU使用率: 5%以下
- メモリ使用量: 追加50MB以下
- 制御データ保存: 1MB以下

リアルタイム性:
- 制御遅延: 50ms以内
- データ処理遅延: 100ms以内
- 制御コマンド配信: 200ms以内
```

## 制御理論実装詳細

### 数学的モデル実装

#### G₁(s): キュー制御系
```c
// キュー制御系の伝達関数実装
typedef struct queue_control_model {
    float K1;          // ゲイン = 1.0
    float tau1_ms;     // 時定数 = 33ms
    float input;       // 入力u₁(t)
    float output;      // 出力y₁(t)
    float state;       // 内部状態
} queue_control_t;

static float update_queue_control(queue_control_t* ctrl, float dt_ms) {
    // G₁(s) = K₁/(τ₁s + 1) の離散化実装
    float alpha = dt_ms / (ctrl->tau1_ms + dt_ms);
    ctrl->state = ctrl->state * (1 - alpha) + ctrl->input * alpha;
    ctrl->output = ctrl->K1 * ctrl->state;
    return ctrl->output;
}
```

#### G₂(s): TCP制御系
```c
// TCP制御系の伝達関数実装（遅延要素付き）
typedef struct tcp_control_model {
    float K2;              // ゲイン = 0.85
    float tau2_ms;         // 時定数 = 134ms
    float theta_ms;        // むだ時間 = 25ms
    float delay_buffer[DELAY_BUFFER_SIZE];  // 遅延バッファ
    int delay_index;       // バッファインデックス
    float input;           // 入力u₂(t)
    float output;          // 出力y₂(t)
    float state;           // 内部状態
} tcp_control_t;

static float update_tcp_control(tcp_control_t* ctrl, float dt_ms) {
    // 遅延要素の実装
    ctrl->delay_buffer[ctrl->delay_index] = ctrl->input;
    ctrl->delay_index = (ctrl->delay_index + 1) % DELAY_BUFFER_SIZE;

    int delay_samples = (int)(ctrl->theta_ms / dt_ms);
    int delayed_index = (ctrl->delay_index - delay_samples + DELAY_BUFFER_SIZE) % DELAY_BUFFER_SIZE;
    float delayed_input = ctrl->delay_buffer[delayed_index];

    // G₂(s) = K₂e^(-θs)/(τ₂s + 1) の離散化実装
    float alpha = dt_ms / (ctrl->tau2_ms + dt_ms);
    ctrl->state = ctrl->state * (1 - alpha) + delayed_input * alpha;
    ctrl->output = ctrl->K2 * ctrl->state;
    return ctrl->output;
}
```

### PID制御実装

#### 汎用PIDコントローラ
```c
typedef struct pid_controller {
    float kp, ki, kd;      // PIDゲイン
    float setpoint;        // 目標値
    float integral;        // 積分項
    float prev_error;      // 前回誤差
    float output_min;      // 出力最小値
    float output_max;      // 出力最大値
    uint32_t last_time_ms; // 前回更新時刻
} pid_controller_t;

static float pid_update(pid_controller_t* pid, float measurement) {
    uint32_t now_ms = get_systime_ms();
    float dt = (now_ms - pid->last_time_ms) / 1000.0f;  // 秒単位
    pid->last_time_ms = now_ms;

    if (dt <= 0.0f) return pid->prev_error;  // 時間経過なし

    float error = pid->setpoint - measurement;
    pid->integral += error * dt;
    float derivative = (error - pid->prev_error) / dt;

    float output = pid->kp * error + pid->ki * pid->integral + pid->kd * derivative;

    // 出力制限
    if (output > pid->output_max) output = pid->output_max;
    if (output < pid->output_min) output = pid->output_min;

    pid->prev_error = error;
    return output;
}
```

### 制御システム統合実装

#### メイン制御ループ
```c
// Phase 9 統合制御システム
typedef struct control_system {
    queue_control_t queue_ctrl;
    tcp_control_t tcp_ctrl;
    pid_controller_t fps_controller;
    pid_controller_t quality_controller;
    tcp_health_monitor_t health_monitor;
    adaptive_buffer_t buffer_ctrl;
} control_system_t;

static void control_system_update(control_system_t* sys) {
    // 1. システム状態測定
    float current_fps = measure_current_fps();
    float tcp_response = measure_tcp_response_time();
    float buffer_usage = measure_buffer_usage();

    // 2. 健全性評価
    float health_score = calculate_health_score(&sys->health_monitor, tcp_response);

    // 3. PID制御計算
    float fps_adjustment = pid_update(&sys->fps_controller, current_fps);
    float quality_adjustment = pid_update(&sys->quality_controller, health_score);

    // 4. 制御コマンド生成
    control_command_t cmd = {
        .priority_adjustment = (int)fps_adjustment,
        .quality_level = (int)quality_adjustment,
        .reconnect_required = (health_score < HEALTH_THRESHOLD)
    };

    // 5. 制御実行
    apply_control_command(&cmd);

    // 6. 適応制御更新
    update_adaptive_buffer(&sys->buffer_ctrl, buffer_usage);
    update_reconnect_interval(&sys->health_monitor, cmd.reconnect_required);
}
```

## テスト要件

### 制御系テスト仕様

#### UT-CE-001: PID制御単体テスト
```yaml
テスト内容:
- ステップ応答テスト: 目標値変更時の応答特性確認
- インパルス応答テスト: 外乱に対する制御応答確認
- 安定性テスト: 制御系の安定性確認
- ゲイン調整テスト: PIDパラメータ最適化確認

合格基準:
- オーバーシュート: 10%以内
- 整定時間: 5秒以内
- 定常偏差: ±5%以内
- 発振: なし
```

#### IT-CE-001: システム統合制御テスト
```yaml
テスト内容:
- エンドツーエンド制御確認
- 制御ループ間相互作用確認
- 負荷変動時の制御応答確認
- 異常時制御動作確認

合格基準:
- 目標性能達成: FPS 9.0fps以上
- 応答時間改善: 100ms以下
- 安定性: 24時間連続稼働
- 異常復旧: 3秒以内
```

## 運用・保守

### 制御パラメータ調整

#### 調整ガイドライン
```yaml
PIDゲイン調整:
1. 比例ゲイン(Kp): 応答速度調整 (推奨: 0.8-1.5)
2. 積分ゲイン(Ki): 定常偏差除去 (推奨: 0.05-0.2)
3. 微分ゲイン(Kd): 安定性向上 (推奨: 0.01-0.1)

調整手順:
1. Kp設定 → ステップ応答確認
2. Ki追加 → 定常偏差確認
3. Kd追加 → 安定性確認
4. 全体最適化 → 性能確認
```

#### 監視・診断

```yaml
制御性能監視:
- FPS制御精度監視: ±5%以内維持確認
- TCP健全性監視: スコア0.7以上維持確認
- バッファ効率監視: 使用率最適化確認

診断情報:
- 制御ログ: パラメータ・制御量履歴
- 性能メトリクス: KPI達成度追跡
- 異常検出: 制御系異常の自動検出
```

## まとめ

Phase 9制御工学統合により、従来の手動調整・固定パラメータシステムから、自律学習・動的最適化システムへの革新的進化を実現。制御理論の実システム適用により、エンジニアリング分野における理論と実践の橋渡しを達成する。

### 期待成果
- **技術的成果**: +30-40%の性能向上と安定性確保
- **学術的成果**: 制御工学理論の実証実装
- **実用的成果**: 自動運用・保守負荷軽減
- **教育的成果**: エンジニアリング教育への貢献

---

**Status**: Phase 9 制御工学統合機能仕様 完成 ✅
**Version**: 1.0
**Next**: 実装フェーズ開始準備