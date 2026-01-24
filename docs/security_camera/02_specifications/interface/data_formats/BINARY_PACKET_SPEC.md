# バイナリパケット仕様

**バージョン**: 2.0 (Phase 9.2対応)
**日付**: 2026-01-22
**エンディアン**: Little Endian
**アライメント**: 自然アライメント (4byte境界)

## 概要

SpresenseとPC間通信で使用するバイナリパケット形式の詳細仕様。エンディアン、アライメント、パディング、構造体レイアウトを厳密に定義し、プラットフォーム間の互換性を保証。

## 基本データ型定義

### プリミティブ型
```c
// Spresense (ARM Cortex-M4) / PC (x86_64) 共通定義
typedef uint8_t   u8;     // 1 byte, unsigned
typedef uint16_t  u16;    // 2 bytes, unsigned
typedef uint32_t  u32;    // 4 bytes, unsigned
typedef uint64_t  u64;    // 8 bytes, unsigned
typedef int8_t    i8;     // 1 byte, signed
typedef int16_t   i16;    // 2 bytes, signed
typedef int32_t   i32;    // 4 bytes, signed
typedef int64_t   i64;    // 8 bytes, signed
typedef float     f32;    // 4 bytes, IEEE 754
typedef double    f64;    // 8 bytes, IEEE 754
```

### エンディアン規則

#### Little Endian統一 ✅
```
採用理由:
- Spresense (ARM Cortex-M4): Little Endian
- PC (x86_64): Little Endian
- 変換不要 → 性能向上

マルチバイト値の格納例:
u32 value = 0x12345678
Memory layout: [0x78][0x56][0x34][0x12]
               LSB              MSB
```

#### エンディアン変換不要の確認
```c
// 両端がLittle Endianのため変換関数不使用
// NG: htonl(), ntohl(), htons(), ntohs()
// OK: 直接コピー

u32 sequence = 12345;
memcpy(packet->sequence_field, &sequence, sizeof(u32));  // 直接コピー
```

## 構造体アライメント

### 自然アライメント原則
```c
// 各フィールドは自然境界にアライメント
struct aligned_example {
    u8  field1;     // offset 0, アライメント 1
    u8  padding1;   // offset 1, パディング
    u16 field2;     // offset 2, アライメント 2
    u32 field3;     // offset 4, アライメント 4
    u8  field4;     // offset 8, アライメント 1
    u8  padding2[3];// offset 9-11, パディング
    // 構造体サイズ: 12 bytes (4byte境界)
};
```

### パケット構造体設計

#### packed属性使用方針
```c
// パケット構造体は__attribute__((packed))必須
typedef struct __attribute__((packed)) {
    u32 sync_word;      // 4 bytes, offset 0
    u32 sequence;       // 4 bytes, offset 4
    u32 size;           // 4 bytes, offset 8
    u8  data[];         // Variable, offset 12
    // u16 crc16;       // 2 bytes, offset 12+N
} mjpeg_packet_t;

// packed無しの場合のパディング例:
typedef struct {
    u32 sync_word;      // 4 bytes, offset 0
    u32 sequence;       // 4 bytes, offset 4
    u32 size;           // 4 bytes, offset 8
    u8  data[];         // Variable, offset 12
    // コンパイラーによる自動パディングでサイズ変動の危険
};
```

## パケット形式詳細

### MJPEGフレームパケット (0xCAFEBABE)

#### 構造体定義
```c
typedef struct __attribute__((packed)) {
    u32 sync_word;      // 0xCAFEBABE (Little Endian)
    u32 sequence;       // フレーム番号 (0から開始)
    u32 size;           // JPEGデータサイズ (bytes)
    u8  jpeg_data[];    // 可変長JPEGデータ
    // u16 crc16;       // CRC (jpeg_data直後に配置)
} mjpeg_packet_t;
```

#### メモリレイアウト
```
Offset  Field           Size    Value Example
------  -----           ----    -------------
0x00    sync_word       4       0xCAFEBABE
0x04    sequence        4       0x00000001
0x08    size           4       0x0000C800 (51200 bytes)
0x0C    jpeg_data      N       [JPEG binary data]
0x0C+N  crc16          2       0x29B1
```

