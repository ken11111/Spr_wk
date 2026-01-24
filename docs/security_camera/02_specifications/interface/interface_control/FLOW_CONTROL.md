# フロー制御仕様

**バージョン**: 2.0 (Phase 9.2対応)
**日付**: 2026-01-22
**目的**: バックプレッシャー制御・バッファオーバーフロー防止

## 概要

SpresenseとPC間通信におけるフロー制御機能。送信側・受信側の処理速度差による問題を解決し、安定した通信を実現。Phase 9.2で健全性ベースフロー制御を追加。

## フロー制御アーキテクチャ

### 基本構成
```
[Spresense送信側] → [バッファ] → [転送レイヤー] → [PC受信側]
       ↑              ↓           ↓              ↓
   フロー制御 ← バッファ監視 ← 転送状況監視 ← 処理能力監視
```

### レイヤー別フロー制御

#### アプリケーション層フロー制御
```c
// フレーム送信レート制御
typedef struct {
    uint32_t target_fps;              // 目標FPS
    uint32_t actual_fps;              // 実測FPS
    uint64_t last_frame_timestamp_us; // 最終フレーム送信時刻
    uint32_t frame_interval_us;       // フレーム間隔
    bool     rate_limiting_active;    // レート制限有効
} frame_rate_control_t;

int control_frame_rate(void)
{
    frame_rate_control_t *ctrl = &g_frame_rate_ctrl;
    uint64_t now_us = get_timestamp_us();
    uint64_t elapsed_us = now_us - ctrl->last_frame_timestamp_us;

    if (elapsed_us < ctrl->frame_interval_us) {
        // フレーム間隔未到達 - 送信スキップ
        return 1;  // SKIP
    }

    ctrl->last_frame_timestamp_us = now_us;
    return 0;  // SEND_OK
}
```

#### バッファ層フロー制御 (Phase 1.5最適化)
```c
// トリプルバッファリング (V4L2制限対応)
#define CAMERA_BUFFER_NUM    3  // V4L2ドライバー最大値

typedef struct {
    uint8_t  jpeg_data[MAX_JPEG_SIZE_VGA];
    uint32_t jpeg_size;
    uint64_t timestamp_us;
    uint32_t sequence;
    buffer_state_t state;
} jpeg_buffer_t;

typedef enum {
    BUFFER_STATE_FREE = 0,      // 空きバッファ
    BUFFER_STATE_CAPTURING = 1, // キャプチャ中
    BUFFER_STATE_READY = 2,     // 送信待ち
    BUFFER_STATE_SENDING = 3,   // 送信中
} buffer_state_t;

// バッファプール管理
typedef struct {
    jpeg_buffer_t buffers[CAMERA_BUFFER_NUM];
    uint8_t free_count;         // 空きバッファ数
    uint8_t ready_count;        // 送信待ちバッファ数
    uint8_t capture_index;      // キャプチャ用インデックス
    uint8_t send_index;         // 送信用インデックス
    bool    buffer_full_alert;  // バッファ満杯警告
} buffer_pool_t;

buffer_flow_control_result_t check_buffer_flow_control(void)
{
    buffer_pool_t *pool = &g_buffer_pool;

    // バッファ使用率計算
    float utilization = (float)(CAMERA_BUFFER_NUM - pool->free_count) / CAMERA_BUFFER_NUM;

    if (utilization >= 0.9) {  // 90%以上
        pool->buffer_full_alert = true;
        LOG_WARN("Buffer utilization critical: %.1f%% (free=%d)",
                utilization * 100, pool->free_count);
        return BUFFER_FLOW_DROP_FRAME;  // フレームドロップ
    } else if (utilization >= 0.7) {  // 70%以上
        LOG_INFO("Buffer utilization high: %.1f%%", utilization * 100);
        return BUFFER_FLOW_SLOW_DOWN;  // フレームレート調整
    }

    return BUFFER_FLOW_NORMAL;
}
```

#### 転送層フロー制御 (Phase 9.2健全性ベース) ⭐

