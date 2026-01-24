# トランスポート比較仕様

**バージョン**: 2.0 (Phase 9.2対応)
**日付**: 2026-01-22
**対象**: USB CDC-ACM vs WiFi TCP

## 概要

SpresenseとPC間通信で利用する2つのトランスポート方式の詳細比較。Phase 1〜9.2の実測データに基づく性能特性、使い分け指針、運用上の考慮点を体系的に整理。

## トランスポート方式概要

### USB CDC-ACM (有線)
- **規格**: USB 2.0 Full Speed (12 Mbps)
- **デバイスクラス**: Communications Device Class - Abstract Control Model
- **物理接続**: USB Type-C ケーブル
- **用途**: 開発・デバッグ・高信頼性要求時

### WiFi TCP (無線)
- **モジュール**: GS2200M WiFi (802.11 b/g/n)
- **プロトコル**: TCP/IP over WiFi
- **物理接続**: 無線LAN
- **用途**: 本格運用・ケーブルレス展開時

## 詳細性能比較

### 基本性能諸元

| 項目 | USB CDC-ACM | WiFi TCP | 比較 |
|------|-------------|----------|------|
| **理論帯域** | 12 Mbps | ~100 Mbps | WiFi優位 |
| **実効帯域** | ~10 Mbps | ~1 Mbps | USB優位 ⭐ |
| **効率** | 83% | 0.5% | USB優位 |
| **レイテンシ** | ~30ms | ~234ms | USB優位 |
| **接続安定性** | 高 | 中 (Phase 9.2改善) | USB優位 |
| **設定複雑度** | 低 | 中 | USB優位 |
| **運用柔軟性** | 低 | 高 | WiFi優位 |

### Phase別性能進化

#### フレームレート実績
```
Phase 1.0 (QVGA):
- USB: 30+ fps (安定)
- WiFi: 未対応

Phase 1.5 (VGA):
- USB: 11.0 fps (安定) ✅
- WiFi: 未対応

Phase 7.0 (WiFi導入):
- USB: 11.0 fps (継続)
- WiFi: ~3 fps (初期実装)

Phase 7.3 (WiFi最適化):
- USB: 11.0 fps (継続)
- WiFi: ~8 fps (99.85%成功率) ⭐

Phase 9.2 (TCP健全性監視):
- USB: 11.0 fps (継続)
- WiFi: ~8 fps (予防的再接続) ⭐
```

#### 信頼性進化
```
USB CDC-ACM (一貫):
- 成功率: 100%
- フレームドロップ: 0%
- エラー復旧: 不要

WiFi TCP進化:
Phase 7.0: 21% 成功率 (同期ワード方式)
Phase 7.3: 99.85% 成功率 (stateful読み取り) ⭐
Phase 9.2: 99.85%+ (予防的再接続) ⭐
```

## 技術特性詳細分析

### USB CDC-ACM特性

#### 利点 ✅
1. **高信頼性**: 100%フレーム配信保証
2. **低レイテンシ**: ~30ms USB転送時間
3. **設定簡単**: TTY Raw Mode設定のみ
4. **デバッグ容易**: シリアル通信のため解析容易
5. **電力効率**: WiFiより低消費電力

#### 制約 ⚠️
1. **帯域制限**: USB 2.0 Full Speed 12Mbps上限
2. **物理接続**: ケーブル必須・可動性制限
3. **30fps困難**: VGA解像度で帯域飽和
4. **単一接続**: 1:1接続のみ

#### 実測性能詳細 (Phase 1.5)
```c
// USB転送性能プロファイル
typedef struct {
    uint32_t packet_size_bytes;      // パケットサイズ
    uint32_t transfer_time_us;       // 転送時間
    float    effective_bandwidth_mbps; // 実効帯域
} usb_transfer_profile_t;

// VGA JPEG パケット (50KB典型)
usb_transfer_profile_t vga_profile = {
    .packet_size_bytes = 51200,      // VGA JPEG典型サイズ
    .transfer_time_us = 30000,       // 30ms転送時間
    .effective_bandwidth_mbps = 13.6 // (51200*8)/(30*1000) = 13.6Mbps
};
// 注意: 理論値12Mbpsを上回るのは測定誤差・バースト転送効果
```

### WiFi TCP特性

#### 利点 ✅
1. **無線自由度**: ケーブルレス・可動性確保
2. **複数接続**: 理論的に複数クライアント対応可能
3. **遠隔アクセス**: ネットワーク経由でアクセス可能
4. **スケーラビリティ**: WiFi APを介した拡張性