#### サイズ計算
```c
#define MJPEG_HEADER_SIZE    12  // sync_word + sequence + size
#define MJPEG_CRC_SIZE       2   // crc16
#define MJPEG_OVERHEAD_SIZE  (MJPEG_HEADER_SIZE + MJPEG_CRC_SIZE)  // 14 bytes

size_t calculate_mjpeg_packet_size(u32 jpeg_size)
{
    return MJPEG_HEADER_SIZE + jpeg_size + MJPEG_CRC_SIZE;
}
```

### メトリクスパケット (0xCAFEBEEF) - Phase 9.2対応 ⭐

#### 構造体定義
```c
typedef struct __attribute__((packed)) {
    u32 sync_word;                    // 0xCAFEBEEF
    u32 frames_sent;                  // 送信フレーム数
    u32 frames_dropped;               // ドロップフレーム数
    u32 avg_jpeg_size;                // 平均JPEGサイズ
    u32 camera_fps;                   // カメラFPS (x100)
    u32 camera_latency_ms;            // カメラレイテンシ
    u32 packet_processing_ms;         // パケット処理時間
    u32 usb_write_ms;                 // USB書き込み時間
    u32 memory_usage_kb;              // メモリ使用量
    u32 buffer_queue_count;           // バッファキュー数
    u32 timestamp_ms;                 // タイムスタンプ
    u32 total_bandwidth_kbps;         // 総帯域幅
    u32 error_count;                  // エラー数
    u32 tcp_health_moving_avg_ms;     // TCP健全性移動平均 ⭐ Phase 9.2
    u32 tcp_health_total_spikes;      // TCP健全性総スパイク数 ⭐ Phase 9.2
    u16 crc16;                        // CRC-16-CCITT
} metrics_packet_t;

// コンパイル時サイズ検証
static_assert(sizeof(metrics_packet_t) == 58, "Metrics packet size must be 58 bytes");
```

#### フィールドエンコーディング詳細

**FPS表現 (x100スケール)**:
```c
// 浮動小数点回避のための整数エンコーディング
u32 encode_fps(float fps_value)
{
    return (u32)(fps_value * 100.0f);
}

float decode_fps(u32 encoded_fps)
{
    return (float)encoded_fps / 100.0f;
}

// 例: 11.5 fps → 1150 として格納
```

**タイムスタンプ (ms)**:
```c
// システム起動からの経過時間 (32bit制限)
u32 get_timestamp_ms(void)
{
    return (u32)(get_system_time_us() / 1000);
}
// 注意: 約49日でオーバーフロー (2^32 ms)
```

### バッチフレームパケット (0xCAFEBABF) - Phase 7.2a

#### 可変長構造
```c
typedef struct __attribute__((packed)) {
    u32 sync_word;          // 0xCAFEBABF
    u32 frame_count;        // バッチ内フレーム数
    u32 total_size;         // 全フレーム合計サイズ
    // 可変長フレームデータ部
    // frame1: [size:4][jpeg_data:N1]
    // frame2: [size:4][jpeg_data:N2]
    // ...
    // u16 crc16;           // 最後にCRC
} batch_packet_t;
```

#### フレームデータ構造
```c
typedef struct __attribute__((packed)) {
    u32 frame_size;         // 個別フレームサイズ
    u8  frame_data[];       // JPEGデータ
} batch_frame_t;
```

## メモリレイアウト検証

### 構造体サイズ検証
```c
// コンパイル時検証マクロ
#define VERIFY_STRUCT_SIZE(type, expected_size) \
    static_assert(sizeof(type) == expected_size, \
                 #type " size mismatch: expected " #expected_size)

VERIFY_STRUCT_SIZE(mjpeg_packet_t, 12);    // ヘッダー部のみ
VERIFY_STRUCT_SIZE(metrics_packet_t, 58);  // Phase 9.2: 50→58 bytes
VERIFY_STRUCT_SIZE(batch_packet_t, 12);    // ヘッダー部のみ
```

### アライメント検証
```c
void verify_packet_alignment(void)
{
    // フィールドオフセット検証
    assert(offsetof(mjpeg_packet_t, sync_word) == 0);
    assert(offsetof(mjpeg_packet_t, sequence) == 4);
    assert(offsetof(mjpeg_packet_t, size) == 8);

    // Phase 9.2拡張フィールド検証
    assert(offsetof(metrics_packet_t, tcp_health_moving_avg_ms) == 48);
    assert(offsetof(metrics_packet_t, tcp_health_total_spikes) == 52);
    assert(offsetof(metrics_packet_t, crc16) == 56);

    LOG_INFO("Packet alignment verification passed");
}
```

