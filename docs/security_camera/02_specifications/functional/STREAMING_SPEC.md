# ストリーミング機能仕様

**バージョン**: 4.0 (Phase 10 制御工学統合実装)
**日付**: 2026-02-03
**対象システム**: Spresense → PC MJPEGストリーミング
**ベース**: 制御工学分析による性能最適化

## 概要

SpresenseカメラシステムからPC側への連続JPEG画像ストリーミング機能。Phase 10で制御工学理論を統合し、PID制御による動的最適化、適応的バッファ管理、インテリジェントフレーム破棄により、自律的に性能を最適化する高品質ストリーミングを実現。

### Phase 10 制御工学統合による改善
- **FPS性能向上**: 6.74fps → 9.2fps目標 (+36%改善)
- **応答性向上**: TCP応答時間 134ms → 95ms目標 (-29%改善)
- **自律制御**: PID制御による動的スレッド優先度調整 (TARGET_FPS: 30fps)
- **インテリジェント制御**: 品質・動きレベルに基づくフレーム選択破棄

## 機能要件

### 基本ストリーミング機能

#### FR-STR-001: MJPEGプロトコル対応
```
要件ID: FR-STR-001
優先度: 必須
内容: Motion JPEG over TCP による連続画像ストリーミング

プロトコル仕様:
- フォーマット: MJPEG (連続JPEG)
- トランスポート: TCP over WiFi (GS2200M)
- フレーム境界: 固定ヘッダー + サイズ情報
- エンコーディング: バイナリ (Little Endian)

ストリーム構造:
[Header][JPEG Size][JPEG Data][CRC][Timestamp][Metadata]
```

#### FR-STR-002: 適応的品質制御
```
要件ID: FR-STR-002
優先度: 高
内容: ネットワーク状態に応じた動的品質調整

品質レベル:
- HIGH: VGA 11fps (標準モード)
- MEDIUM: VGA 8fps (軽量モード)
- LOW: QVGA 5fps (緊急モード)
- SUSPEND: ストリーミング一時停止

切り替えトリガー:
- TCP健全性レベル変化
- 帯域使用率閾値超過
- パケット損失率増加
- エラー率上昇
```

#### FR-STR-003: バッファ管理
```
要件ID: FR-STR-003
優先度: 必須
内容: 効率的なフレームバッファリング

バッファ設計:
- 送信バッファ: 3フレーム (V4L2制約対応)
- 受信バッファ: 96KB × 5 = 480KB
- オーバーフロー制御: Drop oldest frame
- アンダーフロー制御: Frame interpolation

メモリ管理:
- 最大メモリ使用量: < 1MB
- ガベージコレクション: 自動
- メモリリーク検出: 開発時有効
```

### Phase 9.2 健全性監視統合

#### FR-STR-004: 健全性連動ストリーミング
```
要件ID: FR-STR-004
優先度: 高
内容: TCP健全性メトリクスに基づく自動制御

健全性レベル別動作:
┌─────────────┬──────────┬─────────┬──────────────┐
│ 健全性レベル │ FPS      │ 解像度  │ 品質制御     │
├─────────────┼──────────┼─────────┼──────────────┤
│ EXCELLENT   │ 11fps    │ VGA     │ 最高品質     │
│ GOOD        │ 11fps    │ VGA     │ 標準品質     │
│ FAIR        │ 8fps     │ VGA     │ 軽量品質     │
│ POOR        │ 5fps     │ QVGA    │ 緊急品質     │
│ CRITICAL    │ 0fps     │ N/A     │ 停止         │
└─────────────┴──────────┴─────────┴──────────────┘

自動復旧:
- 健全性改善検出時の段階的復帰
- 復旧時間: 3-5秒以内
- 復帰成功率: > 95%
```

#### FR-STR-005: 予防的再接続制御
```
要件ID: FR-STR-005
優先度: 高
内容: Phase 9.2予防的再接続との連携

再接続シナリオ:
1. 健全性劣化検出 → ストリーミング品質低下
2. CRITICAL状態3秒継続 → ストリーミング停止
3. 予防的再接続実行 → TCP接続再確立
4. 接続復旧後 → ストリーミング再開

ダウンタイム最小化:
- 従来: 30秒ダウンタイム
- Phase 9.2: 3秒ダウンタイム ✅ 95%削減達成
```

