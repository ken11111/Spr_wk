# 制御工学統合機能仕様

**バージョン**: 1.0 (Phase 9 新規)
**日付**: 2026-02-03
**対象システム**: Phase 9 制御工学統合実装
**ベース**: Phase 8-9.2制御工学分析結果

## 概要

Phase 9で新規導入する制御工学理論統合による自律最適化機能。PID制御、適応制御、予測制御を組み合わせた包括的システム最適化により、従来のスタティック制御からダイナミック制御へのパラダイムシフトを実現する。

### 制御工学統合の技術価値
- **理論的裏付け**: 数学的モデルG₁(s), G₂(s)による設計根拠
- **自律最適化**: PID制御による動的パラメータ調整
- **予測制御**: 機械学習統合による先行制御
- **システム統合**: エンドツーエンド全体最適化

## 機能要件

### FR-CE-001: PID制御によるスレッド優先度制御
```
要件ID: FR-CE-001
優先度: 高
内容: カメラスレッド優先度のPID制御による自動調整

制御仕様:
- 目標値: TARGET_FPS = 30fps
- 制御変数: camera_thread priority (90-120範囲)
- 制御周期: 100ms
- PIDパラメータ: Kp=1.2, Ki=0.1, Kd=0.05

制御アルゴリズム:
error(t) = TARGET_FPS - current_fps
integral += error(t) * dt
derivative = (error(t) - prev_error) / dt
output = Kp*error + Ki*integral + Kd*derivative
priority_adjustment = clamp(output, -20, +20)
```

### FR-CE-002: 適応的バッファサイズ制御
```
要件ID: FR-CE-002
優先度: 高
内容: フレームバッファの動的サイズ調整

適応制御仕様:
- サイズ範囲: 5-20フレーム
- 監視期間: 60秒間の使用率履歴
- 拡張閾値: 使用率80%以上
- 縮小閾値: 使用率30%以下
- 調整単位: ±1-2フレーム

制御ロジック:
if (avg_usage > 80% && current_size < max_size)
    expand_buffer(current_size + 2)
else if (avg_usage < 30% && current_size > min_size)
    shrink_buffer(current_size - 1)
```

### FR-CE-003: TCP健全性予兆検出制御
```
要件ID: FR-CE-003
優先度: 必須
内容: 健全性スコアによる予防的接続制御

予兆検出仕様:
- 健全性スコア範囲: 0.0-1.0
- 監視項目: TCP応答時間、エラー率
- 履歴期間: 60秒間
- 予防的再接続閾値: 0.7以下
- 重み付け: 応答時間60%, エラー率40%

計算式:
response_factor = 1.0 - (avg_response_time / MAX_ACCEPTABLE_TIME)
error_factor = 1.0 - (error_count / total_requests)
health_score = response_factor * 0.6 + error_factor * 0.4
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

### FR-CE-005: インテリジェントフレーム破棄
```
要件ID: FR-CE-005
優先度: 中
内容: 品質・動きレベルに基づく最適フレーム選択

インテリジェント制御仕様:
- 評価要素: 品質スコア、動きレベル、キーフレーム判定
- 破棄優先度: 非キーフレーム > 低品質 > 低動き
- スコア計算: drop_score = (1-motion)*0.6 + (1-quality)*0.4
- キーフレーム保護: 重要フレームの優先保持

選択アルゴリズム:
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