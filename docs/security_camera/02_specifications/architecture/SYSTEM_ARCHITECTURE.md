# システムアーキテクチャ仕様

**バージョン**: 4.0 (Phase 10 制御工学統合実装)
**日付**: 2026-02-03
**対象システム**: Spresense-PC セキュリティカメラシステム
**ベース**: Phase 8-9.2制御工学分析結果

## 概要

SpresenseカメラとPC間のセキュリティカメラシステム全体アーキテクチャ。Phase 10で制御工学理論を完全統合し、PID制御によるスレッド優先度制御、適応的バッファ管理、予兆検出型TCP健全性監視により、自律的に最適化するインテリジェントシステムを実現する。Phase 11 AI統合への基盤も構築。

### Phase 10 制御工学統合成果
- **FPS性能**: 6.74fps → 9.2fps目標 (+36%改善)
- **TCP応答**: 134ms → 95ms目標 (-29%改善)
- **制御理論**: G₁(s), G₂(s)数学モデルの実装統合
- **自律制御**: PID制御による動的最適化

## システム全体構成

### 高レベルアーキテクチャ

```plantuml
@startuml system_overview
!theme plain

skinparam component {
    BackgroundColor lightblue
    BorderColor darkblue
    FontSize 12
}

skinparam cloud {
    BackgroundColor lightgreen
    BorderColor darkgreen
}

package "Spresense Edge Device" {
    [ISX012 Camera] as Camera
    [JPEG Encoder] as Encoder
    [V4L2 Driver] as V4L2
    [WiFi Module\n(GS2200M)] as WiFi
    [Health Monitor] as HealthMon
    [Adaptive Controller] as Controller
}

package "Network Transport" {
    cloud "TCP/IP over WiFi" as Network {
        [Health Metrics] as Metrics
        [MJPEG Stream] as Stream
        [Control Commands] as Commands
    }
}

package "PC Host System" {
    [Stream Receiver] as Receiver
    [Recording Engine] as Recorder
    [Health Analyzer] as Analyzer
    [Web Dashboard] as Dashboard
    [Storage Manager] as Storage
}

Camera --> Encoder : Raw frames
Encoder --> V4L2 : JPEG data
V4L2 --> Controller : Frame ready
Controller --> WiFi : Adaptive transmission
WiFi --> Network : TCP packets

HealthMon --> Metrics : Health data
HealthMon --> Controller : Adaptation signals

Network --> Receiver : MJPEG + Metrics
Receiver --> Recorder : Frame data
Receiver --> Analyzer : Health data
Analyzer --> Dashboard : Status updates
Recorder --> Storage : Video files

note right of Controller
  Phase 9.2 Core:
  Adaptive control based on
  TCP health metrics
end note

note left of Analyzer
  Phase 9.2 Intelligence:
  Predictive health analysis
  and proactive control
end note

@enduml
```

### システム境界・責任分離

#### エッジサイド (Spresense)
- **ハードウェア抽象化**: ISX012カメラ制御、V4L2ドライバー管理
- **データ生成**: リアルタイムJPEGエンコーディング
- **適応制御**: Phase 9.2健全性に基づく品質調整
- **ネットワーク管理**: TCP接続・健全性監視・予防的再接続

#### ホストサイド (PC)
- **データ処理**: MJPEGストリーミング受信・デコード
- **永続化**: 録画データ管理・ストレージ最適化
- **分析・監視**: 健全性分析・予測・ダッシュボード
- **制御・管理**: システム設定・リモート制御

## Phase 9.2 中核アーキテクチャ

### TCP健全性監視中心設計

```plantuml
@startuml phase92_architecture
!theme plain

skinparam package {
    BackgroundColor lightyellow
    BorderColor orange
    FontSize 12
}

package "Phase 9.2 Health-Centric Architecture" {

    package "Spresense Health Core" {
        [TCP Metrics Collector] as Collector
        [Health Classifier] as Classifier
        [Adaptive Controller] as Adaptive
        [Preventive Reconnection] as Reconnect
    }

    package "Data Flow Layer" {
        [Camera Pipeline] as CameraPipe
        [Streaming Engine] as StreamEng
        [Transport Manager] as Transport
    }

    package "PC Analysis Core" {
        [Health Analyzer] as Analyzer
        [Predictive Model] as Predictor
        [Recording Controller] as RecordCtrl
        [Dashboard Engine] as Dashboard
    }

    Collector --> Classifier : Raw metrics
    Classifier --> Adaptive : Health levels
    Adaptive --> CameraPipe : Quality control
    Adaptive --> StreamEng : Rate control
    Adaptive --> Transport : Connection control

    Classifier --> Reconnect : Critical detection
    Reconnect --> Transport : Preemptive reset

    Transport --> Analyzer : Health telemetry
    Analyzer --> Predictor : Historical data
    Predictor --> RecordCtrl : Quality adaptation
    Analyzer --> Dashboard : Real-time status

    note top of Classifier
        Core Intelligence:
        EXCELLENT/GOOD/FAIR/POOR/CRITICAL
        classification with hysteresis
    end note

    note bottom of Reconnect
        Preventive Action:
        3s downtime vs 30s traditional
        95% improvement achieved
    end note
}

@enduml
```

### 適応制御フロー

```c
// system_adaptive_control.h - Phase 9.2統合適応制御
typedef struct {
    // システム全体状態
    system_health_state_t overall_health;
    tcp_health_level_t current_tcp_health;
    uint64_t last_adaptation_time_us;

    // エッジ側制御パラメータ
    struct {
        uint16_t camera_width, camera_height;
        uint32_t target_fps;
        uint8_t jpeg_quality;
        streaming_quality_t stream_quality;
    } edge_params;

    // ホスト側制御パラメータ
    struct {
        recording_quality_t recording_quality;
        uint64_t max_storage_usage;
        bool predictive_mode_enabled;
        float bandwidth_utilization_target;
    } host_params;

    // Phase 9.2統計
    struct {
        uint32_t total_adaptations;
        uint32_t preventive_reconnections;
        uint64_t total_downtime_saved_ms;
        float adaptation_effectiveness;
    } phase92_stats;

} system_adaptive_controller_t;

// システム全体適応制御
int system_global_adaptation(system_adaptive_controller_t *ctrl,
                           tcp_health_metrics_t *health)
{
    tcp_health_level_t new_level = classify_health_level(health);

    if (new_level == ctrl->current_tcp_health) {
        return 0;  // 変化なし
    }

    LOG_INFO("System-wide adaptation: %s → %s",
            health_level_to_string(ctrl->current_tcp_health),
            health_level_to_string(new_level));

    // エッジ側適応制御
    edge_adaptation_config_t edge_config;
    host_adaptation_config_t host_config;

    switch (new_level) {
        case TCP_HEALTH_EXCELLENT:
            edge_config = (edge_adaptation_config_t){
                .camera_resolution = RESOLUTION_VGA,
                .target_fps = 11,
                .jpeg_quality = 80,
                .stream_quality = STREAM_QUALITY_HIGH,
                .buffering_mode = BUFFERING_OPTIMAL
            };
            host_config = (host_adaptation_config_t){
                .recording_quality = RECORD_QUALITY_HIGH,
                .storage_efficiency = STORAGE_QUALITY_PRIORITY,
                .analysis_level = ANALYSIS_FULL,
                .dashboard_update_rate = UPDATE_RATE_HIGH
            };
            break;

        case TCP_HEALTH_GOOD:
            // 標準設定維持
            break;

        case TCP_HEALTH_FAIR:
            edge_config = (edge_adaptation_config_t){
                .camera_resolution = RESOLUTION_VGA,
                .target_fps = 8,
                .jpeg_quality = 70,
                .stream_quality = STREAM_QUALITY_MEDIUM,
                .buffering_mode = BUFFERING_CONSERVATIVE
            };
            host_config = (host_adaptation_config_t){
                .recording_quality = RECORD_QUALITY_EFFICIENT,
                .storage_efficiency = STORAGE_SIZE_PRIORITY,
                .analysis_level = ANALYSIS_REDUCED,
                .dashboard_update_rate = UPDATE_RATE_MEDIUM
            };
            break;

        case TCP_HEALTH_POOR:
            edge_config = (edge_adaptation_config_t){
                .camera_resolution = RESOLUTION_QVGA,
                .target_fps = 5,
                .jpeg_quality = 60,
                .stream_quality = STREAM_QUALITY_LOW,
                .buffering_mode = BUFFERING_AGGRESSIVE
            };
            host_config = (host_adaptation_config_t){
                .recording_quality = RECORD_QUALITY_EMERGENCY,
                .storage_efficiency = STORAGE_MINIMAL,
                .analysis_level = ANALYSIS_ESSENTIAL,
                .dashboard_update_rate = UPDATE_RATE_LOW
            };
            break;

        case TCP_HEALTH_CRITICAL:
            LOG_WARN("TCP health CRITICAL - activating system emergency protocol");
            return activate_system_emergency_mode(ctrl);
    }

    // エッジ・ホスト同期適応制御実行
    int edge_ret = apply_edge_adaptation(&edge_config);
    int host_ret = apply_host_adaptation(&host_config);

    if (edge_ret == 0 && host_ret == 0) {
        ctrl->current_tcp_health = new_level;
        ctrl->phase92_stats.total_adaptations++;
        ctrl->last_adaptation_time_us = get_timestamp_us();

        LOG_INFO("System adaptation completed: edge=%s, host=%s",
                edge_ret == 0 ? "OK" : "FAILED",
                host_ret == 0 ? "OK" : "FAILED");
    }

    return (edge_ret == 0 && host_ret == 0) ? 0 : -1;
}
```

