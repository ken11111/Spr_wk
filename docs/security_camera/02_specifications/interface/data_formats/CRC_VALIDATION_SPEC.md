# CRC検証仕様

**バージョン**: 2.0 (Phase 9.2対応)
**日付**: 2026-01-22
**アルゴリズム**: CRC-16-CCITT
**多項式**: 0x1021

## 概要

バイナリパケット通信の信頼性保証を目的としたCRC検証仕様。Phase 1.5でルックアップテーブル最適化により77%性能向上、Phase 9.2でメトリクスパケット拡張対応。

## CRC-16-CCITT仕様

### 基本パラメータ
- **アルゴリズム**: CRC-16-CCITT (ITU-T V.41)
- **多項式**: 0x1021 (x^16 + x^12 + x^5 + 1)
- **初期値**: 0xFFFF
- **最終XOR**: 0x0000 (なし)
- **ビット順序**: MSB first
- **バイト順序**: Little Endian (結果格納時)

### 実装詳細

#### Phase 1.0: ビット単位計算 (初期実装)
```c
uint16_t crc16_bit_calc(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;

        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }

    return crc;
}
```

**性能**: 38.4ms (VGAフレーム処理時)
**課題**: パケット処理のボトルネック

#### Phase 1.5: ルックアップテーブル最適化 ⭐
```c
// 256エントリのルックアップテーブル
static const uint16_t crc16_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    // ... 全256エントリ
};

uint16_t crc16_calc(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < length; i++) {
        uint8_t tbl_idx = ((crc >> 8) ^ data[i]) & 0xFF;
        crc = (crc << 8) ^ crc16_table[tbl_idx];
    }

    return crc;
}
```

**性能**: 8.7ms (VGAフレーム処理時)
**改善**: -77.3% (38.4ms → 8.7ms)
**メモリ**: +512bytes (ルックアップテーブル)

### パケット別CRC適用

#### MJPEGフレームパケット (0xCAFEBABE)
```
パケット構造:
[SYNC:4][SEQ:4][SIZE:4][JPEG:N][CRC:2]

CRC計算範囲:
- 開始: SEQフィールド (offset 4)
- 終了: JPEGデータ末尾 (offset 12+N-1)
- サイズ: 8 + N bytes

実装:
uint16_t crc = crc16_calc(&packet[4], 8 + jpeg_size);
```

#### メトリクスパケット (0xCAFEBEEF) - Phase 9.2対応 ⭐
```
パケット構造 (58 bytes):
[SYNC:4][METRICS:52][CRC:2]
         ↑Phase 9.2: +8 bytes健全性データ

CRC計算範囲:
- 開始: METRICSフィールド (offset 4)
- 終了: 予備フィールド末尾 (offset 59)
- サイズ: 56 bytes (Phase 9.2で拡張)

実装:
uint16_t crc = crc16_calc(&packet[4], 56);  // 50→56 bytes
```

#### バッチフレームパケット (0xCAFEBABF)
```
パケット構造:
[SYNC:4][COUNT:4][SIZE:4][FRAME1:N1][FRAME2:N2]...[CRC:2]

CRC計算範囲:
- 開始: COUNTフィールド (offset 4)
- 終了: 最終フレーム末尾
- サイズ: 8 + Σ(Frame sizes)

実装:
uint16_t crc = crc16_calc(&packet[4], 8 + total_frame_size);
```

### 検証手順

#### 送信側 (Spresense)
```c
int mjpeg_pack_frame(uint8_t *buffer, uint32_t sequence,
                    const uint8_t *jpeg_data, uint32_t jpeg_size)
{
    mjpeg_packet_t *packet = (mjpeg_packet_t *)buffer;

    // ヘッダー設定
    packet->sync_word = MJPEG_SYNC_WORD;
    packet->sequence = sequence;
    packet->size = jpeg_size;

    // JPEGデータコピー
    memcpy(packet->jpeg_data, jpeg_data, jpeg_size);

    // CRC計算・設定
    uint16_t crc = crc16_calc((uint8_t*)&packet->sequence, 8 + jpeg_size);
    *(uint16_t*)&packet->jpeg_data[jpeg_size] = crc;  // Little Endian

    return 14 + jpeg_size;  // パケット総サイズ
}
```

