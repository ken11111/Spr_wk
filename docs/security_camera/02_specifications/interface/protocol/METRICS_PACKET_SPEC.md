# メトリクスパケット仕様

**バージョン**: 2.0 (Phase 9.2対応)
**日付**: 2026-01-22
**Phase対応**: 9.2 TCP健全性監視
**Sync Word**: 0xCAFEBEEF
**パケットサイズ**: 58 bytes (Phase 9.2で50→58bytes拡張)

## 概要

SpresenseからPCへの統計情報転送プロトコル。Phase 9.2でTCP健全性監視機能が追加され、GS2200M資源枯渇予測のためのメトリクスを拡張。

## パケット構造

```
┌────────────────────────────────────────────────────────────┐
│                    Metrics Packet (58 bytes)               │
├──────────┬─────────────────────────────────────┬──────────┤
│  HEADER  │           METRICS DATA              │ CHECKSUM │
│ (4 bytes)│            (52 bytes)               │ (2 bytes)│
└──────────┴─────────────────────────────────────┴──────────┘
   0xCAFE      統計情報 (Phase 9.2拡張)            uint16
   BEEF                                           CRC-16
```

**総サイズ**: 4 + 52 + 2 = **58 bytes** (Phase 9.2拡張)

## フィールド詳細

### ヘッダー (4 bytes)
```c
#define METRICS_SYNC_WORD  0xCAFEBEEF
```

### メトリクスデータ (52 bytes)

| オフセット | フィールド名 | サイズ | 説明 | Phase |
|-----------|------------|-------|------|-------|
| 0 | frames_sent | 4 | 送信フレーム数 | 1.0 |
| 4 | frames_dropped | 4 | ドロップフレーム数 | 1.0 |
| 8 | avg_jpeg_size | 4 | 平均JPEGサイズ(bytes) | 1.0 |
| 12 | camera_fps | 4 | カメラFPS (x100) | 1.5 |
| 16 | camera_latency_ms | 4 | カメラレイテンシ(ms) | 1.5 |
| 20 | packet_processing_ms | 4 | パケット処理時間(ms) | 1.5 |
| 24 | usb_write_ms | 4 | USB書き込み時間(ms) | 1.5 |
| 28 | memory_usage_kb | 4 | メモリ使用量(KB) | 1.5 |
| 32 | buffer_queue_count | 4 | バッファキュー数 | 1.5 |
| 36 | timestamp_ms | 4 | タイムスタンプ(ms) | 1.5 |
| 40 | total_bandwidth_kbps | 4 | 総帯域幅(Kbps) | 1.5 |
| 44 | error_count | 4 | エラー数 | 1.5 |
| **48** | **tcp_health_moving_avg_ms** | **4** | **TCP健全性移動平均(ms)** ⭐ | **9.2** |
| **52** | **tcp_health_total_spikes** | **4** | **TCP健全性総スパイク数** ⭐ | **9.2** |

### CRC-16 (2 bytes)
- **アルゴリズム**: CRC-16-CCITT
- **多項式**: 0x1021
- **CRC範囲**: 56 bytes (SYNC除く52bytes統計 + 2bytes予備 + 2bytesCRC前まで)

## Phase 9.2拡張内容 ⭐

### TCP健全性監視フィールド

#### tcp_health_moving_avg_ms
- **目的**: TCP送信時間の移動平均 (8サンプル)
- **単位**: ミリ秒
- **用途**: スパイク検出のベースライン
- **正常値**: 100-350ms
- **警告値**: >1000ms

#### tcp_health_total_spikes
- **目的**: スパイク検出総回数
- **単位**: カウント
- **リセット**: 再起動時
- **用途**: GS2200M資源枯渇パターン分析

### スパイク検出ロジック

```c
// スパイク検出条件
bool is_spike = (send_time_ms > (moving_avg * 3)) || (send_time_ms > 1000);

// 連続スパイク監視
if (consecutive_spikes >= 2) {
    degradation_alert = true;
    // 予防的再接続トリガー
}
```