#### 制約 ⚠️
1. **低効率**: 理論値の0.5%効率
2. **高レイテンシ**: 平均234ms、最大1,189ms
3. **接続不安定**: GS2200M資源枯渇問題 (Phase 9.2改善)
4. **設定複雑**: WiFi認証・TCP設定必要

#### GS2200M性能制約分析 🔴
```c
// GS2200M TCP送信性能プロファイル (Phase 9.1実測)
typedef struct {
    uint32_t normal_send_time_ms;     // 正常時送信時間
    uint32_t congested_send_time_ms;  // 輻輳時送信時間
    uint32_t failure_send_time_ms;    // 失敗直前送信時間
    bool     rst_packet_sent;         // RST送信フラグ
} gs2200m_performance_t;

// 実測パターン
gs2200m_performance_t gs2200m_profile = {
    .normal_send_time_ms = 234,       // 平均234ms
    .congested_send_time_ms = 1189,   // 最大1,189ms
    .failure_send_time_ms = 2289,     // 失敗直前2,289ms
    .rst_packet_sent = true           // 突然のRST送信
};

// Phase 9.2改善: 予防的再接続
// failure_send_time_ms到達前に再接続実行
```

## Phase 9.2健全性監視対応 ⭐

### USB CDC-ACM: 健全性監視なし
```c
// USB CDC-ACMは安定のため監視不要
// 100%成功率継続のため追加監視実装なし
#define USB_HEALTH_MONITORING_REQUIRED    false
#define USB_PREVENTIVE_ACTION_NEEDED      false
```

### WiFi TCP: 高度健全性監視
```c
// Phase 9.2: TCP健全性監視実装
typedef struct {
    uint32_t send_times_ms[8];        // 移動平均ウィンドウ
    uint32_t moving_avg_ms;           // 移動平均
    uint8_t  consecutive_spikes;      // 連続スパイク数
    bool     degradation_alert;       // 劣化警告
    bool     preventive_reconnect_needed; // 予防的再接続
} wifi_health_monitor_t;

// スパイク検出条件
bool is_tcp_spike(uint32_t send_time_ms, uint32_t moving_avg_ms)
{
    return (send_time_ms > (moving_avg_ms * 3)) ||   // 相対スパイク
           (send_time_ms > 1000);                    // 絶対スパイク
}

// 予防的再接続判断
bool wifi_needs_preventive_reconnect(void)
{
    return (g_wifi_health.consecutive_spikes >= 2) &&
           (g_wifi_health.degradation_alert == true);
}
```

### 健全性監視効果比較

| 項目 | USB CDC-ACM | WiFi TCP (Phase 9.1) | WiFi TCP (Phase 9.2) |
|------|-------------|---------------------|---------------------|
| **切断予測** | 不要 | 不可 | 可能 ⭐ |
| **ダウンタイム** | 0秒 | ~30秒 | ~3秒 ⭐ |
| **監視オーバーヘッド** | 0% | 0% | <1% |
| **復旧自動化** | 不要 | 手動 | 自動 ⭐ |

## 使い分け指針

### 開発段階での選択

#### Phase 1〜2 (プロトタイプ): USB推奨 ✅
```
理由:
- デバッグ容易性
- 100%信頼性
- 設定簡単
- 高性能

用途:
- アルゴリズム開発
- 性能測定
- 機能検証
```

#### Phase 3〜7 (機能拡張): USB継続推奨 ✅
```
理由:
- 安定した性能基盤
- 新機能開発時の変数削減
- エラー原因切り分け容易

移行条件:
- 基本機能確立後
- WiFi機能必要時のみ
```

#### Phase 8以降 (運用準備): WiFi移行検討 ⚠️
```
理由:
- 実運用環境での無線要求
- ケーブルレス運用価値

前提条件:
- Phase 9.2健全性監視実装
- 十分な安定性確認
- 運用監視体制整備
```

### 用途別推奨方式

#### 開発・テスト環境: USB CDC-ACM ✅
```
推奨理由:
✅ 100%信頼性
✅ 低レイテンシ
✅ デバッグ容易性
✅ 設定簡単

必要条件:
- PC近接配置可能
- ケーブル接続許容
- 高い信頼性要求
```