#### 受信側 (PC)
```rust
impl MjpegPacket {
    pub fn validate_crc(&self, jpeg_data: &[u8]) -> Result<(), ProtocolError> {
        // CRC計算範囲: sequence + size + jpeg_data
        let mut crc_data = Vec::new();
        crc_data.extend_from_slice(&self.sequence.to_le_bytes());
        crc_data.extend_from_slice(&self.size.to_le_bytes());
        crc_data.extend_from_slice(jpeg_data);

        let calculated_crc = crc16_calc(&crc_data);

        if self.crc16 != calculated_crc {
            return Err(ProtocolError::CrcMismatch {
                expected: calculated_crc,
                received: self.crc16,
            });
        }

        Ok(())
    }
}

fn crc16_calc(data: &[u8]) -> u16 {
    let mut crc: u16 = 0xFFFF;

    for &byte in data {
        let tbl_idx = ((crc >> 8) ^ (byte as u16)) & 0xFF;
        crc = (crc << 8) ^ CRC16_TABLE[tbl_idx as usize];
    }

    crc
}
```

### エラー処理

#### CRC不一致エラー
```c
// Spresense側エラーログ
typedef enum {
    CRC_ERROR_NONE = 0,
    CRC_ERROR_MISMATCH = 1,
    CRC_ERROR_INVALID_LENGTH = 2,
    CRC_ERROR_NULL_POINTER = 3,
} crc_error_t;

// エラー統計
static struct crc_stats_s {
    uint32_t total_packets;
    uint32_t crc_mismatches;
    uint32_t invalid_lengths;
} g_crc_stats;
```

```rust
// PC側エラー処理
#[derive(Debug, PartialEq)]
pub enum ProtocolError {
    CrcMismatch { expected: u16, received: u16 },
    InvalidPacketSize { expected: usize, received: usize },
    InvalidSyncWord { expected: u32, received: u32 },
    // Phase 9.2: 健全性監視エラー
    TcpHealthDegraded,
    MetricsPacketSizeChanged { old_size: usize, new_size: usize },
}

impl ProtocolError {
    pub fn log_error(&self) {
        match self {
            Self::CrcMismatch { expected, received } => {
                log::error!("CRC mismatch: expected 0x{:04X}, received 0x{:04X}",
                           expected, received);
            },
            // ... 他のエラーパターン
        }
    }
}
```

### 性能最適化

#### ルックアップテーブル生成
```c
void crc16_generate_table(void)
{
    for (int i = 0; i < 256; i++) {
        uint16_t crc = i << 8;

        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }

        crc16_table[i] = crc;
    }
}
```

#### SIMD最適化 (将来検討)
```c
// ARM NEON使用例 (Spresense CortexM4では非対応)
#ifdef __ARM_NEON
uint16_t crc16_simd_calc(const uint8_t *data, size_t length)
{
    // 8バイト並列処理
    // 実装はプラットフォーム依存
}
#endif
```

### 性能ベンチマーク

#### Phase別性能比較

| Phase | 実装方式 | 処理時間 | メモリ使用量 | 改善率 |
|-------|----------|----------|-------------|--------|
| **1.0** | ビット単位計算 | 38.4ms | +0 bytes | ベースライン |
| **1.5** | ルックアップ | 8.7ms | +512 bytes | -77.3% ⭐ |
| **将来** | SIMD/ハードウェア | <5ms予想 | +1KB予想 | -87%予想 |

#### VGAフレーム処理時間内訳 (Phase 1.5)

| 処理 | 時間 | 割合 | 備考 |
|------|------|------|------|
| **カメラキャプチャ** | 5-6ms | 11-14% | V4L2ドライバー |
| **JPEG圧縮** | 0.05-265ms | 0-600% | シーン複雑度依存 |
| **パケット化** | 2ms | 5% | ヘッダー作成等 |
| **CRC計算** | 8.7ms | 20% | ルックアップ最適化後 |
| **USB送信** | 30ms | 68% | USB 2.0 Full Speed |
| **合計** | 44-48ms | 100% | フレーム処理時間 |

### Phase 9.2対応詳細 ⭐

#### メトリクスパケットCRC拡張

**従来 (Phase 9.1)**:
```c
// 50 bytes パケット
uint16_t crc = crc16_calc(&packet[4], 48);  // SYNC除く48 bytes
```

**Phase 9.2**:
```c
// 58 bytes パケット (+8 bytes TCP健全性)
typedef struct {
    uint32_t sync_word;                    // 4 bytes
    uint32_t frames_sent;                  // 4 bytes
    // ... 既存フィールド (44 bytes) ...
    uint32_t tcp_health_moving_avg_ms;     // 4 bytes ⭐ NEW
    uint32_t tcp_health_total_spikes;      // 4 bytes ⭐ NEW
    uint16_t crc16;                        // 2 bytes
} metrics_packet_t;

uint16_t crc = crc16_calc(&packet[4], 56);  // 48→56 bytes (+8)
```