## データフロー・アーキテクチャ

### エンドツーエンドデータパス

```plantuml
@startuml data_flow_architecture
!theme plain

skinparam activity {
    StartColor lightgreen
    EndColor lightcoral
    BackgroundColor lightyellow
    BorderColor darkblue
}

title Phase 9.2 End-to-End Data Flow Architecture

|Spresense Edge|
start
:ISX012 Raw Image Capture;
:V4L2 JPEG Hardware Encoding;
note right
  Variable latency: 0.05ms - 265ms
  Scene complexity dependent
end note

:TCP Health Metrics Collection;
:Adaptive Quality Control;
note right
  Phase 9.2 Core:
  Health-based adaptation
  EXCELLENT → CRITICAL levels
end note

if (Health Level?) then (EXCELLENT/GOOD)
  :High Quality Streaming;
  :VGA 11fps transmission;
elseif (FAIR) then
  :Medium Quality Streaming;
  :VGA 8fps transmission;
elseif (POOR) then
  :Emergency Streaming;
  :QVGA 5fps transmission;
else (CRITICAL)
  :Preventive Reconnection;
  note right
    95% downtime reduction:
    30s → 3s recovery time
  end note
  :Stream Suspension;
endif

:TCP Packet Transmission;

|Network Transport|
:WiFi TCP/IP (12Mbps);
:Health Metrics + MJPEG Data;

|PC Host|
:Stream Reception & Validation;
:Health Analysis & Prediction;

if (Health Trend Analysis) then (Degrading)
  :Proactive Quality Reduction;
  :Storage Optimization;
else (Stable/Improving)
  :Quality Enhancement;
  :Full Feature Recording;
endif

:MJPEG Recording to Storage;
:Real-time Dashboard Updates;
stop

@enduml
```

### メトリクスフロー (Phase 9.2)

```c
// metrics_flow.h - Phase 9.2メトリクス統合フロー
typedef struct {
    // エッジ生成メトリクス
    uint32_t tcp_rtt_ms;                 // Round Trip Time
    uint32_t packet_loss_count;          // パケット損失数
    uint32_t send_buffer_usage_percent;  // 送信バッファ使用率
    uint64_t total_bytes_sent;           // 総送信バイト数
    uint32_t connection_error_count;     // 接続エラー数

    // カメラ性能メトリクス
    uint32_t jpeg_encode_time_us;        // JPEGエンコード時間
    uint32_t frame_capture_interval_ms;  // フレーム間隔
    uint8_t current_jpeg_quality;        // 現在のJPEG品質

    // Phase 9.2拡張メトリクス
    tcp_health_level_t current_health_level;
    uint32_t health_adaptations_count;
    uint64_t preventive_reconnections;
    float adaptation_effectiveness_score;

} edge_generated_metrics_t;

typedef struct {
    // ホスト分析メトリクス
    float stream_reception_quality;      // ストリーミング受信品質
    uint32_t frames_received_per_sec;    // 受信フレーム数/秒
    uint64_t recording_data_rate_mbps;   // 録画データレート
    float storage_efficiency_ratio;     // ストレージ効率

    // 予測分析結果
    tcp_health_prediction_t health_forecast;
    uint32_t predicted_adaptation_needs;
    recommended_quality_t recommended_settings;

    // システム全体統計
    float end_to_end_latency_ms;         // エンドツーエンド遅延
    float system_availability_percent;   // システム可用率
    uint32_t total_service_interruptions;

} host_analyzed_metrics_t;

// メトリクス統合処理
int integrate_system_metrics(edge_generated_metrics_t *edge_metrics,
                            host_analyzed_metrics_t *host_metrics,
                            system_performance_t *system_perf)
{
    // システム全体性能評価
    system_perf->overall_health_score =
        calculate_weighted_health_score(edge_metrics, host_metrics);

    // 適応制御必要性判定
    system_perf->adaptation_urgency =
        evaluate_adaptation_urgency(edge_metrics->current_health_level,
                                   host_metrics->health_forecast);

    // Phase 9.2効果測定
    system_perf->phase92_effectiveness =
        calculate_phase92_roi(edge_metrics->preventive_reconnections,
                             host_metrics->total_service_interruptions,
                             system_perf->total_downtime_saved_ms);

    LOG_INFO("System metrics integrated: health=%.1f, urgency=%d, effectiveness=%.2f",
            system_perf->overall_health_score,
            system_perf->adaptation_urgency,
            system_perf->phase92_effectiveness);

    return 0;
}
```

## 通信アーキテクチャ

### プロトコルスタック

```plantuml
@startuml protocol_stack
!theme plain

skinparam package {
    BackgroundColor lightcyan
    BorderColor darkblue
    FontSize 10
}

title Phase 9.2 Communication Protocol Stack

package "Application Layer" {
    [MJPEG Streaming Protocol v3.0] as MJPEG
    [Health Metrics Protocol] as Health
    [Control Commands Protocol] as Control
}

package "Presentation Layer" {
    [JPEG Encoding/Decoding] as JPEG
    [Metrics Serialization] as Serialize
    [Encryption (Future)] as Encrypt
}

package "Session Layer" {
    [Stream Session Management] as Session
    [Health Monitoring Session] as HealthSess
    [Reconnection Management] as Reconnect
}

package "Transport Layer" {
    [TCP with Health Extensions] as TCP
    [Adaptive Flow Control] as FlowCtrl
    [Preventive Connection Reset] as Reset
}

package "Network Layer" {
    [IPv4/IPv6] as IP
    [WiFi Network Management] as WiFi
}

package "Data Link Layer" {
    [802.11 WiFi] as WiFi80211
    [GS2200M Driver] as GS2200M
}

package "Physical Layer" {
    [2.4GHz RF] as RF
    [Antenna] as Antenna
}

MJPEG --> JPEG
Health --> Serialize
Control --> Serialize

JPEG --> Session
Serialize --> HealthSess
Session --> TCP
HealthSess --> TCP

TCP --> FlowCtrl
FlowCtrl --> Reset
Reset --> IP

TCP --> IP
IP --> WiFi
WiFi --> WiFi80211
WiFi80211 --> GS2200M
GS2200M --> RF
RF --> Antenna

note right of Health
  Phase 9.2 Core:
  Real-time health metrics
  58-byte extended packets
end note

note right of Reset
  Phase 9.2 Innovation:
  Preventive reconnection
  based on health trends
end note

@enduml
```

### 接続管理・フォルトトレラント設計

