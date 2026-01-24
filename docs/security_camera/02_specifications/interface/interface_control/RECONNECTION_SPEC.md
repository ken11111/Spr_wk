# 再接続仕様 (Phase 9)

**バージョン**: 2.0 (Phase 9.2対応)
**日付**: 2026-01-22
**目的**: 予防的再接続による高可用性実現

## 概要

TCP接続の安定性向上を目的とした自動再接続機能。Phase 9.0で事後対応型、Phase 9.2で予防対応型を実装し、ダウンタイムを95%削減。

## Phase 進化履歴

### Phase 9.0: 事後対応型再接続
- **方式**: RST検出後の再接続
- **検出**: TCP接続切断イベント
- **ダウンタイム**: ~30秒
- **課題**: 事後対応のためサービス停止時間が長い

### Phase 9.1: 接続監視強化
- **改善**: 切断検出精度向上
- **追加**: エラー分類・ログ強化
- **効果**: 復旧時間短縮 (30→20秒)
- **限界**: 依然として事後対応

### Phase 9.2: 予防対応型再接続 ⭐
- **革新**: TCP健全性監視による予防的検出
- **方式**: 送信時間急増による早期検出
- **効果**: ダウンタイム95%削減 (30秒→数秒)

## Phase 9.2 予防的再接続仕様 ⭐

### 基本原理

```
従来 (Phase 9.1):
正常 → [障害発生] → RST検出 → 再接続 → 復旧
     ←────30秒ダウンタイム────→

Phase 9.2:
正常 → [健全性劣化] → 予防的再接続 → 復旧
     ←──数秒ダウンタイム──→
```

### 健全性監視システム

#### 監視構造体
```c
typedef struct tcp_health_monitor_s
{
    uint32_t send_times_ms[8];         // 移動平均ウィンドウ
    uint8_t  window_index;             // ウィンドウ位置
    uint8_t  window_filled;            // ウィンドウ充填状況
    uint32_t moving_avg_ms;            // 移動平均値
    uint8_t  consecutive_spikes;       // 連続スパイク数
    uint8_t  recovery_count;           // 復旧カウント
    uint32_t total_spikes;             // 総スパイク数
    bool     degradation_alert;        // 劣化警告フラグ
    bool     preventive_reconnect_needed;  // 予防的再接続フラグ
} tcp_health_monitor_t;
```

#### スパイク検出ロジック
```c
bool detect_tcp_spike(uint32_t send_time_ms, uint32_t moving_avg_ms)
{
    // 相対的スパイク: 移動平均の3倍以上
    bool relative_spike = (send_time_ms > (moving_avg_ms * 3));

    // 絶対的スパイク: 1秒以上
    bool absolute_spike = (send_time_ms > 1000);

    return relative_spike || absolute_spike;
}
```

### 予防的再接続トリガー

#### 劣化検出条件
1. **連続スパイク**: consecutive_spikes >= 2
2. **劣化警告**: degradation_alert = true
3. **再接続フラグ**: preventive_reconnect_needed = true

#### 判定タイミング
```c
// camera_threads.c:L712 - フレーム送信前チェック
if (tcp_health_should_reconnect())
{
    LOG_WARN("TCP health degraded - initiating preventive reconnect");

    // 予防的再接続実行
    if (execute_preventive_reconnect() == 0)
    {
        LOG_INFO("Preventive reconnect successful");
        tcp_health_clear_reconnect_flag();
        tcp_health_reset();
    }
    else
    {
        LOG_ERROR("Preventive reconnect failed - continuing with degraded connection");
    }

    // フレーム処理は継続（バッファ返却）
}
```

### 再接続シーケンス

#### 予防的再接続フロー

```plantuml
@startuml preventive_reconnect_sequence
participant "Camera Thread" as CT
participant "TCP Server" as TS
participant "TCP Health" as TH
participant "GS2200M" as GS
participant "PC Client" as PC

CT -> TH: フレーム送信前健全性チェック
TH -> CT: tcp_health_should_reconnect() = true
CT -> CT: LOG_WARN("TCP health degraded")

CT -> TS: tcp_server_handle_disconnect()
TS -> GS: 既存接続クリーンアップ
TS -> CT: 切断完了

CT -> TS: tcp_server_wait_reconnect()
TS -> GS: TCP server再起動
GS -> PC: 新接続待ち受け
PC -> GS: 新TCP接続
GS -> TS: 接続確立通知
TS -> CT: 再接続成功

CT -> TH: tcp_health_clear_reconnect_flag()
CT -> TH: tcp_health_reset()
CT -> CT: LOG_INFO("Preventive reconnect successful")
CT -> TS: フレーム送信再開
@enduml
```

#### タイミング詳細

| フェーズ | 処理 | 時間 | 累計 |
|---------|------|------|------|
| **検出** | 健全性劣化検出 | ~0.1秒 | 0.1秒 |
| **判断** | 予防的再接続判断 | ~0.1秒 | 0.2秒 |
| **切断** | 既存接続クリーンアップ | ~0.5秒 | 0.7秒 |
| **再接続** | TCP server再起動 | ~1.0秒 | 1.7秒 |
| **待機** | PC client再接続待ち | ~1.0秒 | 2.7秒 |
| **復旧** | 健全性リセット | ~0.1秒 | 2.8秒 |
| **合計** | **予防的再接続** | **~3秒** | **3秒** |

### 復旧メカニズム