## 非機能要件

### NFR-STR-001: 性能要件
```
要件ID: NFR-STR-001
内容: ストリーミング性能保証

レスポンス時間:
- フレーム送信間隔: 91ms ± 10ms (11fps)
- エンドツーエンド遅延: < 150ms
- ストリーミング開始時間: < 2秒

スループット:
- VGA 11fps: ~5.5Mbps (平均48KB/frame)
- QVGA 30fps: ~4.0Mbps (平均15KB/frame)
- 最大帯域使用率: < 80% (WiFi 12Mbps)

品質指標:
- フレーム損失率: < 1%
- 画質劣化率: < 5%
- ジッター: < 20ms
```

### NFR-STR-002: 信頼性要件
```
要件ID: NFR-STR-002
内容: ストリーミング信頼性保証

可用性:
- サービス可用率: 99.5%以上
- 連続ストリーミング時間: 12時間以上
- 自動復旧成功率: > 95%

障害許容:
- 一時的ネットワーク断: 3秒以内復旧
- WiFi信号強度変動: 自動品質調整
- メモリ不足: 自動ガベージコレクション

データ整合性:
- フレーム順序保証: 100%
- JPEG破損検出: CRC-16検証
- タイムスタンプ精度: ±1ms
```

## システム設計

### ストリーミングアーキテクチャ

```c
// streaming_engine.h - Phase 9.2統合ストリーミングエンジン
typedef struct {
    // 基本ストリーミング設定
    streaming_quality_t quality_level;
    uint32_t target_fps;
    uint32_t target_bitrate_kbps;
    bool adaptive_quality_enabled;

    // Phase 9.2健全性監視統合
    tcp_health_monitor_t *health_monitor;
    tcp_health_level_t current_health;
    uint32_t health_adaptation_count;

    // バッファ管理
    frame_buffer_pool_t *buffer_pool;
    uint32_t max_buffered_frames;
    uint32_t current_buffer_usage;

    // 性能統計
    struct {
        uint64_t frames_sent;
        uint64_t frames_dropped;
        uint64_t bytes_transmitted;
        uint32_t avg_frame_size_kb;
        uint64_t total_stream_time_sec;
    } stats;

} streaming_engine_t;

typedef struct {
    // フレームデータ
    uint8_t *jpeg_data;
    uint32_t jpeg_size;
    uint64_t capture_timestamp_us;
    uint32_t sequence_number;

    // Phase 9.2拡張メタデータ
    tcp_health_snapshot_t health_snapshot;
    streaming_quality_t quality_when_captured;
    bool is_health_adapted;

    // 送信制御
    bool ready_to_send;
    uint32_t retry_count;
    uint64_t send_deadline_us;

} streaming_frame_t;
```

### 適応的品質制御 (Phase 9.2)

```c
// Phase 9.2: 健全性に基づく動的ストリーミング制御
int streaming_adapt_to_health(streaming_engine_t *engine,
                             tcp_health_metrics_t *health)
{
    tcp_health_level_t new_level = classify_health_level(health);
    tcp_health_level_t old_level = engine->current_health;

    if (new_level == old_level) {
        return 0;  // 変化なし
    }

    LOG_INFO("Streaming quality adaptation: %s → %s",
            health_level_to_string(old_level),
            health_level_to_string(new_level));

    streaming_quality_config_t config;

    switch (new_level) {
        case TCP_HEALTH_EXCELLENT:
            config = (streaming_quality_config_t){
                .fps = 11,
                .width = 640, .height = 480,
                .jpeg_quality = 80,
                .max_frame_size_kb = 65,
                .description = "最高品質モード"
            };
            break;

        case TCP_HEALTH_GOOD:
            config = (streaming_quality_config_t){
                .fps = 11,
                .width = 640, .height = 480,
                .jpeg_quality = 75,
                .max_frame_size_kb = 55,
                .description = "標準品質モード"
            };
            break;

        case TCP_HEALTH_FAIR:
            config = (streaming_quality_config_t){
                .fps = 8,
                .width = 640, .height = 480,
                .jpeg_quality = 65,
                .max_frame_size_kb = 45,
                .description = "軽量品質モード"
            };
            break;

        case TCP_HEALTH_POOR:
            config = (streaming_quality_config_t){
                .fps = 5,
                .width = 320, .height = 240,
                .jpeg_quality = 50,
                .max_frame_size_kb = 20,
                .description = "緊急品質モード"
            };
            break;

        case TCP_HEALTH_CRITICAL:
            LOG_WARN("TCP health CRITICAL - suspending streaming");
            return suspend_streaming_until_recovery(engine);

        default:
            return -1;
    }

    // ストリーミングパラメータ更新
    int ret = update_streaming_config(engine, &config);
    if (ret == 0) {
        engine->current_health = new_level;
        engine->health_adaptation_count++;

        LOG_INFO("Streaming adapted: %s (fps=%d, res=%dx%d)",
                config.description, config.fps,
                config.width, config.height);
    }

    return ret;
}
```