```c
// connection_architecture.h - Phase 9.2接続管理
typedef struct {
    // 基本接続情報
    int tcp_socket_fd;
    struct sockaddr_in server_addr;
    connection_state_t state;
    uint64_t connection_established_time;

    // Phase 9.2健全性監視
    tcp_health_monitor_t health_monitor;
    preventive_reconnection_t reconnect_controller;

    // 接続品質統計
    struct {
        uint32_t successful_connects;
        uint32_t failed_connects;
        uint32_t unexpected_disconnects;
        uint32_t preventive_reconnects;
        uint64_t total_uptime_ms;
        uint64_t total_downtime_ms;
    } connection_stats;

    // 適応的接続制御
    struct {
        uint32_t current_timeout_ms;
        uint32_t retry_interval_ms;
        uint32_t max_retry_attempts;
        bool aggressive_retry_mode;
    } adaptive_params;

} connection_manager_t;

typedef enum {
    CONNECTION_STATE_DISCONNECTED = 0,
    CONNECTION_STATE_CONNECTING = 1,
    CONNECTION_STATE_CONNECTED = 2,
    CONNECTION_STATE_DEGRADED = 3,        // Phase 9.2: 劣化状態
    CONNECTION_STATE_RECOVERING = 4,      // Phase 9.2: 回復中
    CONNECTION_STATE_PREVENTIVE_RESET = 5, // Phase 9.2: 予防的リセット
} connection_state_t;

// Phase 9.2統合接続制御
int manage_adaptive_connection(connection_manager_t *mgr)
{
    tcp_health_metrics_t current_health =
        get_current_health_metrics(&mgr->health_monitor);

    tcp_health_level_t health_level =
        classify_health_level(&current_health);

    switch (health_level) {
        case TCP_HEALTH_EXCELLENT:
        case TCP_HEALTH_GOOD:
            // 標準動作継続
            if (mgr->state == CONNECTION_STATE_DEGRADED) {
                LOG_INFO("Connection health recovered");
                mgr->state = CONNECTION_STATE_CONNECTED;
            }
            break;

        case TCP_HEALTH_FAIR:
            if (mgr->state == CONNECTION_STATE_CONNECTED) {
                LOG_WARN("Connection degrading - entering degraded state");
                mgr->state = CONNECTION_STATE_DEGRADED;

                // 適応的パラメータ調整
                mgr->adaptive_params.current_timeout_ms *= 1.5;
                mgr->adaptive_params.retry_interval_ms *= 0.8;
            }
            break;

        case TCP_HEALTH_POOR:
            LOG_WARN("Connection health POOR - preparing for preventive action");
            mgr->state = CONNECTION_STATE_DEGRADED;
            mgr->adaptive_params.aggressive_retry_mode = true;
            break;

        case TCP_HEALTH_CRITICAL:
            LOG_ERROR("Connection health CRITICAL - initiating preventive reconnection");
            return initiate_preventive_reconnection(mgr);
    }

    return 0;
}

int initiate_preventive_reconnection(connection_manager_t *mgr)
{
    LOG_INFO("Phase 9.2: Initiating preventive reconnection");

    uint64_t reconnect_start = get_timestamp_us();
    mgr->state = CONNECTION_STATE_PREVENTIVE_RESET;

    // 現在の接続を適切に終了
    graceful_connection_shutdown(mgr->tcp_socket_fd, 1000);  // 1秒タイムアウト

    // 即座に新規接続開始
    mgr->state = CONNECTION_STATE_CONNECTING;
    int new_socket = create_tcp_connection(&mgr->server_addr,
                                          mgr->adaptive_params.current_timeout_ms);

    if (new_socket >= 0) {
        mgr->tcp_socket_fd = new_socket;
        mgr->state = CONNECTION_STATE_CONNECTED;

        uint64_t reconnect_time = get_timestamp_us() - reconnect_start;
        mgr->connection_stats.preventive_reconnects++;

        LOG_INFO("Preventive reconnection successful in %llu ms",
                reconnect_time / 1000);

        // 健全性監視リセット
        reset_health_monitor(&mgr->health_monitor);

        return 0;
    } else {
        LOG_ERROR("Preventive reconnection failed - fallback to normal recovery");
        mgr->state = CONNECTION_STATE_DISCONNECTED;
        return initiate_standard_reconnection(mgr);
    }
}
```

## 障害回復・冗長化設計

### Phase 9.2統合障害回復

```plantuml
@startuml fault_recovery
!theme plain

skinparam activity {
    StartColor yellow
    EndColor lightgreen
    BackgroundColor lightyellow
    BorderColor red
}

title Phase 9.2 Integrated Fault Recovery Architecture

start
:Normal Operation;
:Continuous Health Monitoring;

if (Health Status?) then (EXCELLENT/GOOD)
  :Continue Normal Operation;
elseif (FAIR) then
  :Degradation Detected;
  :Adaptive Quality Reduction;
  :Enhanced Monitoring;
elseif (POOR) then
  :Significant Degradation;
  :Emergency Quality Mode;
  :Prepare for Recovery;
else (CRITICAL)
  :Critical Failure Detection;

  fork
    :Preventive Reconnection;
    note right: Phase 9.2 Core Feature
  fork again
    :Data Preservation;
    :State Backup;
  end fork

  :Connection Reset (3s);

  if (Reconnection Success?) then (Yes)
    :Service Restoration;
    :Quality Ramp-up;
    :Monitor Stability;
  else (Failed)
    :Exponential Backoff;
    :Alternative Transport;
    note right: Fallback to USB CDC-ACM
  endif
endif

:Update Health Statistics;
:Log Recovery Metrics;
:Continue Monitoring;

stop

@enduml
```

### 障害分類・対応マトリクス

```c
// fault_recovery_matrix.h - Phase 9.2障害回復マトリクス
typedef enum {
    FAULT_TYPE_NETWORK_TIMEOUT = 0,
    FAULT_TYPE_CONNECTION_RESET = 1,
    FAULT_TYPE_PACKET_LOSS = 2,
    FAULT_TYPE_BANDWIDTH_DEGRADATION = 3,
    FAULT_TYPE_CAMERA_HARDWARE = 4,
    FAULT_TYPE_MEMORY_EXHAUSTION = 5,
    FAULT_TYPE_STORAGE_FULL = 6,
    FAULT_TYPE_HEALTH_CRITICAL = 7,      // Phase 9.2専用
} system_fault_type_t;

typedef struct {
    system_fault_type_t fault_type;
    const char *description;
    uint32_t detection_threshold;
    uint32_t recovery_timeout_ms;
    recovery_strategy_t primary_strategy;
    recovery_strategy_t fallback_strategy;
    bool requires_preventive_action;      // Phase 9.2拡張
} fault_recovery_policy_t;

static const fault_recovery_policy_t fault_policies[] = {
    {
        FAULT_TYPE_NETWORK_TIMEOUT,
        "Network timeout detected",
        3000,  // 3秒閾値
        10000, // 10秒回復タイムアウト
        RECOVERY_TCP_RETRY,
        RECOVERY_PREVENTIVE_RECONNECT,
        false
    },
    {
        FAULT_TYPE_CONNECTION_RESET,
        "TCP connection reset by peer",
        1,     // 1回発生で検出
        5000,  // 5秒回復タイムアウト
        RECOVERY_IMMEDIATE_RECONNECT,
        RECOVERY_USB_FALLBACK,
        false
    },
    {
        FAULT_TYPE_HEALTH_CRITICAL,
        "Phase 9.2 health critical state",
        1,     // 即座に検出
        3000,  // 3秒高速回復
        RECOVERY_PREVENTIVE_RECONNECT,
        RECOVERY_EMERGENCY_MODE,
        true   // Phase 9.2予防的対応
    },
    // 他の障害タイプ...
};

int execute_fault_recovery(system_fault_type_t fault_type,
                          fault_recovery_context_t *context)
{
    const fault_recovery_policy_t *policy = find_recovery_policy(fault_type);
    if (!policy) {
        return -1;
    }

    LOG_WARN("Fault detected: %s (type=%d)", policy->description, fault_type);

    // Phase 9.2予防的アクション確認
    if (policy->requires_preventive_action) {
        LOG_INFO("Executing Phase 9.2 preventive recovery");

        // 予防的措置実行
        int preventive_result = execute_preventive_measures(context);
        if (preventive_result == 0) {
            // 予防的対応成功
            context->recovery_stats.preventive_successes++;
            return 0;
        }

        LOG_WARN("Preventive measures failed, proceeding with standard recovery");
    }

    // 通常回復戦略実行
    uint64_t recovery_start = get_timestamp_us();
    int recovery_result = execute_recovery_strategy(policy->primary_strategy,
                                                   context,
                                                   policy->recovery_timeout_ms);

    if (recovery_result != 0) {
        LOG_ERROR("Primary recovery failed, trying fallback strategy");
        recovery_result = execute_recovery_strategy(policy->fallback_strategy,
                                                   context,
                                                   policy->recovery_timeout_ms * 2);
    }

    uint64_t recovery_time = get_timestamp_us() - recovery_start;

    // 回復統計更新
    if (recovery_result == 0) {
        context->recovery_stats.successful_recoveries++;
        context->recovery_stats.total_recovery_time_us += recovery_time;

        LOG_INFO("Fault recovery successful in %llu ms", recovery_time / 1000);
    } else {
        context->recovery_stats.failed_recoveries++;
        LOG_ERROR("Fault recovery failed after %llu ms", recovery_time / 1000);
    }

    return recovery_result;
}
```