##### USB CDC-ACM フロー制御
```c
// USB CDC-ACMは内部でフロー制御自動実行
// アプリケーション層での明示制御は不要
typedef struct {
    bool hardware_flow_control;    // ハードウェアフロー制御
    bool software_flow_control;    // ソフトウェアフロー制御
    uint32_t write_buffer_size;    // 書き込みバッファサイズ
    uint32_t write_timeout_ms;     // 書き込みタイムアウト
} usb_flow_control_t;

static const usb_flow_control_t usb_flow_config = {
    .hardware_flow_control = false,  // CDC-ACMでは通常不使用
    .software_flow_control = false,  // XON/XOFF不使用
    .write_buffer_size = 64 * 1024,  // 64KB
    .write_timeout_ms = 1000,        // 1秒
};
```

##### WiFi TCP フロー制御 (Phase 9.2健全性ベース) ⭐
```c
// TCP健全性ベースフロー制御
typedef struct {
    bool health_based_flow_control;     // 健全性ベース制御有効 ⭐
    uint32_t normal_send_interval_ms;   // 正常時送信間隔
    uint32_t degraded_send_interval_ms; // 劣化時送信間隔
    uint32_t critical_send_interval_ms; // 危険時送信間隔
    float throttling_ratio;             // スロットリング比率
    bool  adaptive_throttling;          // 適応的スロットリング
} tcp_health_flow_control_t;

tcp_flow_control_action_t tcp_health_based_flow_control(void)
{
    tcp_health_monitor_t *health = &g_tcp_health;
    tcp_health_flow_control_t *ctrl = &g_tcp_flow_ctrl;

    if (!ctrl->health_based_flow_control) {
        return TCP_FLOW_NORMAL;  // 健全性ベース制御無効
    }

    // 健全性状況に基づく制御判断
    if (health->degradation_alert) {
        LOG_WARN("TCP health degraded - applying throttling");
        ctrl->throttling_ratio = 0.5;  // 50%スロットリング
        return TCP_FLOW_THROTTLE;
    }

    if (health->consecutive_spikes >= 1) {
        LOG_INFO("TCP minor degradation - light throttling");
        ctrl->throttling_ratio = 0.8;  // 20%スロットリング
        return TCP_FLOW_SLOW_DOWN;
    }

    return TCP_FLOW_NORMAL;
}

int apply_tcp_throttling(float throttling_ratio)
{
    // フレーム送信間隔を延長
    uint32_t normal_interval = 1000 / g_frame_rate_ctrl.target_fps;  // ms
    uint32_t throttled_interval = (uint32_t)(normal_interval / throttling_ratio);

    g_frame_rate_ctrl.frame_interval_us = throttled_interval * 1000;

    LOG_INFO("TCP throttling applied: ratio=%.2f, interval=%d ms",
            throttling_ratio, throttled_interval);
    return 0;
}
```

## バックプレッシャー処理

### 送信側バックプレッシャー検出
```c
typedef enum {
    BACKPRESSURE_NONE = 0,        // バックプレッシャーなし
    BACKPRESSURE_BUFFER_FULL = 1, // バッファ満杯
    BACKPRESSURE_NETWORK_SLOW = 2, // ネットワーク遅延
    BACKPRESSURE_RECEIVER_SLOW = 3, // 受信側処理遅延
    BACKPRESSURE_HEALTH_DEGRADED = 4, // 健全性劣化 ⭐ Phase 9.2
} backpressure_type_t;

backpressure_type_t detect_backpressure(void)
{
    // バッファ満杯検出
    if (g_buffer_pool.free_count == 0) {
        return BACKPRESSURE_BUFFER_FULL;
    }

    // Phase 9.2: TCP健全性劣化検出 ⭐
    if (g_tcp_health.degradation_alert) {
        return BACKPRESSURE_HEALTH_DEGRADED;
    }

    // 送信時間監視によるネットワーク遅延検出
    if (g_tcp_health.moving_avg_ms > 500) {  // 500ms超過
        return BACKPRESSURE_NETWORK_SLOW;
    }

    // PC側処理遅延検出 (メトリクスから推定)
    if (g_last_metrics.packet_processing_ms > 50) {  // 50ms超過
        return BACKPRESSURE_RECEIVER_SLOW;
    }

    return BACKPRESSURE_NONE;
}
```