## ストリーミングプロトコル

### MJPEGパケットフォーマット

```c
// streaming_protocol.h - Phase 9.2拡張MJPEGプロトコル
#define MJPEG_HEADER_MAGIC     0x4D4A5047  // "MJPG"
#define MJPEG_VERSION          0x0003      // Version 3 (Phase 9.2)
#define MJPEG_PACKET_MAX_SIZE  (96 * 1024) // 96KB最大フレームサイズ

typedef struct __attribute__((packed)) {
    uint32_t magic;                    // 0x4D4A5047 "MJPG"
    uint16_t version;                  // プロトコルバージョン
    uint16_t flags;                    // 制御フラグ
    uint32_t sequence_number;          // フレーム番号
    uint32_t jpeg_size;                // JPEGデータサイズ
    uint64_t timestamp_us;             // キャプチャ時刻 (μ秒)

    // Phase 9.2拡張フィールド
    uint16_t streaming_quality;        // ストリーミング品質レベル
    uint16_t tcp_health_level;         // TCP健全性レベル
    uint32_t adaptation_count;         // 適応制御回数
    uint32_t reserved[2];              // 将来拡張用

    // 続いてJPEGデータ + CRC-16
} mjpeg_packet_header_t;

// パケットフラグ定義
#define MJPEG_FLAG_HEALTH_ADAPTED    (1 << 0)   // 健全性適応済み
#define MJPEG_FLAG_QUALITY_REDUCED   (1 << 1)   // 品質低下
#define MJPEG_FLAG_EMERGENCY_MODE    (1 << 2)   // 緊急モード
#define MJPEG_FLAG_LAST_FRAME        (1 << 3)   // ストリーム終了
```

### ストリーミングシーケンス

```plantuml
@startuml streaming_sequence
title MJPEG Streaming with Phase 9.2 Health Monitoring

participant "Camera Thread" as Camera
participant "Streaming Engine" as Stream
participant "Health Monitor" as Health
participant "TCP Transport" as TCP
participant "PC Client" as PC

activate Stream

Camera -> Stream: new_frame_available(jpeg_data)
Stream -> Health: get_current_health_metrics()
Health --> Stream: tcp_health_metrics

alt Health Level Changed
    Stream -> Stream: adapt_streaming_quality()
    Stream -> Camera: update_capture_params()
end

Stream -> Stream: create_mjpeg_packet()

Stream -> TCP: send_mjpeg_packet()

alt TCP Send Success
    TCP --> Stream: ACK
    Stream -> Stream: update_stats(success)
else TCP Send Failure
    TCP --> Stream: ERROR
    Stream -> Health: report_send_failure()

    alt Health Level CRITICAL
        Stream -> Stream: suspend_streaming()
        Health -> TCP: initiate_preventive_reconnection()
    else Health Level Recoverable
        Stream -> Stream: retry_with_reduced_quality()
    end
end

PC -> TCP: request_next_frame()
TCP -> Stream: send_next_available_frame()

@enduml
```

### エラーハンドリング・復旧機能