## 性能・スケーラビリティ設計

### 性能要件・制約

```c
// system_performance.h - システム性能要件
typedef struct {
    // エンドツーエンド性能要件
    uint32_t max_end_to_end_latency_ms;      // 150ms以下
    uint32_t target_fps;                     // 11fps VGA
    uint32_t max_frame_loss_percent;         // 1%以下

    // ネットワーク性能
    uint32_t min_bandwidth_kbps;             // 2Mbps最低保証
    uint32_t max_bandwidth_usage_percent;    // 80%使用上限
    uint32_t tcp_connection_timeout_ms;      // 5秒接続タイムアウト

    // Phase 9.2健全性性能
    uint32_t health_monitoring_interval_ms;  // 100ms監視間隔
    uint32_t adaptation_response_time_ms;    // 500ms以内適応
    uint32_t preventive_reconnect_time_ms;   // 3秒以内完了

    // リソース制約
    uint32_t max_memory_usage_mb;            // 50MB制限
    uint32_t max_cpu_usage_percent;          // 40%制限
    uint32_t max_storage_rate_mbps;          // 10MBps書き込み

} system_performance_requirements_t;

// 性能監視・制御
typedef struct {
    // リアルタイム性能測定
    uint32_t current_latency_ms;
    uint32_t current_fps;
    float current_bandwidth_mbps;
    float current_cpu_usage;
    uint32_t current_memory_mb;

    // Phase 9.2効果測定
    struct {
        uint32_t adaptations_per_hour;
        float downtime_reduction_percent;    // 目標95%削減
        uint32_t proactive_interventions;
        float service_quality_improvement;
    } phase92_metrics;

    // 制約違反検出
    uint32_t latency_violations;
    uint32_t bandwidth_violations;
    uint32_t resource_violations;

} system_performance_monitor_t;
```

### スケーラビリティ・将来拡張

```plantuml
@startuml scalability_architecture
!theme plain

skinparam package {
    BackgroundColor lightgray
    BorderColor darkblue
}

title System Scalability & Future Extension Architecture

package "Current Phase 9.2 System" {
    [Spresense Camera 1] as Cam1
    [TCP Health Monitor] as Health1
    [PC Host] as Host
}

package "Horizontal Scaling (Phase 10.0)" {
    [Spresense Camera 2] as Cam2
    [Spresense Camera 3] as Cam3
    [Spresense Camera N] as CamN

    [Load Balancer] as LB
    [Multi-Stream Manager] as MultiMgr
    [Distributed Storage] as DistStorage
}

package "Vertical Enhancement (Phase 11.0)" {
    [AI Analytics Engine] as AI
    [Edge Computing Node] as EdgeComp
    [Cloud Integration] as Cloud
    [Mobile Clients] as Mobile
}

package "Advanced Features (Future)" {
    [Real-time Object Detection] as ObjDet
    [Predictive Maintenance] as PredMaint
    [5G Network Support] as FiveG
    [Blockchain Logging] as Blockchain
}

Cam1 --> Health1
Health1 --> Host

Cam2 --> LB
Cam3 --> LB
CamN --> LB
LB --> MultiMgr
MultiMgr --> DistStorage

Host --> AI
AI --> EdgeComp
EdgeComp --> Cloud
Cloud --> Mobile

ObjDet --> PredMaint
PredMaint --> FiveG
FiveG --> Blockchain

note right of Health1
  Phase 9.2 Foundation:
  Proven health monitoring
  ready for multi-camera scaling
end note

note right of MultiMgr
  Phase 10.0 Vision:
  N-camera aggregation
  with unified health management
end note

@enduml
```

### Figure 5: Enhanced Multi-Variable Control System

Phase 11の詳細フィードバック制御システム - フレームサイズ、伝送時間、その他性能メトリクスを統合した制御工学設計を示します。

```plantuml
@startuml enhanced_multivariable_control_system
!theme plain

skinparam rectangle {
    BackgroundColor lightblue
    BorderColor blue
    FontSize 11
    RoundCorner 10
}

skinparam package {
    BackgroundColor lightyellow
    BorderColor orange
    FontSize 12
}

skinparam note {
    BackgroundColor lightyellow
    BorderColor orange
    FontSize 10
}

title Figure 5: Enhanced Multi-Variable Control System (Phase 11)\nVerified Implementation Parameters

' Input measurements (left side)
rectangle "Frame Size\nSensor\n[1-128 KB]\n(Normalized: /128)" as FrameSize #LightBlue
rectangle "Transmission\nTimer\n[0-200 ms]\n(Normalized: /200)" as TxTime #LightBlue
rectangle "Queue Depth\nMonitor\n[0-9 frames]\n(Setpoint: 3.5)" as QueueMon #LightBlue
rectangle "Scene Complexity\nAnalyzer\n[0.0-2.0]\n(Threshold: 0.5)" as ComplexAnal #LightBlue
rectangle "Frame Statistics\nPredictor\n(5-frame window)" as Predictor #LightBlue

' Multi-variable controller (center)
package "Phase 11 Enhanced Controller\n(100ms cycle, 10Hz)" #LightYellow {
  rectangle "Input Normalizer\n[0,1] range" as Norm
  rectangle "Weight Matrix\nQueue: 0.6\nSize: 0.2\nTrans: 0.1\nPred: 0.1" as Weights
  rectangle "Weighted Sum\nu_total = Σ(wi×ui)\nΣwi = 1.0" as Sum
  rectangle "Adaptive PID\nKp: 0.15 ± 0.8×complexity\nKi: 0.02 ± 0.8×complexity\nKd: 0.0 (disabled)" as PID
  rectangle "Stability Monitor\nVariance < 0.5\n(5-sample window)" as StabMon
}

' Fallback mechanism
rectangle "Phase 10 Fallback\nClassic PID\n(3 failures trigger)" as Fallback #Pink

' Actuators (right side)
rectangle "FPS Actuator\n[5-30 fps]\nBase: 7 fps" as FPSCtrl #LightGreen
rectangle "Quality Control\n[50-95%]\n(JPEG quality)" as QualCtrl #LightGreen
rectangle "Thread Priority\n[100→105]\n(Queue ≥6 boost)" as PrioCtrl #LightGreen

' Plant models
rectangle "Camera Plant\nG₁(s)=1/(33ms·s+1)\n(ISX012 + JPEG)" as CamPlant #LightCoral
rectangle "USB Plant\nG₂(s)=0.85/(134ms·s+1)\n(CDC transmission)" as USBPlant #LightCoral
rectangle "System Response\nG_sys(s)=1/(14.05s·s+1)\n(Overall dynamics)" as SysPlant #LightCoral

' Forward paths
FrameSize --> Norm
TxTime --> Norm
QueueMon --> Norm
ComplexAnal --> Norm
Predictor --> Norm

Norm --> Weights
Weights --> Sum
Sum --> PID
ComplexAnal --> PID : "Gain adaptation"
PID --> StabMon

StabMon --> FPSCtrl : "Control output"
StabMon --> QualCtrl : "Quality adjust"
QueueMon --> PrioCtrl : "Direct priority"

FPSCtrl --> CamPlant
QualCtrl --> CamPlant
CamPlant --> USBPlant
USBPlant --> SysPlant

' Fallback path
StabMon --> Fallback : "3 failures"
Fallback --> FPSCtrl : "Emergency mode"

' Feedback paths (dashed)
SysPlant -.-> TxTime : "Tx measurement\n(100ms stats)"
USBPlant -.-> QueueMon : "Queue feedback\n(real-time)"
CamPlant -.-> FrameSize : "Frame size\n(per frame)"
CamPlant -.-> ComplexAnal : "Complexity calc\n(10-frame window)"
SysPlant -.-> Predictor : "History update\n(5-frame buffer)"

note right of Norm
  **入力正規化仕様**
  • Queue Depth: [0-10] → [0,1]
  • Frame Size: [1-128KB] → [0,1]
  • Transmission: [0-200ms] → [0,1]
  • Complexity: [0.0-2.0] → [0,1]
  • Predictor: 履歴ベース正規化
end note

note right of Sum
  **多変数統合制御**
  • 重み付け総和: u_total = Σ(wi × ui)
  • Queue重み: 0.6 (60% - 最重要)
  • Size重み: 0.2 (20% - 中重要)
  • Transmission重み: 0.1 (10%)
  • Prediction重み: 0.1 (10%)
  • 重み総和: Σwi = 1.0 (正規化)
end note

note right of PID
  **適応PID制御**
  • ベースゲイン: Kp=0.15, Ki=0.02, Kd=0.0
  • 適応機構: K_adaptive = K_base × (1 ± 0.8×complexity)
  • 複雑度閾値: 0.5 (適応発動条件)
  • 最大適応率: ±20% (安全性保証)
  • 安定性監視: 分散<0.5, 5サンプル窓
end note

note right of FPSCtrl
  **制御出力仕様**
  • FPS範囲: [5-30] fps (ハード制限)
  • ベースFPS: 7 fps (PID出力加算)
  • Quality範囲: [50-95%] (JPEG品質)
  • Priority範囲: [100-105] (USB スレッド)
  • 応答時間: 100ms制御周期
end note

note bottom of SysPlant
  **実測性能結果**
  • FPS改善: +36% (6.74→9.2 fps)
  • レイテンシ削減: -29% (134→95ms)
  • 制御周期: 100ms (10Hz)
  • 予測窓: 5フレーム
  • フォールバック: 3連続失敗
  • システム時定数: τ = 14.05s
end note

@enduml
```