### エンディアン検証
```c
bool verify_endianness(void)
{
    u32 test_value = 0x12345678;
    u8 *bytes = (u8 *)&test_value;

    // Little Endian確認
    bool is_little_endian = (bytes[0] == 0x78 && bytes[1] == 0x56 &&
                            bytes[2] == 0x34 && bytes[3] == 0x12);

    if (!is_little_endian) {
        LOG_ERROR("Platform is not Little Endian - packet format incompatible");
        return false;
    }

    LOG_INFO("Little Endian verified");
    return true;
}
```

## プラットフォーム互換性

### コンパイラー差分対応

#### GCC (Spresense)
```c
// NuttX/GCC環境
#ifdef __GNUC__
    #define PACKED __attribute__((packed))
    #define ALIGNED(n) __attribute__((aligned(n)))
#endif

typedef struct PACKED {
    u32 field1;
    u16 field2;
} gcc_packet_t;
```

#### MSVC/Clang (PC)
```c
// Windows/MSVC環境
#ifdef _MSC_VER
    #define PACKED
    #pragma pack(push, 1)
    typedef struct {
        u32 field1;
        u16 field2;
    } msvc_packet_t;
    #pragma pack(pop)
#endif
```

#### Rust (PC側受信)
```rust
// Rust側での同等定義
#[repr(C, packed)]
pub struct MjpegPacket {
    pub sync_word: u32,
    pub sequence: u32,
    pub size: u32,
    // jpeg_data and crc16 handled separately
}

#[repr(C, packed)]
pub struct MetricsPacket {
    pub sync_word: u32,
    pub frames_sent: u32,
    // ... 既存フィールド ...
    // Phase 9.2拡張
    pub tcp_health_moving_avg_ms: u32,
    pub tcp_health_total_spikes: u32,
    pub crc16: u16,
}

impl MetricsPacket {
    pub const SIZE: usize = 58;  // Phase 9.2
}
```

## データ転送最適化

### ゼロコピー転送
```c
// パケット構築時のメモリコピー最小化
int mjpeg_build_packet_zerocopy(u8 *buffer, u32 sequence,
                               const u8 *jpeg_data, u32 jpeg_size)
{
    mjpeg_packet_t *packet = (mjpeg_packet_t *)buffer;

    // ヘッダー直接書き込み (コピー不要)
    packet->sync_word = MJPEG_SYNC_WORD;
    packet->sequence = sequence;
    packet->size = jpeg_size;

    // JPEGデータ: ポインター操作のみ (既に適切な位置にある場合)
    // memcpy(&packet->jpeg_data[0], jpeg_data, jpeg_size);  // 通常必要

    // CRC計算・配置
    u16 crc = crc16_calc((u8*)&packet->sequence, 8 + jpeg_size);
    *(u16*)&packet->jpeg_data[jpeg_size] = crc;

    return MJPEG_HEADER_SIZE + jpeg_size + MJPEG_CRC_SIZE;
}
```

### DMA転送対応
```c
// DMA転送用アライメント要件
#define DMA_ALIGNMENT    32  // ARM Cortex-M4 cache line

typedef struct __attribute__((aligned(DMA_ALIGNMENT))) {
    u8 packet_buffer[MAX_PACKET_SIZE];
} dma_buffer_t;

static dma_buffer_t g_dma_buffers[3] __attribute__((aligned(DMA_ALIGNMENT)));
```

## エラー検出・復旧

### 構造体破損検出
```c
typedef enum {
    PACKET_VALIDATE_OK = 0,
    PACKET_VALIDATE_SIZE_MISMATCH = 1,
    PACKET_VALIDATE_ALIGNMENT_ERROR = 2,
    PACKET_VALIDATE_SYNC_WORD_INVALID = 3,
    PACKET_VALIDATE_CRC_MISMATCH = 4,
} packet_validate_result_t;

packet_validate_result_t validate_mjpeg_packet(const u8 *buffer, size_t buffer_size)
{
    if (buffer_size < MJPEG_HEADER_SIZE) {
        return PACKET_VALIDATE_SIZE_MISMATCH;
    }

    const mjpeg_packet_t *packet = (const mjpeg_packet_t *)buffer;

    // アライメント検証
    if ((uintptr_t)packet % 4 != 0) {
        return PACKET_VALIDATE_ALIGNMENT_ERROR;
    }

    // 同期ワード検証
    if (packet->sync_word != MJPEG_SYNC_WORD) {
        return PACKET_VALIDATE_SYNC_WORD_INVALID;
    }

    // サイズ整合性検証
    size_t expected_size = MJPEG_HEADER_SIZE + packet->size + MJPEG_CRC_SIZE;
    if (buffer_size != expected_size) {
        return PACKET_VALIDATE_SIZE_MISMATCH;
    }

    // CRC検証
    u16 expected_crc = crc16_calc((u8*)&packet->sequence, 8 + packet->size);
    u16 actual_crc = *(u16*)&packet->jpeg_data[packet->size];
    if (expected_crc != actual_crc) {
        return PACKET_VALIDATE_CRC_MISMATCH;
    }

    return PACKET_VALIDATE_OK;
}
```