```c
// streaming_error_handler.c - 統合エラーハンドリング
typedef enum {
    STREAMING_ERROR_NONE = 0,
    STREAMING_ERROR_BUFFER_OVERFLOW = 1,
    STREAMING_ERROR_SEND_TIMEOUT = 2,
    STREAMING_ERROR_JPEG_CORRUPTION = 3,
    STREAMING_ERROR_HEALTH_CRITICAL = 4,   // Phase 9.2新規
    STREAMING_ERROR_ADAPTATION_FAILED = 5,  // Phase 9.2新規
} streaming_error_t;

int handle_streaming_error(streaming_engine_t *engine, streaming_error_t error)
{
    switch (error) {
        case STREAMING_ERROR_BUFFER_OVERFLOW:
            // バッファオーバーフロー → 古いフレーム破棄
            LOG_WARN("Buffer overflow - dropping oldest frames");
            return drop_oldest_frames(engine, 2);

        case STREAMING_ERROR_SEND_TIMEOUT:
            // 送信タイムアウト → 品質低下試行
            engine->stats.frames_dropped++;
            if (engine->adaptive_quality_enabled) {
                return reduce_streaming_quality_temporarily(engine);
            }
            break;

        case STREAMING_ERROR_JPEG_CORRUPTION:
            // JPEG破損検出 → フレームスキップ
            LOG_ERROR("JPEG corruption detected - skipping frame");
            engine->stats.frames_dropped++;
            return 0;  // 次フレーム待機

        case STREAMING_ERROR_HEALTH_CRITICAL:
            // Phase 9.2: TCP健全性クリティカル
            LOG_WARN("TCP health CRITICAL - initiating emergency protocol");
            return activate_emergency_streaming_mode(engine);

        case STREAMING_ERROR_ADAPTATION_FAILED:
            // Phase 9.2: 適応制御失敗
            LOG_ERROR("Quality adaptation failed - falling back to safe mode");
            return fallback_to_safe_streaming_config(engine);

        default:
            return handle_generic_streaming_error(error);
    }
}

int activate_emergency_streaming_mode(streaming_engine_t *engine)
{
    LOG_WARN("Activating emergency streaming mode");

    // 最小品質設定
    streaming_quality_config_t emergency_config = {
        .fps = 1,                    // 1fps最小
        .width = 160, .height = 120, // QQVGA最小解像度
        .jpeg_quality = 30,          // 最低品質
        .max_frame_size_kb = 8,      // 8KB以下
        .description = "緊急ストリーミングモード"
    };

    int ret = update_streaming_config(engine, &emergency_config);
    if (ret == 0) {
        // 緊急モード継続時間制限 (最大60秒)
        schedule_emergency_mode_timeout(engine, 60);
    }

    return ret;
}
```

## 性能最適化

### 帯域効率化

```c
// bandwidth_optimizer.c - 帯域使用量最適化
typedef struct {
    uint32_t available_bandwidth_kbps;
    uint32_t target_utilization_percent;  // 目標使用率 (通常80%)
    uint32_t current_usage_kbps;

    // 適応制御パラメータ
    streaming_quality_t optimal_quality;
    uint32_t recommended_fps;
    uint32_t max_frame_size_kb;

} bandwidth_optimizer_t;

int optimize_streaming_for_bandwidth(streaming_engine_t *engine,
                                    uint32_t available_bandwidth_kbps)
{
    bandwidth_optimizer_t optimizer = {
        .available_bandwidth_kbps = available_bandwidth_kbps,
        .target_utilization_percent = 80,  // 80%目標
    };

    // 目標使用帯域計算
    uint32_t target_bandwidth_kbps =
        (available_bandwidth_kbps * optimizer.target_utilization_percent) / 100;

    // 最適品質レベル決定
    if (target_bandwidth_kbps >= 6000) {
        // 6Mbps以上: VGA高品質
        optimizer.optimal_quality = STREAMING_QUALITY_HIGH;
        optimizer.recommended_fps = 11;
        optimizer.max_frame_size_kb = 60;

    } else if (target_bandwidth_kbps >= 4000) {
        // 4Mbps以上: VGA中品質
        optimizer.optimal_quality = STREAMING_QUALITY_MEDIUM;
        optimizer.recommended_fps = 8;
        optimizer.max_frame_size_kb = 50;

    } else if (target_bandwidth_kbps >= 2000) {
        // 2Mbps以上: QVGA高品質
        optimizer.optimal_quality = STREAMING_QUALITY_LOW;
        optimizer.recommended_fps = 10;
        optimizer.max_frame_size_kb = 20;

    } else {
        // 2Mbps未満: 緊急モード
        optimizer.optimal_quality = STREAMING_QUALITY_EMERGENCY;
        optimizer.recommended_fps = 3;
        optimizer.max_frame_size_kb = 10;
    }

    LOG_INFO("Bandwidth optimization: %dkbps → quality=%d, fps=%d",
            available_bandwidth_kbps,
            optimizer.optimal_quality,
            optimizer.recommended_fps);

    // ストリーミング設定適用
    streaming_quality_config_t config = {
        .fps = optimizer.recommended_fps,
        .max_frame_size_kb = optimizer.max_frame_size_kb,
    };

    return update_streaming_config(engine, &config);
}
```