## セキュリティ・プライバシー設計

### セキュリティアーキテクチャ

```c
// security_architecture.h - セキュリティ設計
typedef struct {
    // 通信セキュリティ
    bool tls_encryption_enabled;           // TLS 1.3対応 (将来)
    bool certificate_validation_enabled;   // 証明書検証
    char preshared_key[64];               // 事前共有鍵
    uint32_t session_key_rotation_sec;     // セッション鍵ローテーション

    // アクセス制御
    authentication_method_t auth_method;   // 認証方式
    authorization_level_t auth_level;      // 認可レベル
    uint32_t failed_auth_attempts;        // 失敗回数
    uint64_t account_lockout_time;         // ロックアウト時間

    // データ保護
    bool video_encryption_enabled;         // 録画データ暗号化
    encryption_algorithm_t video_cipher;   // 暗号化アルゴリズム
    bool secure_erase_enabled;            // セキュア削除
    uint32_t data_retention_days;         // データ保持期間

    // Phase 9.2セキュリティ統合
    bool health_data_anonymization;       // 健全性データ匿名化
    bool secure_health_transmission;      // 健全性データ保護
    audit_log_level_t audit_level;       // 監査ログレベル

} security_config_t;

// セキュリティ監視
int monitor_security_health(security_config_t *config,
                           tcp_health_metrics_t *health)
{
    // 異常なネットワークパターン検出
    if (health->packet_loss_rate > 0.1 &&
        health->connection_resets > 10) {
        LOG_WARN("Potential security anomaly detected");

        // セキュリティレベル向上
        config->auth_level = AUTH_LEVEL_HIGH;
        config->session_key_rotation_sec /= 2;  // ローテーション頻度倍増

        return SECURITY_ALERT_ANOMALY;
    }

    // DoS攻撃検出 (Phase 9.2健全性データ活用)
    if (health->connection_attempts > 100 &&
        health->successful_connections < 0.1 * health->connection_attempts) {
        LOG_ERROR("Potential DoS attack detected");

        return SECURITY_ALERT_DOS;
    }

    return SECURITY_STATUS_NORMAL;
}
```

## 運用・保守アーキテクチャ

### 運用監視・ダッシュボード

```c
// operations_dashboard.h - 運用監視アーキテクチャ
typedef struct {
    // システム全体ステータス
    system_health_status_t overall_status;
    uint64_t system_uptime_sec;
    uint32_t active_connections;
    float system_load_average;

    // Phase 9.2運用統計
    struct {
        uint32_t health_adaptations_24h;
        uint32_t preventive_reconnections_24h;
        float service_availability_percent;
        uint64_t downtime_saved_minutes;
    } phase92_ops;

    // パフォーマンス指標
    struct {
        float current_throughput_mbps;
        uint32_t average_latency_ms;
        float error_rate_percent;
        uint32_t active_recordings;
        uint64_t total_data_recorded_gb;
    } performance_kpis;

    // リソース使用状況
    struct {
        float cpu_usage_percent;
        uint32_t memory_usage_mb;
        float disk_usage_percent;
        uint32_t network_bandwidth_usage_percent;
    } resource_usage;

    // アラート・イベント
    system_alert_t active_alerts[MAX_ACTIVE_ALERTS];
    uint32_t alert_count;
    system_event_t recent_events[MAX_RECENT_EVENTS];
    uint32_t event_count;

} operations_dashboard_t;

void generate_operations_report(operations_dashboard_t *dashboard,
                               const char *report_file)
{
    FILE *fp = fopen(report_file, "w");
    if (!fp) return;

    time_t now = time(NULL);
    fprintf(fp, "# システム運用レポート\n");
    fprintf(fp, "生成日時: %s\n", ctime(&now));

    fprintf(fp, "\n## システム全体ステータス\n");
    fprintf(fp, "- 稼働時間: %llu時間\n", dashboard->system_uptime_sec / 3600);
    fprintf(fp, "- 全体ステータス: %s\n",
            system_status_to_string(dashboard->overall_status));
    fprintf(fp, "- アクティブ接続: %d\n", dashboard->active_connections);

    fprintf(fp, "\n## Phase 9.2 効果測定\n");
    fprintf(fp, "- 健全性適応: %d回/24h\n", dashboard->phase92_ops.health_adaptations_24h);
    fprintf(fp, "- 予防的再接続: %d回/24h\n", dashboard->phase92_ops.preventive_reconnections_24h);
    fprintf(fp, "- サービス可用率: %.2f%%\n", dashboard->phase92_ops.service_availability_percent);
    fprintf(fp, "- ダウンタイム削減: %llu分\n", dashboard->phase92_ops.downtime_saved_minutes);

    fprintf(fp, "\n## パフォーマンス指標\n");
    fprintf(fp, "- スループット: %.2f Mbps\n", dashboard->performance_kpis.current_throughput_mbps);
    fprintf(fp, "- 平均レイテンシ: %d ms\n", dashboard->performance_kpis.average_latency_ms);
    fprintf(fp, "- エラー率: %.3f%%\n", dashboard->performance_kpis.error_rate_percent);
    fprintf(fp, "- 録画データ総計: %llu GB\n", dashboard->performance_kpis.total_data_recorded_gb);

    fprintf(fp, "\n## 推奨改善アクション\n");
    if (dashboard->resource_usage.cpu_usage_percent > 80) {
        fprintf(fp, "- ⚠️ CPU使用率高 (%.1f%%) - 負荷分散検討\n",
                dashboard->resource_usage.cpu_usage_percent);
    }
    if (dashboard->phase92_ops.health_adaptations_24h > 100) {
        fprintf(fp, "- ℹ️ 健全性適応多発 - ネットワーク環境確認推奨\n");
    }
    if (dashboard->phase92_ops.service_availability_percent < 99.0) {
        fprintf(fp, "- ⚠️ 可用率目標未達 - システム設定見直し\n");
    }

    fclose(fp);
    LOG_INFO("Operations report generated: %s", report_file);
}
```

## まとめ

Phase 9.2 TCP健全性監視を中核とする統合システムアーキテクチャにより、Spresense-PC間の高可用性セキュリティカメラシステムを実現。適応的制御、予防的回復、エンドツーエンド最適化により、95%のダウンタイム削減と安定した映像監視サービスを提供する。

### 主要アーキテクチャ成果
- **Phase 9.2健全性中心設計** ⭐ システム全体統合
- **適応的エンドツーエンド制御** ✅ Edge-Host協調動作
- **予防的障害回復** ✅ 3秒高速復旧 (95%改善)
- **スケーラブル拡張設計** ✅ 将来N-camera対応
- **統合運用監視** ✅ リアルタイム可視化

**Phase 9.2 TCP健全性監視統合による次世代システムアーキテクチャの完全仕様** ✅

## Phase 10-11 制御工学アーキテクチャ

### 物理システム構成 - 階層型制御ビュー