### バックプレッシャー対応戦略
```c
int handle_backpressure(backpressure_type_t type)
{
    switch (type) {
        case BACKPRESSURE_BUFFER_FULL:
            // 最古フレーム破棄 (oldest-first)
            LOG_WARN("Buffer full - dropping oldest frame");
            return drop_oldest_frame();

        case BACKPRESSURE_NETWORK_SLOW:
            // フレームレート削減 (一時的)
            LOG_WARN("Network slow - reducing frame rate");
            return reduce_frame_rate_temporarily();

        case BACKPRESSURE_RECEIVER_SLOW:
            // JPEG品質低下 (将来実装)
            LOG_WARN("Receiver slow - reducing JPEG quality");
            return reduce_jpeg_quality();  // 未実装

        case BACKPRESSURE_HEALTH_DEGRADED:  // ⭐ Phase 9.2
            // 予防的再接続 or 大幅スロットリング
            LOG_WARN("TCP health degraded - initiating recovery");
            if (g_tcp_health.preventive_reconnect_needed) {
                return initiate_preventive_reconnect();
            } else {
                return apply_aggressive_throttling();
            }

        case BACKPRESSURE_NONE:
        default:
            return 0;  // 対処不要
    }
}

int drop_oldest_frame(void)
{
    buffer_pool_t *pool = &g_buffer_pool;

    // 送信待ちキューから最古フレーム削除
    for (int i = 0; i < CAMERA_BUFFER_NUM; i++) {
        if (pool->buffers[i].state == BUFFER_STATE_READY) {
            // 最古フレーム発見 (timestamp_us最小)
            uint8_t oldest_idx = find_oldest_ready_buffer();
            if (oldest_idx < CAMERA_BUFFER_NUM) {
                pool->buffers[oldest_idx].state = BUFFER_STATE_FREE;
                pool->free_count++;
                pool->ready_count--;
                LOG_INFO("Dropped frame: seq=%d, age=%llu ms",
                        pool->buffers[oldest_idx].sequence,
                        (get_timestamp_us() - pool->buffers[oldest_idx].timestamp_us) / 1000);
                return 0;
            }
        }
    }

    return -1;  // ドロップ対象なし
}
```

## 受信側フロー制御 (PC)

### 受信バッファ管理
```rust
// Rust PC側受信フロー制御
use std::collections::VecDeque;

pub struct ReceiveFlowControl {
    receive_buffer: VecDeque<Vec<u8>>,
    max_buffer_size: usize,
    current_buffer_usage: usize,
    processing_backlog: usize,
    flow_control_active: bool,
}

impl ReceiveFlowControl {
    const MAX_BUFFER_SIZE: usize = 512 * 1024;  // 512KB
    const FLOW_CONTROL_THRESHOLD: f32 = 0.8;    // 80%

    pub fn new() -> Self {
        Self {
            receive_buffer: VecDeque::new(),
            max_buffer_size: Self::MAX_BUFFER_SIZE,
            current_buffer_usage: 0,
            processing_backlog: 0,
            flow_control_active: false,
        }
    }

    pub fn check_flow_control(&mut self) -> FlowControlAction {
        let utilization = self.current_buffer_usage as f32 / self.max_buffer_size as f32;

        if utilization >= Self::FLOW_CONTROL_THRESHOLD {
            self.flow_control_active = true;
            log::warn!("Receive buffer utilization high: {:.1}% - activating flow control",
                      utilization * 100.0);
            FlowControlAction::SlowDown
        } else if utilization >= 0.9 {
            log::error!("Receive buffer critical: {:.1}% - dropping packets",
                       utilization * 100.0);
            FlowControlAction::DropPackets
        } else {
            self.flow_control_active = false;
            FlowControlAction::Normal
        }
    }

    pub fn receive_packet(&mut self, packet_data: Vec<u8>) -> Result<(), FlowControlError> {
        let packet_size = packet_data.len();

        // フロー制御チェック
        match self.check_flow_control() {
            FlowControlAction::DropPackets => {
                log::warn!("Dropping packet due to buffer overflow: size={} bytes", packet_size);
                return Err(FlowControlError::BufferOverflow);
            }
            FlowControlAction::SlowDown => {
                // 受信スロットリング (実装は上位層で)
                log::debug!("Flow control active - slow down recommended");
            }
            FlowControlAction::Normal => {
                // 正常処理
            }
        }

        // バッファに追加
        if self.current_buffer_usage + packet_size <= self.max_buffer_size {
            self.receive_buffer.push_back(packet_data);
            self.current_buffer_usage += packet_size;
            Ok(())
        } else {
            Err(FlowControlError::BufferOverflow)
        }
    }
}

#[derive(Debug)]
pub enum FlowControlAction {
    Normal,
    SlowDown,
    DropPackets,
}

#[derive(Debug)]
pub enum FlowControlError {
    BufferOverflow,
    ProcessingBacklog,
}
```