#### 本格運用環境: WiFi TCP (Phase 9.2) ⚠️
```
推奨理由:
✅ ケーブルレス自由度
✅ 遠隔監視可能
✅ 複数台管理可能
✅ 予防的再接続 (Phase 9.2)

必要条件:
- WiFi環境安定
- 多少の性能劣化許容
- 運用監視体制
- Phase 9.2実装
```

#### デモ・展示: WiFi TCP優先 ✅
```
推奨理由:
✅ 見栄えの良さ
✅ 可動性デモ可能
✅ ケーブル不要

注意点:
⚠️ 性能劣化あり
⚠️ 接続安定性要確認
⚠️ バックアップ計画必要
```

## 設定・運用指針

### USB CDC-ACM設定

#### Linux (WSL2含む)
```bash
# デバイス確認
ls -la /dev/ttyACM*

# TTY Raw Mode設定 (重要!)
sudo stty -F /dev/ttyACM0 raw -echo 115200

# 権限設定
sudo chmod 666 /dev/ttyACM0

# usbipd設定 (WSL2)
usbipd wsl attach --busid X-X --distribution Ubuntu-20.04
```

#### Windows
```cmd
# デバイスマネージャーでCOMポート確認
# レジストリ設定でバッファサイズ調整推奨

mode COM3: BAUD=115200 PARITY=n DATA=8 STOP=1
```

### WiFi TCP設定

#### Spresense側
```c
// WiFi接続設定
#define WIFI_SSID        "YourSSID"
#define WIFI_PASSWORD    "YourPassword"
#define TCP_SERVER_PORT  8888
#define TCP_BUFFER_SIZE  250000  // GS2200M内部バッファ

// Phase 9.2健全性監視設定
#define TCP_HEALTH_MONITORING    true
#define TCP_SPIKE_THRESHOLD      3      // 移動平均の3倍
#define TCP_CONSECUTIVE_LIMIT    2      // 連続スパイク上限
#define PREVENTIVE_RECONNECT     true   // 予防的再接続有効
```

#### PC側
```rust
// Rust設定例
const TCP_SERVER_ADDR: &str = "192.168.1.100:8888";
const CONNECTION_TIMEOUT: Duration = Duration::from_secs(10);
const RECV_BUFFER_SIZE: usize = 128 * 1024;  // 128KB受信バッファ
const HEALTH_MONITORING: bool = true;         // 健全性監視有効
```

## 性能監視・最適化

### 監視メトリクス

#### 共通メトリクス
```c
typedef struct {
    uint32_t packets_sent;           // 送信パケット数
    uint32_t packets_received;       // 受信パケット数
    uint32_t packet_loss_count;      // パケットロス数
    float    packet_loss_rate;       // パケットロス率
    uint32_t avg_latency_ms;         // 平均レイテンシ
    uint32_t max_latency_ms;         // 最大レイテンシ
    float    effective_bandwidth_mbps; // 実効帯域
} transport_metrics_t;
```

#### USB固有メトリクス
```c
typedef struct {
    uint32_t usb_write_errors;       // USB書き込みエラー数
    uint32_t tty_configuration_errors; // TTY設定エラー
    bool     raw_mode_configured;    // Raw Mode設定状況
    uint32_t avg_transfer_time_ms;   // 平均転送時間
} usb_specific_metrics_t;
```

#### WiFi固有メトリクス (Phase 9.2) ⭐
```c
typedef struct {
    uint32_t tcp_send_timeouts;         // TCP送信タイムアウト数
    uint32_t tcp_health_spikes;         // 健全性スパイク数
    uint32_t preventive_reconnects;     // 予防的再接続数
    uint32_t traditional_reconnects;    // 従来型再接続数
    uint32_t connection_failures;       // 接続失敗数
    float    health_moving_avg_ms;      // 健全性移動平均
    bool     health_degradation_alert;  // 劣化警告状況
} wifi_specific_metrics_t;
```

### 最適化指針

#### USB最適化
```c
// USB転送最適化設定
#define USB_BULK_TRANSFER_SIZE    (64 * 1024)  // 64KB bulk転送
#define USB_TRANSFER_TIMEOUT_MS   1000         // 1秒タイムアウト
#define USB_QUEUE_DEPTH          3            // キュー深度

// DMA転送有効化 (可能な場合)
#define USB_DMA_ENABLED          true
#define USB_ZERO_COPY_ENABLED    true         // ゼロコピー転送
```