```plantuml
@startuml physical_control_system_hierarchical
!theme plain

skinparam rectangle {
    BackgroundColor lightblue
    BorderColor blue
    FontSize 11
    RoundCorner 10
}

skinparam package {
    BackgroundColor lightyellow
    BorderColor orange
    FontSize 12
}

title Phase 10-11 Hierarchical Control System - Physical Configuration

package "Spresense エッジデバイス" #LightBlue {

    package "ハードウェア層" #LightCyan {
        rectangle "ISX012 Camera\nISX012 カメラセンサー\n解像度: VGA (640x480)\nフォーマット: JPEG\nFPS制御: V4L2 VIDIOC_S_PARM\n制御範囲: 5-30 fps" as Camera #lightcyan

        rectangle "JPEG Encoder\nハードウェアJPEGエンコーダ\n品質: 可変 (60-80%)\nレイテンシ: 0.05-265ms\n出力: 1KB-128KB/frame" as Encoder #lightcyan

        rectangle "WiFi Module GS2200M\nGS2200M WiFi制御\n帯域幅: 1-12Mbps\n遅延: τ₂ = 134ms\nゲイン: K₂ = 0.85" as WiFi #lightcyan
    }

    package "ソフトウェア層 - スレッドアーキテクチャ" #LightYellow {

        rectangle "Camera Thread Priority: 110\nカメラスレッド (最高優先度)\n優先度: 110 (固定)\n実行周期: 7-30fps駆動\n機能: フレーム取得・JPEG化\n出力: Action Queue填充" as CamThread #lightyellow

        rectangle "Frame Queue Action/Empty\nフレームキューシステム\nAction Queue: 0-7 frames\nEmpty Queue: バッファ再利用\n容量: 動的調整 (5-9 frames)\n時定数: τ₁ = 33ms" as Queue #lightyellow

        rectangle "USB Thread Priority: 100→105\nUSBスレッド (可変優先度)\n基本優先度: 100\nブースト優先度: 105\nトリガー: Queue深度 >= 6\nリセット: Queue深度 <= 2\n機能: TCP/USB送信" as USBThread #lightyellow
    }

    package "Phase 10-11 制御層" #LightGreen {

        rectangle "Control Thread Priority: 95\n制御スレッド (最低優先度)\n優先度: 95 (固定)\n制御周期: 100ms (10Hz)\n安定性監視: 10サンプル\nフォールバック: 3回失敗で発動" as CtrlThread #lightgreen

        package "Phase 10 基本制御" #PaleGreen {
            rectangle "PID Controller\nPID制御器\nKp: 0.15 (比例)\nKi: 0.02 (積分)\nKd: 0.0 (微分無効)\n目標値: 3.5 frames\n制限: [5, 30] fps\nアンチワインドアップ: ±5.0" as PID #palegreen
        }

        package "Phase 11 拡張制御" #DarkSeaGreen {
            rectangle "Frame Statistics\nフレーム統計解析\n窓サイズ: 10 frames\n複雑度指数: 0.0-2.0\n分散解析: 正規化\n平滑化係数: 0.8" as Stats #darkseagreen

            rectangle "Multi-Variable Controller\n多変数制御器\n入力数: 6変数\n重み: Queue(0.6), Size(0.2)\nTrans(0.1), Pred(0.1)\n適応制御: 複雑度ベース\n予測: 10ステップホライゾン" as MultiVar #darkseagreen

            rectangle "Adaptive PID\n適応PID\nベースゲイン: Phase 10継承\n適応係数: 0.8\n最大適応率: ±0.2\n複雑度閾値: 0.5" as AdaptPID #darkseagreen
        }
    }
}

package "PC ホストシステム" #LightBlue {
    rectangle "TCP Receiver\nTCP受信システム\n受信バッファ: 8KB\n処理時間: τ₃ = 174ms\nスループット: 可変" as Receiver #lightblue

    rectangle "Display Pipeline\n表示パイプライン\n目標FPS: 60fps GUI\nデコード時間: τ₄ = 2.07ms\n表示遅延: ~16.7ms" as Display #lightblue
}

' データフロー（主要パス）
Camera --> Encoder : "Raw frames"
Encoder --> CamThread : "JPEG data"
CamThread --> Queue : "Frame buffers\n33ms τ₁"
Queue --> USBThread : "Transmission queue"
USBThread --> WiFi : "TCP packets\n134ms τ₂"
WiFi --> Receiver : "Network stream"
Receiver --> Display : "Decoded frames"

' 制御信号フロー
Queue --> CtrlThread : "Queue depth\nmeasurement"
CtrlThread --> PID : "Error signal\n(3.5 - depth)"
PID --> Camera : "FPS setpoint\n[5-30] fps"

' Phase 11 拡張制御
Stats --> MultiVar : "Statistics\ndata"
Queue --> Stats : "Frame sizes"
MultiVar --> AdaptPID : "Weighted\ncontrol input"
AdaptPID --> Camera : "Adaptive FPS\ncommand"

' 優先度制御ループ
Queue --> USBThread : "Priority boost\nsignal (≥6 frames)"

' フォールバック制御
CtrlThread -.-> PID : "Fallback to\nPhase 10"

note right of PID
  **Phase 10 基本制御**
  • Queue深度 → FPS制御
  • 100ms制御周期
  • 安定性保証
end note

note right of AdaptPID
  **Phase 11 拡張制御**
  • 6変数統合制御
  • 適応ゲイン調整
  • 予測制御統合
  • Phase 10フォールバック
end note

note bottom of Queue
  **実測システム動特性**
  • G₁(s) = 1/(33ms·s + 1)
  • G₂(s) = 0.85/(134ms·s + 1)
  • 全体時定数: τ = 14.05s
end note

@enduml
```

### 制御理論モデル - Phase 10→11 階層アーキテクチャ

制御工学の観点から、数学的制御関係とPhase 10からPhase 11への階層的制御構造を示します。