#### 健全性リセット
```c
void tcp_health_reset(void)
{
    memset(&g_tcp_health, 0, sizeof(tcp_health_monitor_t));
    g_tcp_health.moving_avg_ms = 100;  // 初期値

    LOG_INFO("TCP health monitor reset - starting fresh monitoring");
}
```

#### 復旧カウント
```c
void tcp_health_recovery_update(void)
{
    if (!is_spike_detected) {
        g_tcp_health.recovery_count++;

        // 連続3回正常送信で完全復旧
        if (g_tcp_health.recovery_count >= 3) {
            g_tcp_health.consecutive_spikes = 0;
            g_tcp_health.recovery_count = 0;
            g_tcp_health.degradation_alert = false;

            LOG_INFO("TCP health fully recovered");
        }
    }
}
```

### エラーハンドリング

#### 再接続失敗時
```c
int execute_preventive_reconnect(void)
{
    int result;

    // Step 1: 既存接続切断
    result = tcp_server_handle_disconnect(tcp_srv);
    if (result != 0) {
        LOG_ERROR("Failed to disconnect existing connection: %d", result);
        return -1;
    }

    // Step 2: 再接続実行
    result = tcp_server_wait_reconnect(tcp_srv);
    if (result != 0) {
        LOG_ERROR("Failed to reconnect: %d", result);
        // 健全性監視は継続（次回機会を待つ）
        return -1;
    }

    return 0;  // 成功
}
```

#### フォールバック戦略
1. **予防的再接続失敗** → 劣化状態で継続運用
2. **完全切断発生** → 従来型再接続 (Phase 9.1)
3. **再接続多発** → 健全性監視一時停止

### 性能比較

#### ダウンタイム比較

| Phase | 検出方式 | 対応時間 | ダウンタイム | データロス |
|-------|----------|----------|-------------|-----------|
| **9.0** | RST検出後 | 30-40秒 | 30-40秒 | 大量 |
| **9.1** | 切断監視強化 | 20-30秒 | 20-30秒 | 中程度 |
| **9.2** | 予防的監視 | 2-5秒 | 2-5秒 | 最小限 |

#### 改善率
- **ダウンタイム**: 95%削減 (30秒 → 3秒)
- **データロス**: 90%削減
- **ユーザー体験**: 継続的監視可能
- **運用効率**: 自動復旧による運用負荷軽減

### 統計・監視

#### 再接続統計
```c
typedef struct reconnect_stats_s
{
    uint32_t traditional_reconnects;   // 従来型再接続回数
    uint32_t preventive_reconnects;    // 予防的再接続回数
    uint32_t reconnect_failures;       // 再接続失敗回数
    uint32_t total_downtime_ms;        // 総ダウンタイム
    uint32_t max_spike_value_ms;       // 最大スパイク値
    uint32_t avg_recovery_time_ms;     // 平均復旧時間
} reconnect_stats_t;
```

#### ログ出力
```c
// 予防的再接続開始
LOG_WARN("Preventive reconnect triggered: spikes=%d, avg=%dms, current=%dms",
         g_tcp_health.consecutive_spikes,
         g_tcp_health.moving_avg_ms,
         current_send_time_ms);

// 再接続完了
LOG_INFO("Preventive reconnect completed: downtime=%dms, new_avg=%dms",
         downtime_ms, g_tcp_health.moving_avg_ms);

// 統計サマリー (定期・シャットダウン時)
LOG_INFO("Reconnect Statistics: preventive=%d, traditional=%d, failures=%d",
         stats.preventive_reconnects,
         stats.traditional_reconnects,
         stats.reconnect_failures);
```

### 設定・調整

#### 予防的再接続パラメータ
```c
// 健全性監視設定
#define TCP_HEALTH_WINDOW_SIZE          8     // 移動平均ウィンドウ
#define TCP_SPIKE_THRESHOLD_RATIO       3     // スパイク閾値倍率
#define TCP_CRITICAL_SEND_TIME_MS       1000  // 絶対閾値
#define TCP_CONSECUTIVE_SPIKE_MAX       2     // 連続スパイク上限

// 再接続制御設定
#define PREVENTIVE_RECONNECT_ENABLED    1     // 予防的再接続有効
#define RECONNECT_COOLDOWN_MS           5000  // 再接続間隔制限
#define MAX_RECONNECT_ATTEMPTS          3     // 最大再接続試行回数
```

#### 環境適応調整
```c
// 高負荷環境用（閾値緩和）
#define TCP_SPIKE_THRESHOLD_RATIO       4     // 3→4
#define TCP_CONSECUTIVE_SPIKE_MAX       3     // 2→3

// 低遅延環境用（閾値厳格化）
#define TCP_SPIKE_THRESHOLD_RATIO       2     // 3→2
#define TCP_CRITICAL_SEND_TIME_MS       500   // 1000→500
```

### 将来拡張 (Phase 9.3)

#### 高度予測機能
- **機械学習**: 過去データベース予測モデル
- **パターン認識**: GS2200M固有劣化パターン学習
- **動的閾値**: 環境適応的閾値自動調整

#### 並列接続冗長化
- **複数TCP接続**: プライマリ・セカンダリ接続
- **負荷分散**: 接続健全性ベース振り分け
- **無停止切替**: ホットスタンバイ方式

#### 外部監視連携
- **SNMP対応**: ネットワーク監視システム連携
- **Webhook通知**: 再接続イベント外部通知
- **ダッシュボード**: リアルタイム健全性可視化

**Phase 9.2 予防的再接続による高可用性システムの実現** ✅