### 自動復旧機能
```c
int recover_corrupted_packet(u8 *buffer, size_t buffer_size)
{
    // 同期ワード検索による復旧試行
    for (size_t i = 0; i < buffer_size - 4; i++) {
        u32 *sync_candidate = (u32 *)&buffer[i];

        if (*sync_candidate == MJPEG_SYNC_WORD ||
            *sync_candidate == METRICS_SYNC_WORD ||
            *sync_candidate == BATCH_SYNC_WORD) {

            // パケット境界発見 - 再整列
            memmove(buffer, &buffer[i], buffer_size - i);
            LOG_INFO("Packet realigned: offset %zu", i);
            return buffer_size - i;
        }
    }

    LOG_ERROR("Unable to recover packet - no sync word found");
    return -1;
}
```

## 性能プロファイリング

### パケット処理時間測定
```c
typedef struct {
    u32 total_packets_processed;
    u64 total_processing_time_us;
    u32 max_processing_time_us;
    u32 min_processing_time_us;
    u64 total_bytes_processed;
} packet_performance_stats_t;

void profile_packet_processing(const u8 *buffer, size_t size)
{
    u64 start_us = get_timestamp_us();

    // パケット処理実行
    packet_validate_result_t result = validate_mjpeg_packet(buffer, size);

    u64 end_us = get_timestamp_us();
    u32 processing_time_us = (u32)(end_us - start_us);

    // 統計更新
    g_packet_stats.total_packets_processed++;
    g_packet_stats.total_processing_time_us += processing_time_us;
    g_packet_stats.total_bytes_processed += size;

    if (processing_time_us > g_packet_stats.max_processing_time_us) {
        g_packet_stats.max_processing_time_us = processing_time_us;
    }

    if (g_packet_stats.min_processing_time_us == 0 ||
        processing_time_us < g_packet_stats.min_processing_time_us) {
        g_packet_stats.min_processing_time_us = processing_time_us;
    }
}
```

### メモリ使用量最適化
```c
// パケットバッファ使用量監視
typedef struct {
    size_t buffer_size;
    size_t bytes_used;
    float  utilization_percent;
} buffer_utilization_t;

buffer_utilization_t get_buffer_utilization(void)
{
    buffer_utilization_t util;
    util.buffer_size = MAX_PACKET_SIZE;
    util.bytes_used = get_current_packet_size();
    util.utilization_percent = (float)util.bytes_used / util.buffer_size * 100.0f;

    return util;
}
```

## 将来拡張

### パケットバージョニング
```c
// 将来のプロトコル拡張に備えたバージョニング
typedef struct __attribute__((packed)) {
    u32 sync_word;          // 0xCAFEBABE
    u8  version_major;      // メジャーバージョン
    u8  version_minor;      // マイナーバージョン
    u16 reserved;           // 将来使用
    u32 sequence;
    u32 size;
    u8  data[];
} versioned_packet_t;
```

### 圧縮パケット対応
```c
// パケットレベル圧縮 (Phase 3.0想定)
typedef struct __attribute__((packed)) {
    u32 sync_word;          // 0xCAFECAFE (圧縮パケット識別)
    u32 compressed_size;    // 圧縮後サイズ
    u32 original_size;      // 圧縮前サイズ
    u8  compression_type;   // 圧縮アルゴリズム (LZ4, zstd等)
    u8  reserved[3];
    u8  compressed_data[];
} compressed_packet_t;
```

**Phase 9.2対応バイナリパケット仕様による完全なプラットフォーム互換性保証** ✅