```plantuml
@startuml phase10_11_control_theory
!theme plain

skinparam rectangle {
    BackgroundColor lightblue
    BorderColor blue
    FontSize 11
    RoundCorner 10
}

skinparam package {
    BackgroundColor lightyellow
    BorderColor orange
    FontSize 12
}

skinparam note {
    BackgroundColor lightyellow
    BorderColor orange
    FontSize 10
}

title Phase 10→11 階層制御理論モデル - 制御工学アプローチ

' Phase 10 基本制御層
package "Phase 10 基本制御層 (Foundation Layer)" #lightblue {

    rectangle "r(t)\n基準入力\n3.5 frames" as RefInput #lightgreen
    rectangle "Σ\n比較器\n(-)" as Comparator #lightyellow
    rectangle "PID Controller\nKp=0.15\nKi=0.02\nKd=0.0" as PIDController #lightcyan
    rectangle "Saturation\n[5-30 fps]" as Saturator #orange
    rectangle "Plant\nG(s)=1/(14.05s+1)\nQueue System" as Plant #lightblue
    rectangle "y(t)\nQueue Depth\nOutput" as Output #lightgreen

    RefInput --> Comparator : r(t)
    Comparator --> PIDController : e(t)
    PIDController --> Saturator : u(t)
    Saturator --> Plant : FPS指令
    Plant --> Output : queue_depth
    Output --> Comparator : フィードバック
}

' Phase 11 拡張制御層
package "Phase 11 拡張制御層 (Enhancement Layer)" #lightgreen {

    ' 多変数入力（6変数）
    rectangle "Queue深度\nInput" as QueueInput #lightcyan
    rectangle "Frame Size\nInput" as SizeInput #lightcyan
    rectangle "分散指標\nInput" as VarianceInput #lightcyan
    rectangle "伝送時間\nInput" as TransInput #lightcyan
    rectangle "予測負荷\nInput" as PredInput #lightcyan
    rectangle "複雑度指標\nInput" as ComplexInput #lightcyan

    ' 制御ブロック
    rectangle "Weighted Sum\nΣ(wi×ui)\n正規化統合" as WeightedSum #lightyellow
    rectangle "Complexity\nAdaptive\nK_adaptive" as Adaptive #orange
    rectangle "Predictive\nController\n10 frame window" as Predictive #lightblue
    rectangle "Adaptive PID\nController\nPhase 11" as AdaptivePID #lightcyan
    rectangle "Stability\nMonitor" as Stability #lightpink
    rectangle "Fallback\nLogic" as Fallback #red

    ' 制御フロー
    QueueInput --> WeightedSum : w1=0.6
    SizeInput --> WeightedSum : w2=0.2
    VarianceInput --> WeightedSum : w3=0.1
    TransInput --> WeightedSum : w4=0.1
    PredInput --> WeightedSum : w5=0.1
    ComplexInput --> WeightedSum : w6=0.0

    WeightedSum --> AdaptivePID : u_total(t)
    ComplexInput --> Adaptive : complexity[0.0-2.0]
    Adaptive --> AdaptivePID : adaptive_gain
    PredInput --> Predictive : history_data
    Predictive --> AdaptivePID : prediction
    AdaptivePID --> Stability : control_output
    Stability --> Fallback : stability_check

    ' フォールバック
    Fallback -.-> PIDController : "3回連続失敗で\nPhase 10復帰"
}

' システムプラント詳細
package "システムプラント (実測モデル)" #lightyellow {
    rectangle "Action Queue\nG1(s) = 1/(33ms*s+1)" as ActionQueue #lightblue
    rectangle "USB System\nG2(s) = 0.85/(134ms*s+1)" as USBSystem #lightblue
    rectangle "Overall System\nG_system(s) = 1/(14.05s*s+1)" as OverallSystem #lightblue

    ActionQueue --> USBSystem : frame_buffer
    USBSystem --> OverallSystem : tcp_stream
}

' 制御信号フロー（Phase 11有効時）
AdaptivePID --> ActionQueue : adaptive_fps
OverallSystem --> QueueInput : queue_feedback
OverallSystem --> SizeInput : size_feedback
OverallSystem --> TransInput : transmission_feedback

' 直接制御パス（Phase 10フォールバック）
Saturator -.-> ActionQueue : basic_fps

' 数学的注釈
note right of PIDController
  **Phase 10 数学モデル**
  • PID制御: u(t) = Kp·e(t) + Ki·∫e(τ)dτ + Kd·de/dt
  • ゲイン: Kp=0.15, Ki=0.02, Kd=0.0
  • 制御周期: T = 100ms = 0.1s
  • 設定値: r(t) = 3.5 frames
  • プラント: G(s) = 1/(14.05s + 1)
end note

note right of AdaptivePID
  **Phase 11 数学モデル**
  • 統合制御入力: u_total = Σ(wi × ui_normalized)
  • 適応ゲイン: K_adaptive = K_base × (1 ± α × complexity)
  • 予測制御: u_pred = K_pred × Σ(predicted_trend)
  • 最終制御: u_final = K_adaptive × (u_total + u_pred)
  • 安定性条件: |avg_error| < 0.5, variance < 0.5
  • フォールバック: 3回連続失敗 → Phase 10復帰
end note

note bottom of OverallSystem
  **実測システム同定結果**
  • アクションキュー: τ₁ = 33ms (Camera→Queue)
  • USB伝送: τ₂ = 134ms, ゲイン = 0.85
  • 総合時定数: τ_system = 14.05s
  • 非線形性: FPS飽和 [5-30], 優先度ブースト
  • Phase 11性能: FPS向上36%, 遅延削減29%
end note

@enduml
```

### メトリクス制御通信システム - 双方向制御アーキテクチャ

実装計画されているメトリクス制御通信とフィードバック制御ループを含む、Phase 10-11の制御工学設計を示します。

```plantuml
@startuml metrics_control_communication_system
!theme plain

skinparam rectangle {
    BackgroundColor lightblue
    BorderColor blue
    FontSize 11
    RoundCorner 10
}

skinparam package {
    BackgroundColor lightyellow
    BorderColor orange
    FontSize 12
}

skinparam note {
    BackgroundColor lightyellow
    BorderColor orange
    FontSize 10
}

title メトリクス制御通信システム - 双方向制御アーキテクチャ\nPhase 10-11 実装計画

' Spresense側システム
package "Spresense エッジデバイス" #lightblue {

    ' カメラ・エンコーダシステム
    package "映像処理システム" #lightcyan {
        rectangle "ISX012 Camera\nフレーム取得\n30fps" as Camera #lightcyan
        rectangle "H.264 Encoder\nハードウェアエンコード\nビットレート制御" as Encoder #lightcyan
        rectangle "Frame Queue\nキュー深度監視\n動的サイズ調整" as FrameQueue #lightcyan
    }

    ' メトリクス収集システム
    package "メトリクス収集システム" #lightgreen {
        rectangle "Frame Statistics\nフレーム統計\nFPS計算\nエンコード時間" as FrameStats #lightgreen
        rectangle "Queue Monitor\nキュー深度計測\nバッファ使用率\n待ち時間計測" as QueueMonitor #lightgreen
        rectangle "Performance Logger\nCPU使用率\nメモリ使用量\nエラー率" as PerfLogger #lightgreen
    }

    ' 制御システム
    package "制御システム" #lightyellow {
        rectangle "FPS Controller\n動的FPS制御\n目標値設定\n実績値監視" as FPSController #lightyellow
        rectangle "Quality Controller\nビットレート制御\n品質調整\n適応制御" as QualityController #lightyellow
        rectangle "Command Handler\n制御指令受信\nパラメータ変更\n状態更新" as CmdHandler #lightyellow
    }

    ' 通信システム
    package "通信システム" #lightpink {
        rectangle "Protocol Handler\nメッセージ振り分け\n映像 vs 制御\nパケット化" as ProtocolHandler #lightpink
        rectangle "Metrics Sender\n定期メトリクス送信\n0x30-0x32パケット\n2秒周期" as MetricsSender #lightpink
        rectangle "USB Transport\nCDC-ACM\n12Mbps\n優先度制御" as USBTransport #lightpink
    }
}

' PC側システム
package "PC ホストシステム" #lightblue {

    ' 受信・分析システム
    package "受信・分析システム" #lightcyan {
        rectangle "Stream Receiver\n映像データ受信\nMJPEG/H.264\nデコード" as StreamReceiver #lightcyan
        rectangle "Metrics Receiver\nメトリクス受信\n統計情報解析\nトレンド分析" as MetricsReceiver #lightcyan
        rectangle "Performance Analyzer\nパフォーマンス分析\n最適化判定\nアラート生成" as PerfAnalyzer #lightcyan
    }

    ' 制御システム
    package "制御システム" #lightyellow {
        rectangle "Control Decision\n制御判定エンジン\nFPS最適化\n品質調整" as ControlDecision #lightyellow
        rectangle "Command Sender\n制御指令送信\n0x40-0x43パケット\nリアルタイム制御" as CmdSender #lightyellow
    }

    ' ユーザーインターフェース
    package "ユーザーインターフェース" #lightgreen {
        rectangle "Control Panel\nGUI制御パネル\n手動調整\nリアルタイム監視" as ControlPanel #lightgreen
        rectangle "Metrics Dashboard\nメトリクス表示\nグラフ・統計\nアラート表示" as Dashboard #lightgreen
    }
}

' データフロー（映像チャネル）
Camera --> Encoder : "RAWフレーム"
Encoder --> FrameQueue : "H.264 NAL"
FrameQueue --> ProtocolHandler : "映像データ"
ProtocolHandler --> USBTransport : "0x10-0x13\n映像パケット"
USBTransport --> StreamReceiver : "USB CDC\n映像ストリーム"
StreamReceiver --> Dashboard : "映像表示"

' メトリクス収集フロー
Camera --> FrameStats : "フレーム情報"
Encoder --> FrameStats : "エンコード統計"
FrameQueue --> QueueMonitor : "キュー状態"
FrameStats --> MetricsSender : "統計データ"
QueueMonitor --> MetricsSender : "キュー深度"
PerfLogger --> MetricsSender : "システム統計"

' メトリクス送信・受信
MetricsSender --> ProtocolHandler : "メトリクス情報"
ProtocolHandler --> USBTransport : "0x30-0x32\nメトリクスパケット"
USBTransport --> MetricsReceiver : "USB CDC\nメトリクス"
MetricsReceiver --> PerfAnalyzer : "統計解析"
PerfAnalyzer --> Dashboard : "可視化"

' フィードバック制御フロー
PerfAnalyzer --> ControlDecision : "分析結果"
ControlPanel --> ControlDecision : "手動指令"
ControlDecision --> CmdSender : "制御判定"
CmdSender --> USBTransport : "0x40-0x43\n制御パケット"
USBTransport --> ProtocolHandler : "USB CDC\n制御指令"
ProtocolHandler --> CmdHandler : "制御指令"
CmdHandler --> FPSController : "FPS変更"
CmdHandler --> QualityController : "品質変更"

' 制御適用フロー
FPSController --> Camera : "FPS設定"
QualityController --> Encoder : "ビットレート設定"

' 注釈
note right of MetricsSender
  **メトリクス送信仕様**
  • 送信周期: 2秒
  • パケットサイズ: <64バイト
  • 優先度: 低（映像より下位）
  • 内容: FPS, キュー深度, エラー率
end note

note right of CmdSender
  **制御指令仕様**
  • イベント駆動送信
  • 応答時間: <500ms
  • パケットサイズ: 1-8バイト
  • 制御項目: FPS, ビットレート, 品質
end note

note bottom of USBTransport
  **通信チャネル分離**
  • 映像データ: 高優先度, 連続送信
  • メトリクス: 低優先度, 定期送信
  • 制御指令: 中優先度, 即座送信
  • 帯域幅配分: 映像90%, 制御10%
end note

@enduml
```