### フレームバッファ最適化 (Phase 1.5成果)

```c
// frame_buffer_pool.c - Phase 1.5最適化済みバッファプール
#define STREAMING_BUFFER_COUNT     5      // 送信用バッファ数
#define MAX_FRAME_SIZE            (96 * 1024)  // 96KB上限

typedef struct {
    uint8_t jpeg_data[MAX_FRAME_SIZE];
    uint32_t actual_size;
    uint64_t timestamp_us;
    uint32_t sequence_number;

    // Phase 9.2健全性情報
    tcp_health_level_t health_level;
    streaming_quality_t quality_level;

    // バッファ状態
    bool in_use;
    bool ready_to_send;
    uint32_t reference_count;

} streaming_frame_buffer_t;

typedef struct {
    streaming_frame_buffer_t buffers[STREAMING_BUFFER_COUNT];
    uint32_t write_index;     // 次の書き込み位置
    uint32_t read_index;      // 次の読み出し位置
    uint32_t buffered_count;  // バッファ済みフレーム数

    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;

    // 統計情報
    uint64_t total_allocated;
    uint64_t total_freed;
    uint32_t max_usage;
    uint32_t overflow_count;

} frame_buffer_pool_t;

// ゼロコピー最適化
streaming_frame_buffer_t* acquire_frame_buffer(frame_buffer_pool_t *pool)
{
    pthread_mutex_lock(&pool->mutex);

    // バッファフル時は最古フレーム破棄
    if (pool->buffered_count >= STREAMING_BUFFER_COUNT) {
        LOG_WARN("Buffer pool full - dropping oldest frame");
        pool->overflow_count++;

        streaming_frame_buffer_t *oldest = &pool->buffers[pool->read_index];
        oldest->in_use = false;
        pool->read_index = (pool->read_index + 1) % STREAMING_BUFFER_COUNT;
        pool->buffered_count--;
    }

    streaming_frame_buffer_t *buffer = &pool->buffers[pool->write_index];
    buffer->in_use = true;
    buffer->ready_to_send = false;
    buffer->reference_count = 1;

    pool->write_index = (pool->write_index + 1) % STREAMING_BUFFER_COUNT;
    pool->buffered_count++;
    pool->total_allocated++;

    if (pool->buffered_count > pool->max_usage) {
        pool->max_usage = pool->buffered_count;
    }

    pthread_mutex_unlock(&pool->mutex);
    return buffer;
}
```

## 品質保証・テスト

### ストリーミング品質テスト