### 適応的品質制御 (将来実装)
```rust
// 受信側からの品質制御要求
pub struct AdaptiveQualityControl {
    current_bandwidth_usage: u32,    // 現在帯域使用量
    target_bandwidth: u32,           // 目標帯域
    quality_adjustment_active: bool,
    last_quality_command_time: std::time::Instant,
}

impl AdaptiveQualityControl {
    pub fn evaluate_quality_adjustment(&mut self) -> Option<QualityCommand> {
        let utilization = self.current_bandwidth_usage as f32 / self.target_bandwidth as f32;

        if utilization > 1.2 {  // 120%超過
            Some(QualityCommand::ReduceQuality(70))  // 品質70%に低下
        } else if utilization > 1.0 {  // 100%超過
            Some(QualityCommand::ReduceQuality(80))  // 品質80%に低下
        } else if utilization < 0.8 && self.quality_adjustment_active {  // 80%未満で回復
            Some(QualityCommand::RestoreQuality(90))  // 品質90%に復帰
        } else {
            None  // 調整不要
        }
    }
}

#[derive(Debug)]
pub enum QualityCommand {
    ReduceQuality(u8),   // 品質削減 (0-100%)
    RestoreQuality(u8),  // 品質復帰 (0-100%)
}
```

## フロー制御メトリクス

### 監視メトリクス
```c
typedef struct {
    // 基本フロー統計
    uint32_t total_frames_processed;      // 処理総フレーム数
    uint32_t frames_dropped_buffer_full;  // バッファ満杯ドロップ
    uint32_t frames_dropped_flow_control; // フロー制御ドロップ
    float    avg_buffer_utilization;      // 平均バッファ使用率
    uint32_t max_buffer_utilization;      // 最大バッファ使用率

    // Phase 9.2健全性フロー制御 ⭐
    uint32_t health_based_throttling_events; // 健全性ベーススロットリング回数
    uint32_t throttling_duration_total_ms;   // 総スロットリング時間
    float    avg_throttling_ratio;            // 平均スロットリング比率

    // バックプレッシャー統計
    uint32_t backpressure_buffer_events;     // バッファバックプレッシャー
    uint32_t backpressure_network_events;    // ネットワークバックプレッシャー
    uint32_t backpressure_receiver_events;   // 受信側バックプレッシャー
    uint32_t backpressure_health_events;     // 健全性バックプレッシャー ⭐

    // フレームレート制御
    float    target_fps;                      // 目標FPS
    float    actual_fps;                      // 実測FPS
    float    fps_efficiency;                  // FPS効率 (actual/target)
} flow_control_metrics_t;

void update_flow_control_metrics(void)
{
    flow_control_metrics_t *metrics = &g_flow_metrics;
    buffer_pool_t *pool = &g_buffer_pool;

    // バッファ使用率更新
    float current_utilization = (float)(CAMERA_BUFFER_NUM - pool->free_count) / CAMERA_BUFFER_NUM;
    metrics->avg_buffer_utilization =
        (metrics->avg_buffer_utilization * 0.9) + (current_utilization * 0.1);

    if (current_utilization > metrics->max_buffer_utilization) {
        metrics->max_buffer_utilization = current_utilization;
    }

    // FPS効率計算
    if (metrics->target_fps > 0) {
        metrics->fps_efficiency = metrics->actual_fps / metrics->target_fps;
    }
}
```

