# Phase 11 適応制御システム技術仕様書

> **✗ 撤回 (2026-05-08, Phase 12.4 ユーザー判断)**: 本仕様は **撤回** された。
>
> **撤回理由**: Phase 12 確定方針 (Tier 1 維持 + 家庭用) により、Phase 10 単一入力 PID で十分であり Phase 11 多変数+予測適応制御は過剰機能と判定。
>
> - `enhanced_control.h` は OBSOLETE 注記つきで保持 (将来 Tier 移行時の再評価起点)
> - FMEA B8 (RPN 225 → 1, 解消) と L2.C 図 (Phase 11 = 「✗ 撤回」表示) も連動更新済
> - 詳細: [`../../05_future_actions/phase_planned/Phase12_実施計画書.md`](../../05_future_actions/phase_planned/Phase12_実施計画書.md) §Phase 12.4
>
> 本書は撤回された設計の記録として保持する (Phase 13 で Tier 移行時の参考資料)。

---

## 1. システム概要

### 1.1 目的
Phase 10の単一入力PID制御を拡張し、画角による画像サイズ変動に対応する多変数適応制御システムを実装する。

### 1.2 アーキテクチャ概要
```
Camera → Frame Analysis → Multi-Variable Control → Adaptive PID → Output
                ↓              ↓                    ↑
        Statistics      Prediction            Feedback
```

## 2. 制御理論基盤

### 2.1 多変数制御システム
**状態方程式**:
```
X(k+1) = A·X(k) + B·U(k) + W(k)
Y(k) = C·X(k) + V(k)

where:
X = [queue_depth, avg_frame_size, transmission_time]ᵀ
U = [fps_adjustment, buffer_size_adjustment]ᵀ
W = 外乱（画像内容変動）
V = 観測ノイズ
```

**制御目標**:
- キュー深度安定化: 3.5 ± 0.5フレーム
- フレームサイズ変動対応: ±50%変動耐性
- 伝送時間最適化: <100ms平均

### 2.2 適応制御アルゴリズム
**ゲイン適応則**:
```c
// シーン複雑度に基づく適応
complexity_index = frame_size_variance / avg_frame_size;

if (complexity_index > COMPLEXITY_THRESHOLD) {
    // 複雑シーン: 保守的制御
    Kp_adaptive = Kp_base * 0.8;
    Ki_adaptive = Ki_base * 0.5;
} else {
    // シンプルシーン: 積極的制御
    Kp_adaptive = Kp_base * 1.2;
    Ki_adaptive = Ki_base * 1.0;
}
```

## 3. データ構造設計

### 3.1 フレーム統計構造体
```c
typedef struct frame_statistics_s {
    uint32_t frame_count;                // 総フレーム数
    uint32_t window_size;               // 統計窓サイズ (10)
    uint32_t size_history[10];          // サイズ履歴
    uint32_t avg_size_bytes;            // 平均サイズ
    uint32_t size_variance;             // サイズ分散
    float complexity_index;             // 複雑度指数
    uint8_t history_index;              // 履歴インデックス
    uint64_t last_update_us;            // 最終更新時刻
} frame_statistics_t;
```

### 3.2 多変数制御入力
```c
typedef struct multi_variable_input_s {
    float queue_depth;                  // 正規化キュー深度
    float avg_frame_size_kb;           // 正規化平均フレームサイズ
    float frame_size_variance;         // 正規化サイズ分散
    float transmission_time_ms;        // 正規化伝送時間
    float predicted_queue_load;        // 予測キュー負荷
    float complexity_index;            // シーン複雑度指数
    bool valid;                        // 入力妥当性フラグ
} multi_variable_input_t;
```

### 3.3 適応PIDコントローラ
```c
typedef struct adaptive_pid_controller_s {
    // 基本PID係数
    float base_kp, base_ki, base_kd;

    // 適応PID係数
    float current_kp, current_ki, current_kd;

    // 適応制御パラメータ
    float complexity_threshold;        // 複雑度閾値 (0.5)
    float adaptation_factor;           // 適応係数 (0.8)
    bool adaptation_enabled;           // 適応制御有効フラグ

    // 制御状態
    float integral;                    // 積分項
    float previous_error;              // 前回誤差
    uint64_t last_update_us;          // 最終更新時刻

    // 統計情報
    uint32_t adaptation_count;         // 適応実行回数
    float avg_complexity;              // 平均複雑度
} adaptive_pid_controller_t;
```

## 4. 制御アルゴリズム