### 予防的再接続

1. **検出**: 連続スパイク≥2回
2. **判断**: `tcp_health_should_reconnect()` = true
3. **実行**: フレーム送信前に再接続
4. **復旧**: 成功時にTCP健全性リセット

## 送信仕様

### 送信タイミング
- **頻度**: 10フレームごと
- **条件**: カメラアクティブ時のみ
- **優先度**: フレーム送信より低優先

### エラー処理
- **CRC不一致**: パケット破棄、error_count増加
- **同期ワード不一致**: パケット破棄
- **送信失敗**: 再送なし (次回送信時に最新値送信)

## 実装詳細

### Spresense側 (送信)

```c
// mjpeg_protocol.c:L156
int mjpeg_pack_metrics(uint8_t *buffer,
                      const performance_stats_t *stats,
                      uint32_t tcp_health_moving_avg_ms,    // Phase 9.2
                      uint32_t tcp_health_total_spikes)     // Phase 9.2
{
    metrics_packet_t *packet = (metrics_packet_t *)buffer;

    packet->sync_word = METRICS_SYNC_WORD;
    packet->frames_sent = stats->frames_sent;
    // ... 既存フィールド ...

    // Phase 9.2: TCP健全性フィールド追加
    packet->tcp_health_moving_avg_ms = tcp_health_moving_avg_ms;
    packet->tcp_health_total_spikes = tcp_health_total_spikes;

    // CRC計算: 56 bytes範囲
    packet->crc16 = crc16_calc((uint8_t*)&packet->frames_sent, 56);

    return METRICS_PACKET_SIZE; // 58 bytes
}
```

### PC側 (受信)

```rust
// protocol.rs:L89
#[repr(C)]
pub struct MetricsPacket {
    pub sync_word: u32,
    pub frames_sent: u32,
    // ... 既存フィールド ...

    // Phase 9.2: TCP健全性フィールド
    pub tcp_health_moving_avg_ms: u32,
    pub tcp_health_total_spikes: u32,

    pub crc16: u16,
}

impl MetricsPacket {
    pub fn parse(data: &[u8]) -> Result<Self, ProtocolError> {
        if data.len() != 58 {  // Phase 9.2: 50→58
            return Err(ProtocolError::InvalidSize);
        }

        let packet = unsafe {
            std::ptr::read(data.as_ptr() as *const MetricsPacket)
        };

        // CRC検証: 56 bytes
        let expected_crc = crc16_calc(&data[4..60]);
        if packet.crc16 != expected_crc {
            return Err(ProtocolError::CrcMismatch);
        }

        Ok(packet)
    }
}
```

## 期待効果

### Phase 9.2での改善

| 項目 | Phase 9.1 (事後対応) | Phase 9.2 (予防対応) |
|------|---------------------|---------------------|
| **切断検出** | RST受信後 | 送信時間急増時 |
| **対応時間** | 30秒後 | 数秒以内 |
| **ダウンタイム** | ~30秒 | ~数秒 |
| **改善率** | - | 95%削減期待 |
| **監視情報** | なし | リアルタイム健全性 |

### 運用メリット

1. **予防的監視**: 障害前の早期検出
2. **可視化**: PC側でTCP健全性リアルタイム表示
3. **分析**: 健全性履歴のCSV記録
4. **最適化**: GS2200M資源使用パターン分析

## 互換性

### 下位互換性
- **Phase 9.1以前**: 58bytesパケットを50bytesとして処理
- **エラー**: パケットサイズ不一致でエラー
- **対策**: PC側でパケットサイズ判定による自動対応

### 上位互換性
- **将来拡張**: 追加フィールドは末尾に追加
- **バージョニング**: sync_word変更による識別
- **移行**: 段階的移行サポート

**Phase 9.2 TCP健全性監視による予防的再接続の実現** ✅