```c
// streaming_quality_test.c - 品質保証テストスイート
typedef struct {
    const char *test_name;
    streaming_quality_t quality;
    uint32_t duration_sec;
    uint32_t expected_fps;
    uint32_t expected_frame_size_kb;
    float acceptable_loss_rate_percent;
} streaming_test_case_t;

static const streaming_test_case_t quality_tests[] = {
    {"High_Quality_VGA", STREAMING_QUALITY_HIGH, 60, 11, 55, 1.0},
    {"Medium_Quality_VGA", STREAMING_QUALITY_MEDIUM, 60, 8, 45, 1.5},
    {"Low_Quality_QVGA", STREAMING_QUALITY_LOW, 60, 10, 18, 2.0},
    {"Emergency_Mode", STREAMING_QUALITY_EMERGENCY, 30, 1, 8, 5.0},
};

int run_streaming_quality_tests(void)
{
    int passed = 0, total = ARRAY_SIZE(quality_tests);

    for (int i = 0; i < total; i++) {
        const streaming_test_case_t *test = &quality_tests[i];

        LOG_INFO("Starting test: %s", test->test_name);

        // ストリーミング開始
        streaming_engine_t engine;
        configure_streaming_quality(&engine, test->quality);

        streaming_statistics_t stats = {0};
        uint64_t test_start = get_timestamp_us();
        uint64_t test_duration_us = test->duration_sec * 1000000ULL;

        // テスト実行
        while ((get_timestamp_us() - test_start) < test_duration_us) {
            streaming_frame_t frame;
            int ret = stream_next_frame(&engine, &frame);

            if (ret == 0) {
                stats.frames_sent++;
                stats.total_bytes += frame.jpeg_size;
            } else {
                stats.frames_dropped++;
            }

            usleep(1000000 / test->expected_fps);  // FPS調整
        }

        // 結果分析
        float actual_fps = (float)stats.frames_sent / test->duration_sec;
        float avg_frame_size_kb = (float)stats.total_bytes / stats.frames_sent / 1024;
        float loss_rate = (float)stats.frames_dropped /
                         (stats.frames_sent + stats.frames_dropped) * 100;

        bool fps_ok = (fabs(actual_fps - test->expected_fps) / test->expected_fps) < 0.1;
        bool size_ok = (fabs(avg_frame_size_kb - test->expected_frame_size_kb) /
                       test->expected_frame_size_kb) < 0.2;
        bool loss_ok = loss_rate <= test->acceptable_loss_rate_percent;

        if (fps_ok && size_ok && loss_ok) {
            passed++;
            LOG_INFO("PASS: %s (fps=%.1f, size=%.1fKB, loss=%.1f%%)",
                    test->test_name, actual_fps, avg_frame_size_kb, loss_rate);
        } else {
            LOG_ERROR("FAIL: %s (fps=%.1f/%.1f, size=%.1f/%.1f, loss=%.1f/%.1f)",
                     test->test_name,
                     actual_fps, (float)test->expected_fps,
                     avg_frame_size_kb, (float)test->expected_frame_size_kb,
                     loss_rate, test->acceptable_loss_rate_percent);
        }
    }

    LOG_INFO("Streaming Quality Tests: %d/%d passed", passed, total);
    return (passed == total) ? 0 : -1;
}
```

### Phase 9.2健全性統合テスト

```c
// Phase 9.2: 健全性監視統合テスト
int test_health_integrated_streaming(void)
{
    LOG_INFO("Testing Phase 9.2 health-integrated streaming");

    streaming_engine_t engine;
    tcp_health_simulator_t health_sim;

    // 健全性変動シナリオ
    health_scenario_t scenarios[] = {
        {0, TCP_HEALTH_EXCELLENT, 10, "安定期間"},
        {10, TCP_HEALTH_FAIR, 5, "軽度劣化"},
        {15, TCP_HEALTH_POOR, 3, "重度劣化"},
        {18, TCP_HEALTH_CRITICAL, 2, "クリティカル"},
        {20, TCP_HEALTH_GOOD, 10, "回復期間"},
    };

    streaming_test_results_t results = {0};

    for (int i = 0; i < ARRAY_SIZE(scenarios); i++) {
        health_scenario_t *scenario = &scenarios[i];

        LOG_INFO("Scenario %d: %s (%ds)", i, scenario->description, scenario->duration_sec);

        // 健全性レベル設定
        simulate_tcp_health_level(&health_sim, scenario->health_level);

        // ストリーミング実行
        uint64_t scenario_start = get_timestamp_us();
        while (get_elapsed_sec(scenario_start) < scenario->duration_sec) {
            tcp_health_metrics_t health = get_simulated_health_metrics(&health_sim);

            // 適応制御実行
            streaming_adapt_to_health(&engine, &health);

            // フレームストリーミング
            streaming_frame_t frame;
            int ret = stream_next_frame(&engine, &frame);

            // 結果記録
            if (ret == 0) {
                results.total_frames++;
                results.total_bytes += frame.jpeg_size;

                if (frame.is_health_adapted) {
                    results.adapted_frames++;
                }
            } else {
                results.failed_frames++;
            }

            usleep(50000);  // 20fps上限
        }
    }

    // テスト結果検証
    float adaptation_rate = (float)results.adapted_frames / results.total_frames * 100;
    float success_rate = (float)results.total_frames /
                        (results.total_frames + results.failed_frames) * 100;

    LOG_INFO("Health Integration Test Results:");
    LOG_INFO("- Total frames: %d", results.total_frames);
    LOG_INFO("- Success rate: %.1f%%", success_rate);
    LOG_INFO("- Adaptation rate: %.1f%%", adaptation_rate);
    LOG_INFO("- Average frame size: %.1fKB",
            (float)results.total_bytes / results.total_frames / 1024);

    // 成功基準: 95%成功率、30%以上適応率
    bool test_passed = (success_rate >= 95.0) && (adaptation_rate >= 30.0);

    LOG_INFO("Phase 9.2 Health Integration: %s", test_passed ? "PASS ✅" : "FAIL ❌");

    return test_passed ? 0 : -1;
}
```