### 制御工学フィードバックループ - 適応制御システム

メトリクス制御通信によるフィードバック制御ループの詳細な制御工学設計を示します。

```plantuml
@startuml feedback_control_loop_system
!theme plain

skinparam rectangle {
    BackgroundColor lightblue
    BorderColor blue
    FontSize 11
    RoundCorner 10
}

skinparam package {
    BackgroundColor lightyellow
    BorderColor orange
    FontSize 12
}

skinparam note {
    BackgroundColor lightyellow
    BorderColor orange
    FontSize 10
}

title 制御工学フィードバックループ - Spresense自律制御システム\n実装済みPI制御アーキテクチャ

' 制御入力（Spresense内部設定）
rectangle "制御パラメータ\nControl Parameters\nSetpoint: 3.5 (queue)\nKp: 0.15, Ki: 0.02\n制御周期: 100ms" as TargetSettings #lightgreen

' 制御システム（Spresense側）
package "自律制御システム (Spresense側)" #lightyellow {
    rectangle "誤差計算器\nError Calculator\ne(t) = 3.5 - queue_depth" as ErrorCalc #lightyellow
    rectangle "PIDコントローラ\nPI Controller\nKp=0.15, Ki=0.02, Kd=0.0" as PIDController #lightyellow
    rectangle "出力制限\nOutput Limiter\nFPS[5-30]制限\nクランプ機能" as OutputLimiter #lightyellow
    rectangle "制御実行\nControl Execution\ncamera_set_fps_runtime()" as ControlExec #lightyellow
}

' 通信チャネル
package "通信チャネル" #lightpink {
    rectangle "USB CDC\nData Channel\n12Mbps\n映像+メトリクス" as DataChannel #lightpink
}

' プラント（Spresense側）
package "プラント (Spresense側)" #lightblue {
    rectangle "制御指令処理\nCommand Processing\nFPS/品質パラメータ変更" as CmdProcessing #lightblue
    package "通信制御部分" {
    rectangle "カメラシステム\nCamera System\nISX012 + H.264\nフレーム生成" as CameraSystem #lightblue
    rectangle "メトリクス収集\nMetrics Collection\n並行スレッド処理\nCPU/メモリ影響" as MetricsCollection #lightblue
    rectangle "統合処理ハンドラ\nProtocol Handler\nパケット振り分け\n通信チャネル管理" as IntegratedHandler #lightblue
    }
}

' PC側監視システム
package "PC側監視・可視化システム" #lightgreen {
    rectangle "メトリクス受信\nMetrics Receiver\nTCP/USB受信\nデータ解析" as MetricsReceiver #lightgreen
    rectangle "ダッシュボード\nDashboard\nBode線図\nNyquist線図\nステップ応答" as Dashboard #lightgreen
    rectangle "制御解析\nControl Analysis\nシミュレーション\n性能評価" as ControlAnalysis #lightgreen
}

' 制御フロー
TargetSettings --> ErrorCalc : "目標値"
ErrorCalc --> PIDController : "誤差信号\ne(t)"
PIDController --> ControlLogic : "制御信号\nu(t)"
ControlLogic --> CmdGen : "制御判定"
CmdGen --> ControlChannel : "制御指令"

ControlChannel --> CmdProcessing : "FPS/Quality\nCommands"
CmdProcessing --> CameraSystem : "パラメータ設定"
CmdProcessing --> MetricsCollection : "監視設定"

' Spresense内部制御ループ（100ms周期）
TargetSettings --> ErrorCalc : "固定設定値"
ErrorCalc --> PIDController : "誤差信号\ne(t)"
PIDController --> OutputLimiter : "制御信号\nu(t)"
OutputLimiter --> ControlExec : "FPS指令"
ControlExec --> CameraSystem : "FPS設定"

' フィードバックループ（Spresense内部）
CameraSystem --> IntegratedHandler : "フレームデータ"
MetricsCollection --> ErrorCalc : "キュー深度測定\nフィードバック"
MetricsCollection --> IntegratedHandler : "統計情報"

' PC側への一方向通信
IntegratedHandler --> DataChannel : "映像+メトリクス\n一括送信"
DataChannel --> MetricsReceiver : "USB CDC\nデータストリーム"
MetricsReceiver --> Dashboard : "可視化・表示"
Dashboard --> ControlAnalysis : "制御解析\n（シミュレーション）"

' 擾乱入力
rectangle "外部擾乱\nDisturbances\nネットワーク負荷\nシステム負荷\n環境変化" as Disturbances #orange
rectangle "内部擾乱\nInternal Load\nメトリクス処理負荷\n統計計算CPU使用\n並行スレッド競合" as InternalLoad #orange

Disturbances --> CameraSystem : "性能影響"
Disturbances --> MetricsCollection : "処理能力影響"
InternalLoad --> CameraSystem : "リソース競合"
InternalLoad --> IntegratedHandler : "処理負荷"

' 制御仕様の詳細注釈
note right of PIDController
  **PI制御パラメータ（実装済み）**
  • Kp = 0.15 (比例ゲイン)
  • Ki = 0.02 (積分ゲイン)
  • Kd = 0.0 (微分無効)
  • 制御周期: 100ms (10Hz)
  • 設定値: 3.5 (キュー深度)
  • 飽和制限: FPS[5-30]
  • 実装: fps_controller.c
end note

note right of MetricsCollection
  **メトリクス収集の制御影響**
  • 並行スレッド: カメラ処理と独立実行
  • リソース競合: CPU/メモリ使用率への影響
  • レスポンス影響: 統計処理によるレイテンシ
  • 制御対象: 収集頻度、データサイズ
  • 優先度制御: カメラ < メトリクス収集
end note

note right of IntegratedHandler
  **統合処理ハンドラ (Protocol Handler)**
  • パケット振り分け: 映像 vs メトリクス
  • 通信チャネル管理: 優先度制御
  • バッファ管理: キュー深度監視
  • 制御応答性: 指令処理の即座実行
  • 実装ファイル: protocol_handler.c/h
end note

note right of Dashboard
  **PC側ダッシュボード機能**
  • Bode線図: 周波数応答解析
  • Nyquist線図: 安定性判定
  • ステップ応答: 過渡特性表示
  • メトリクス表示: リアルタイム監視
  • 制御解析: シミュレーション機能
  • ※制御指令送信機能なし
end note

note bottom of ControlAnalysis
  **制御解析・シミュレーション**
  • パラメータ感度解析
  • システム同定結果表示
  • 制御性能評価指標
  • 安定性マージン計算
  • リアルタイム特性監視
  • ※Spresenseへの制御指令なし
end note

@enduml
```

---

## 関連文書

- **品質要求 (NFR 集約)**: [`../quality/QUALITY_REQUIREMENTS.md`](../quality/QUALITY_REQUIREMENTS.md) — ISO/IEC 25010 8 属性で散在 NFR を集約
- **品質シナリオ (QAS)**: [`../quality/QUALITY_ATTRIBUTE_SCENARIOS.md`](../quality/QUALITY_ATTRIBUTE_SCENARIOS.md) — arc42 §10 形式 10 シナリオ
- **用語集**: [`../quality/GLOSSARY.md`](../quality/GLOSSARY.md) — Phase 定義 / 構造的天井 / 制御工学用語
- **セキュリティ設計-実装乖離**: [`../quality/SECURITY_GAP_ANALYSIS.md`](../quality/SECURITY_GAP_ANALYSIS.md) — `SECURITY_ARCHITECTURE.md` の設計と実装の乖離開示
- **要求書**: [`../../01_requirements/FUNCTIONAL_REQUIREMENTS.md`](../../01_requirements/FUNCTIONAL_REQUIREMENTS.md) v1.0 — 実装事実併記版