### 4.1 制御サイクル (100ms周期)
```c
void execute_enhanced_control_cycle(enhanced_control_system_t *system) {
    // 1. フレーム統計更新
    frame_statistics_update(&system->stats, current_frame_size);

    // 2. 多変数入力構築
    multi_variable_input_t input;
    build_control_input(&input, &system->stats, queue_depth, transmission_time);

    // 3. 予測制御
    float predicted_load = predictive_control_update(&system->predictor, &input);
    input.predicted_queue_load = predicted_load;

    // 4. 適応PID制御
    float complexity = system->stats.complexity_index;
    adaptive_pid_adapt_gains(&system->pid, complexity);

    // 5. 制御出力計算
    float control_error = SETPOINT - weighted_control_input(&input);
    float fps_output = adaptive_pid_update(&system->pid, control_error);

    // 6. バッファ管理
    uint32_t optimal_buffer_size = buffer_manager_calculate_size(
        &system->buffer_mgr, &system->stats);

    // 7. 制御出力適用
    camera_set_fps_runtime((int)(fps_output + 0.5f));
    if (optimal_buffer_size != system->buffer_mgr.current_buffer_size) {
        buffer_manager_resize(&system->buffer_mgr, optimal_buffer_size);
    }

    // 8. システム監視
    if (!system_stability_check(&system->stats, fps_output)) {
        trigger_fallback_to_phase10();
    }
}
```

### 4.2 重み付き制御入力計算
```c
float weighted_control_input(multi_variable_input_t *input) {
    // 重み係数 (チューニング対象)
    const float w_queue = 0.6f;        // キュー深度重み
    const float w_size = 0.2f;         // フレームサイズ重み
    const float w_transmission = 0.1f;  // 伝送時間重み
    const float w_prediction = 0.1f;   // 予測重み

    return w_queue * input->queue_depth +
           w_size * input->avg_frame_size_kb +
           w_transmission * input->transmission_time_ms +
           w_prediction * input->predicted_queue_load;
}
```

### 4.3 予測制御アルゴリズム
```c
float predictive_control_update(predictive_controller_t *predictor,
                               multi_variable_input_t *input) {
    // 線形回帰による次フレームサイズ予測
    float predicted_size = linear_regression_predict(
        predictor->size_history, PREDICTION_WINDOW);

    // 予測サイズからキュー影響推定
    float size_ratio = predicted_size / input->avg_frame_size_kb;
    float predicted_transmission = input->transmission_time_ms * size_ratio;

    // キュー負荷予測
    float predicted_queue_load = (predicted_transmission > TRANSMISSION_THRESHOLD) ?
                                size_ratio * 0.5f : 0.0f;

    // 予測精度更新
    update_prediction_accuracy(predictor, input->avg_frame_size_kb);

    return predicted_queue_load;
}
```

## 5. 性能仕様

### 5.1 制御性能
- **応答時間**: 画像サイズ変動検出から制御適用まで100ms以内
- **安定性**: ±10%以内の出力変動範囲
- **適応精度**: シーン変化検出精度90%以上
- **予測精度**: フレームサイズ予測精度80%以上

### 5.2 資源使用量
- **メモリ使用量**: 追加200KB以下 (1.5MB制限内)
- **CPU使用率**: 追加10%以下
- **制御周期**: 100ms (Phase 10と同等)
- **コンテキストスイッチ**: 最小化設計

### 5.3 信頼性
- **連続動作**: 24時間以上無故障動作
- **フォールバック**: Phase 10制御への100ms以内復帰
- **異常検出**: 制御発散・振動の自動検出
- **メモリリーク**: ゼロリーク保証

## 6. 統合仕様

### 6.1 Phase 10互換性
- 既存制御インターフェース100%保持
- フォールバック時のシームレス移行
- 設定パラメータの継承

### 6.2 既存システム統合
- カメラスレッドとの非同期統合
- フレームキューとの効率的連携
- メトリクス出力の拡張

### 6.3 デバッグ・監視機能
```c
// 制御ログ出力 (5秒周期)
LOG_INFO("=== Phase 11 Enhanced Control ===");
LOG_INFO("Queue: %.1f, Size: %.1fKB, Complexity: %.2f, FPS: %.1f",
         queue_depth, avg_size_kb, complexity_index, fps_output);
LOG_INFO("Adaptation: Kp=%.3f Ki=%.3f, Predictions: %.0f%% accurate",
         current_kp, current_ki, prediction_accuracy * 100);
```

## 7. テスト仕様

### 7.1 ユニットテスト
- フレーム統計計算精度テスト
- 適応ゲイン調整ロジックテスト
- 予測アルゴリズム精度テスト
- バッファサイズ計算テスト

### 7.2 統合テスト
- 制御ループエンドツーエンドテスト
- 画像サイズ変動シミュレーション
- 負荷変動ストレステスト
- フォールバック機能テスト

### 7.3 性能テスト
- 24時間連続動作テスト
- メモリリークテスト
- リアルタイム性能測定
- 互換性確認テスト

---

**仕様承認**:
**作成日**: 2026年2月
**レビュー担当**: システム制御チーム
**承認日**: 2026年2月