#### WiFi最適化 (Phase 9.2)
```c
// TCP健全性最適化設定
#define TCP_WINDOW_SIZE_BYTES      (32 * 1024)  // 32KB TCPウィンドウ
#define TCP_NODELAY_ENABLED        true         // Nagleアルゴリズム無効
#define TCP_KEEPALIVE_ENABLED      true         // Keep-alive有効
#define TCP_HEALTH_WINDOW_SIZE     8            // 健全性監視ウィンドウ
#define PREVENTIVE_RECONNECT_DELAY_MS  100      // 再接続遅延
```

## 障害対応・トラブルシューティング

### USB CDC-ACM障害対応

#### よくある問題
1. **TTY Canonical Mode**: バイナリデータ破損
   - **解決**: `stty raw -echo`設定
   - **検証**: `stty -F /dev/ttyACM0 -a`で確認

2. **権限不足**: `/dev/ttyACM0`アクセス不可
   - **解決**: `sudo chmod 666 /dev/ttyACM0`
   - **恒久対応**: udevルール設定

3. **WSL2 USB転送**: デバイス認識不可
   - **解決**: usbipd経由でUSBデバイス共有
   - **確認**: `lsusb`でデバイス表示

#### 診断手順
```bash
# 1. デバイス存在確認
ls -la /dev/ttyACM*

# 2. TTY設定確認
stty -F /dev/ttyACM0 -a | grep -E "(raw|echo)"

# 3. 権限確認
ls -la /dev/ttyACM0

# 4. 通信テスト
echo "test" > /dev/ttyACM0
```

### WiFi TCP障害対応

#### GS2200M固有問題 (Phase 9.2以前)
1. **突然のRST送信**: リソース枯渇による強制切断
   - **検出**: send()時間2000ms超過
   - **対応**: Phase 9.2予防的再接続
   - **回避**: 送信間隔調整

2. **接続確立失敗**: TCP 3-way handshake失敗
   - **原因**: GS2200M内部状態異常
   - **対応**: モジュールリセット
   - **予防**: 接続前状態確認

#### Phase 9.2診断機能 ⭐
```c
// TCP健全性診断
void diagnose_tcp_health(void)
{
    LOG_INFO("=== TCP Health Diagnosis ===");
    LOG_INFO("Moving average: %d ms", g_tcp_health.moving_avg_ms);
    LOG_INFO("Consecutive spikes: %d", g_tcp_health.consecutive_spikes);
    LOG_INFO("Total spikes: %d", g_tcp_health.total_spikes);
    LOG_INFO("Degradation alert: %s",
             g_tcp_health.degradation_alert ? "YES" : "NO");
    LOG_INFO("Preventive reconnect needed: %s",
             g_tcp_health.preventive_reconnect_needed ? "YES" : "NO");

    // 推奨アクション
    if (g_tcp_health.degradation_alert) {
        LOG_WARN("Recommendation: Execute preventive reconnect");
    } else if (g_tcp_health.consecutive_spikes > 0) {
        LOG_INFO("Recommendation: Monitor closely");
    } else {
        LOG_INFO("Recommendation: Normal operation");
    }
}
```

## 将来拡張・ロードマップ

### Phase 10: USB High Speed対応
```c
// USB 3.0/High Speed (480 Mbps)対応
#define USB_HIGH_SPEED_ENABLED       true
#define USB_HIGH_SPEED_BANDWIDTH     (480 * 1000 * 1000)  // 480Mbps
#define TARGET_FPS_USB_HIGH_SPEED    60                    // 60fps目標

// 期待効果: VGA 30fps → 60fps実現
```

### Phase 11: WiFi最適化
```c
// GS2200M代替モジュール検討
#define WIFI_MODULE_NEXT_GEN         "ESP32-S3"
#define EXPECTED_TCP_EFFICIENCY      50                     // 0.5% → 50%
#define TARGET_FPS_WIFI_OPTIMIZED    25                     // 25fps目標
```

### Phase 12: ハイブリッド通信
```c
// USB + WiFi併用通信
typedef struct {
    bool    usb_primary;           // USB主系
    bool    wifi_backup;           // WiFi副系
    bool    auto_failover;         // 自動切替
    uint32_t failover_threshold_ms; // 切替閾値
} hybrid_transport_t;

// 期待効果: 高信頼性 + 無線柔軟性
```

**Phase 9.2対応トランスポート比較による最適選択指針の確立** ✅