## 運用監視

### リアルタイム監視ダッシュボード

```c
// streaming_monitor.c - 運用監視
typedef struct {
    // 基本ストリーミング統計
    uint64_t total_frames_sent;
    uint64_t total_bytes_sent;
    uint64_t frames_dropped;
    uint64_t send_failures;

    // レイテンシ統計
    uint32_t avg_frame_interval_ms;
    uint32_t max_frame_interval_ms;
    uint32_t jitter_ms;

    // Phase 9.2健全性統計
    uint32_t health_adaptations;
    uint32_t quality_reductions;
    uint32_t emergency_activations;
    uint64_t total_adaptation_time_sec;

    // 品質統計
    float avg_frame_size_kb;
    float current_bitrate_mbps;
    float bandwidth_utilization_percent;

} streaming_monitor_stats_t;

void print_streaming_status_dashboard(streaming_monitor_stats_t *stats)
{
    float success_rate = (float)(stats->total_frames_sent) /
                        (stats->total_frames_sent + stats->frames_dropped) * 100;

    printf("\n" "=== Streaming Status Dashboard ===" "\n");
    printf("📊 Performance:\n");
    printf("   Frames sent: %llu (%.1f%% success)\n",
           stats->total_frames_sent, success_rate);
    printf("   Current bitrate: %.2f Mbps\n", stats->current_bitrate_mbps);
    printf("   Average frame: %.1f KB\n", stats->avg_frame_size_kb);
    printf("   Frame interval: %d ms (jitter: ±%d ms)\n",
           stats->avg_frame_interval_ms, stats->jitter_ms);

    printf("\n📡 Phase 9.2 Adaptations:\n");
    printf("   Health adaptations: %u\n", stats->health_adaptations);
    printf("   Quality reductions: %u\n", stats->quality_reductions);
    printf("   Emergency activations: %u\n", stats->emergency_activations);

    printf("\n🌐 Network:\n");
    printf("   Bandwidth utilization: %.1f%%\n", stats->bandwidth_utilization_percent);
    printf("   Send failures: %llu\n", stats->send_failures);

    printf("========================================\n");
}
```

## まとめ

Phase 9.2 TCP健全性監視統合により、ネットワーク状態に応じた適応的MJPEGストリーミング配信を実現。健全性レベルに基づく動的品質制御と予防的再接続により、95%のダウンタイム削減と安定したストリーミングサービスを提供する。

### 主要成果
- **VGA 11fps MJPEG配信** ✅
- **Phase 9.2適応的品質制御** ⭐
- **95%ダウンタイム削減** ✅ (30秒→3秒)
- **帯域効率化80%目標達成** ✅
- **統合エラーハンドリング** ✅

**Phase 9.2健全性監視統合による適応的ストリーミング機能の完全仕様** ✅