### ログ・監視出力
```c
void log_flow_control_status(void)
{
    static uint64_t last_log_time_us = 0;
    uint64_t now_us = get_timestamp_us();

    // 5秒間隔でログ出力
    if (now_us - last_log_time_us < 5000000) {  // 5秒
        return;
    }

    flow_control_metrics_t *metrics = &g_flow_metrics;
    buffer_pool_t *pool = &g_buffer_pool;

    LOG_INFO("=== Flow Control Status ===");
    LOG_INFO("Buffer: free=%d, ready=%d, utilization=%.1f%%",
            pool->free_count, pool->ready_count,
            metrics->avg_buffer_utilization * 100);
    LOG_INFO("FPS: target=%.1f, actual=%.1f, efficiency=%.1f%%",
            metrics->target_fps, metrics->actual_fps,
            metrics->fps_efficiency * 100);
    LOG_INFO("Drops: buffer_full=%d, flow_control=%d",
            metrics->frames_dropped_buffer_full,
            metrics->frames_dropped_flow_control);

    // Phase 9.2健全性フロー制御統計 ⭐
    if (metrics->health_based_throttling_events > 0) {
        LOG_INFO("Health throttling: events=%d, total_time=%d ms, avg_ratio=%.2f",
                metrics->health_based_throttling_events,
                metrics->throttling_duration_total_ms,
                metrics->avg_throttling_ratio);
    }

    last_log_time_us = now_us;
}
```

## 設定・調整

### フロー制御パラメータ
```c
// フロー制御設定構造体
typedef struct {
    // バッファ制御設定
    uint8_t  buffer_full_threshold;     // バッファ満杯閾値 (80%)
    uint8_t  buffer_warning_threshold;  // バッファ警告閾値 (70%)
    bool     oldest_first_drop;         // 最古フレーム優先ドロップ

    // フレームレート制御
    uint32_t target_fps;                // 目標FPS
    uint32_t min_fps;                   // 最小FPS (スロットリング時)
    bool     adaptive_fps_enabled;      // 適応的FPS制御

    // Phase 9.2健全性ベース制御 ⭐
    bool     health_based_flow_control; // 健全性ベースフロー制御有効
    float    health_throttling_ratio;   // 健全性劣化時スロットリング比率
    uint32_t health_recovery_delay_ms;  // 健全性回復後の制御解除遅延

    // バックプレッシャー制御
    bool     backpressure_detection_enabled; // バックプレッシャー検出有効
    uint32_t network_slow_threshold_ms;      // ネットワーク遅延閾値
    uint32_t receiver_slow_threshold_ms;     // 受信側遅延閾値
} flow_control_config_t;

static flow_control_config_t flow_config = {
    .buffer_full_threshold = 90,        // 90%
    .buffer_warning_threshold = 70,     // 70%
    .oldest_first_drop = true,
    .target_fps = 11,                   // VGA 11fps
    .min_fps = 5,                       // 最小5fps
    .adaptive_fps_enabled = true,
    // Phase 9.2設定 ⭐
    .health_based_flow_control = true,
    .health_throttling_ratio = 0.5,     // 50%スロットリング
    .health_recovery_delay_ms = 2000,   // 2秒遅延
    .backpressure_detection_enabled = true,
    .network_slow_threshold_ms = 500,
    .receiver_slow_threshold_ms = 50,
};
```

## 将来拡張

### AI ベースフロー制御 (Phase 3.0)
```c
// 機械学習ベース予測フロー制御
typedef struct {
    bool ai_flow_control_enabled;
    float predicted_bandwidth;          // 予測帯域
    float predicted_latency;            // 予測レイテンシ
    uint32_t optimal_fps;               // AI推奨FPS
    uint8_t optimal_quality;            // AI推奨品質
} ai_flow_control_t;
```

### QoS ベース制御
```c
// Quality of Service ベース制御
typedef struct {
    qos_level_t required_qos;           // 要求QoSレベル
    bool priority_frame_marking;       // 優先フレームマーキング
    uint32_t critical_frame_interval;  // 重要フレーム間隔
} qos_flow_control_t;
```

**Phase 9.2健全性監視統合フロー制御による安定通信の実現** ✅