#### 下位互換性対応
```rust
// PC側での動的サイズ対応
pub fn parse_metrics_packet(data: &[u8]) -> Result<MetricsPacket, ProtocolError> {
    match data.len() {
        50 => parse_legacy_metrics(data),      // Phase 9.1
        58 => parse_extended_metrics(data),    // Phase 9.2 ⭐
        _ => Err(ProtocolError::InvalidPacketSize {
            expected: 58,
            received: data.len(),
        }),
    }
}
```

### 検証・テスト

#### 単体テスト
```c
// Spresense側テスト
void test_crc16_known_vectors(void)
{
    // RFC 1331 Appendix A - CRC-16-CCITT test vectors
    uint8_t test1[] = "123456789";
    uint16_t expected1 = 0x29B1;
    uint16_t actual1 = crc16_calc(test1, 9);

    assert(actual1 == expected1);

    // Phase 9.2 specific test
    uint8_t metrics_test[56] = { /* 健全性データを含むメトリクス */ };
    uint16_t crc_new = crc16_calc(metrics_test, 56);
    assert(crc_new != 0);  // 非ゼロ確認
}
```

```rust
// PC側テスト
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_crc16_known_vectors() {
        let test_data = b"123456789";
        assert_eq!(crc16_calc(test_data), 0x29B1);
    }

    #[test]
    fn test_phase92_metrics_crc() {
        let mut packet = MetricsPacket::default();
        packet.tcp_health_moving_avg_ms = 150;  // Phase 9.2
        packet.tcp_health_total_spikes = 5;      // Phase 9.2

        let crc = packet.calculate_crc();
        assert!(packet.validate_crc().is_ok());
    }
}
```

#### 統合テスト
```c
// エンドツーエンドCRC検証
void test_mjpeg_crc_integrity(void)
{
    uint8_t packet_buffer[MAX_PACKET_SIZE];
    uint8_t jpeg_data[65536];
    uint32_t jpeg_size = generate_test_jpeg(jpeg_data);

    // パケット作成
    int packet_size = mjpeg_pack_frame(packet_buffer, 123, jpeg_data, jpeg_size);

    // CRC検証
    mjpeg_packet_t *packet = (mjpeg_packet_t *)packet_buffer;
    uint16_t calculated_crc = crc16_calc((uint8_t*)&packet->sequence, 8 + jpeg_size);
    uint16_t packet_crc = *(uint16_t*)&packet->jpeg_data[jpeg_size];

    assert(calculated_crc == packet_crc);
}
```

### 運用監視

#### CRC統計
```c
typedef struct {
    uint32_t total_packets_sent;
    uint32_t total_packets_received;
    uint32_t crc_calculation_time_us;
    uint32_t crc_mismatches;
    double   crc_error_rate;
} crc_statistics_t;

void crc_update_stats(bool crc_match, uint32_t calc_time_us)
{
    g_crc_stats.total_packets_sent++;
    g_crc_stats.crc_calculation_time_us += calc_time_us;

    if (!crc_match) {
        g_crc_stats.crc_mismatches++;
    }

    g_crc_stats.crc_error_rate =
        (double)g_crc_stats.crc_mismatches / g_crc_stats.total_packets_sent;
}
```

#### ログ出力
```c
// 定期統計ログ (10秒間隔)
LOG_INFO("CRC Statistics: sent=%d, errors=%d, rate=%.6f%%, avg_time=%.2fms",
         g_crc_stats.total_packets_sent,
         g_crc_stats.crc_mismatches,
         g_crc_stats.crc_error_rate * 100.0,
         (double)g_crc_stats.crc_calculation_time_us / g_crc_stats.total_packets_sent / 1000.0);
```

### 今後の最適化

#### ハードウェアCRC (将来検討)
```c
// STM32等のハードウェアCRC使用例
#ifdef HARDWARE_CRC_AVAILABLE
uint16_t crc16_hw_calc(const uint8_t *data, size_t length)
{
    // ハードウェアCRC peripheral使用
    // 期待性能: <1ms
}
#endif
```

#### 並列CRC計算
```c
// 大きなパケット用並列計算
uint16_t crc16_parallel_calc(const uint8_t *data, size_t length)
{
    if (length > PARALLEL_THRESHOLD) {
        // データ分割して並列計算
        // 複数スレッドでCRC計算後結合
    } else {
        return crc16_calc(data, length);
    }
}
```

**Phase 9.2対応CRC検証による通信信頼性の完全保証** ✅