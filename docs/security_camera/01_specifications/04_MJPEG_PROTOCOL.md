# MJPEG ストリーミングプロトコル仕様

**バージョン**: 1.0
**日付**: 2025-12-16
**目的**: Spresense から PC への MJPEG フレーム転送

## プロトコル概要

### 設計方針

1. **シンプル**: ヘッダー + データのみ
2. **堅牢**: 同期ワード、シーケンス番号、チェックサムで信頼性確保
3. **効率的**: 最小限のオーバーヘッド
4. **拡張可能**: 将来の機能追加に対応

### フレームフォーマット

```
┌────────────────────────────────────────────────────────────┐
│                    MJPEG Frame Packet                      │
├──────────┬──────────┬──────────┬───────────────┬──────────┤
│  HEADER  │   SEQ    │   SIZE   │  JPEG DATA    │ CHECKSUM │
│ (4 bytes)│ (4 bytes)│ (4 bytes)│  (variable)   │ (2 bytes)│
└──────────┴──────────┴──────────┴───────────────┴──────────┘
   0xCAFE      uint32     uint32      N bytes       uint16
   BABE                                              CRC-16
```

**総サイズ**: 14 bytes (header) + N bytes (JPEG data)

## フィールド詳細

### 1. HEADER (4 bytes)

**目的**: フレーム同期

```c
#define MJPEG_SYNC_WORD  0xCAFEBABE  // マジックナンバー
```

- **値**: `0xCAFEBABE` (固定)
- **エンディアン**: Little Endian
- **バイト配列**: `[0xBE, 0xBA, 0xFE, 0xCA]`

**用途**:
- ストリーム内でフレーム境界を識別
- データ破損検出
- 再同期ポイント

### 2. SEQUENCE (4 bytes)

**目的**: フレーム順序管理

```c
uint32_t sequence_number;  // 0 から開始、1ずつ増加
```

- **範囲**: 0 ～ 0xFFFFFFFF
- **オーバーフロー**: 0 に戻る (wrap around)
- **用途**:
  - フレームドロップ検出
  - 順序保証
  - フレームレート計算

### 3. SIZE (4 bytes)

**目的**: JPEG データ長

```c
uint32_t jpeg_size;  // JPEG データのバイト数
```

- **範囲**: 1 ～ 524,288 (512 KB max)
- **実測値**: 5,000 ～ 15,000 bytes (QVGA)
- **用途**:
  - メモリ割り当て
  - データ完全性確認
  - バッファ管理

### 4. JPEG DATA (可変長)

**目的**: JPEG 画像データ

- **フォーマット**: JPEG (JFIF)
- **開始マーカー**: `0xFF 0xD8` (SOI)
- **終了マーカー**: `0xFF 0xD9` (EOI)
- **サイズ**: SIZE フィールドで指定

**JPEG 構造**:
```
0xFF 0xD8       (SOI - Start of Image)
0xFF 0xE0       (APP0 - JFIF marker)
...             (JPEG segments)
0xFF 0xDA       (SOS - Start of Scan)
...             (Compressed image data)
0xFF 0xD9       (EOI - End of Image)
```

### 5. CHECKSUM (2 bytes)

**目的**: データ完全性検証

```c
uint16_t crc16;  // CRC-16-CCITT
```

- **アルゴリズム**: CRC-16-CCITT (polynomial 0x1021)
- **初期値**: 0xFFFF
- **対象範囲**: HEADER + SEQ + SIZE + JPEG_DATA
- **用途**:
  - データ破損検出
  - 転送エラー検出

**CRC-16-CCITT 計算**:
```c
uint16_t crc16_ccitt(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < len; i++)
    {
        crc ^= (uint16_t)data[i] << 8;

        for (int j = 0; j < 8; j++)
        {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc = crc << 1;
        }
    }

    return crc;
}
```

## パケット例

### サンプルフレーム (5,824 bytes JPEG)

```
Offset  | Field      | Value (hex)        | Value (dec) | Description
--------|------------|--------------------|-------------|------------------
0x0000  | HEADER     | BE BA FE CA        |             | Sync word
0x0004  | SEQ        | 1E 00 00 00        | 30          | Frame #30
0x0008  | SIZE       | C0 16 00 00        | 5,824       | JPEG size
0x000C  | JPEG_DATA  | FF D8 FF E0 ...    |             | JPEG data (5,824 bytes)
0x16CC  | CHECKSUM   | 3A 2F              | 0x2F3A      | CRC-16
--------|------------|--------------------|-------------|------------------
Total size: 5,838 bytes (14 bytes overhead + 5,824 bytes data)
```

**オーバーヘッド**: 14 bytes / 5,838 bytes = **0.24%**

## 通信フロー

### 送信側 (Spresense)

```
1. カメラから JPEG フレーム取得
   ↓
2. シーケンス番号インクリメント
   ↓
3. パケット構築:
   - HEADER: 0xCAFEBABE
   - SEQ: sequence_number++
   - SIZE: jpeg_size
   - DATA: jpeg_data
   - CRC: calculate(HEADER + SEQ + SIZE + DATA)
   ↓
4. USB CDC 経由で送信
   ↓
5. 次のフレームへ
```

### 受信側 (PC)

```
1. USB CDC からデータ読み取り
   ↓
2. SYNC_WORD を探す (0xCAFEBABE)
   ↓
3. ヘッダー解析:
   - SEQ, SIZE を読み取り
   ↓
4. JPEG データ受信 (SIZE bytes)
   ↓
5. CHECKSUM 受信・検証
   ↓
6. CRC 一致?
   Yes → JPEG デコード・表示
   No  → エラーログ・スキップ
   ↓
7. 次のフレームへ
```

## エラーハンドリング

### 1. 同期ワード不一致

**検出**: HEADER != 0xCAFEBABE

**対処**:
```c
// 1バイトずつスキャンして次の SYNC_WORD を探す
while (read_byte() != 0xBE);
while (read_byte() != 0xBA);
while (read_byte() != 0xFE);
while (read_byte() != 0xCA);
// 再同期完了
```

### 2. シーケンス番号ジャンプ

**検出**: `new_seq != expected_seq`

**対処**:
```c
if (new_seq != last_seq + 1)
{
    uint32_t dropped = new_seq - last_seq - 1;
    log_warn("Dropped %u frames (last=%u, new=%u)",
             dropped, last_seq, new_seq);
    // フレームドロップをカウント・ログ
}
last_seq = new_seq;
```

### 3. CRC エラー

**検出**: `calculated_crc != received_crc`

**対処**:
```c
if (calc_crc != recv_crc)
{
    log_error("CRC mismatch (calc=0x%04X, recv=0x%04X)",
              calc_crc, recv_crc);
    // フレーム破棄
    // エラーカウント増加
    // 次のフレームへ
}
```

### 4. サイズ異常

**検出**: `size > MAX_JPEG_SIZE` または `size == 0`

**対処**:
```c
if (size == 0 || size > 524288)
{
    log_error("Invalid JPEG size: %u bytes", size);
    // フレーム破棄
    // 再同期試行
}
```

## パフォーマンス

### 帯域幅計算

**QVGA @ 30fps**:
- 平均 JPEG サイズ: 5,800 bytes
- オーバーヘッド: 14 bytes
- フレームあたり: 5,814 bytes
- **帯域幅**: 5,814 × 30 = **174,420 bytes/s** ≈ **1.4 Mbps**

**USB CDC (Full Speed)**:
- 理論最大: 12 Mbps
- **使用率**: 1.4 / 12 = **11.7%**
- **余裕**: 十分

### レイテンシ

**送信側 (Spresense)**:
- パケット構築: < 1 ms
- CRC 計算: < 1 ms
- USB 送信: 5,814 bytes @ 12 Mbps ≈ 3.9 ms
- **合計**: < 6 ms

**受信側 (PC)**:
- USB 受信: ≈ 4 ms
- CRC 検証: < 1 ms
- JPEG デコード: 1-2 ms (ハードウェアアクセラレーション)
- **合計**: < 7 ms

**エンドツーエンド**: < 13 ms (**78 fps 可能**)

## 実装ガイド

### Spresense 側 (C)

```c
typedef struct {
    uint32_t sync_word;      // 0xCAFEBABE
    uint32_t sequence;       // Frame sequence number
    uint32_t size;           // JPEG data size
    uint8_t  data[];         // Flexible array (JPEG data + CRC)
} __attribute__((packed)) mjpeg_packet_t;

int mjpeg_send_frame(int usb_fd, const uint8_t *jpeg_data,
                     uint32_t jpeg_size, uint32_t *seq)
{
    // パケットバッファ割り当て
    size_t packet_size = sizeof(mjpeg_packet_t) + jpeg_size + 2;
    uint8_t *packet = malloc(packet_size);

    // ヘッダー構築
    mjpeg_packet_t *pkt = (mjpeg_packet_t *)packet;
    pkt->sync_word = 0xCAFEBABE;
    pkt->sequence = (*seq)++;
    pkt->size = jpeg_size;

    // JPEG データコピー
    memcpy(pkt->data, jpeg_data, jpeg_size);

    // CRC 計算
    uint16_t crc = crc16_ccitt(packet,
                                sizeof(mjpeg_packet_t) + jpeg_size);
    memcpy(pkt->data + jpeg_size, &crc, 2);

    // USB 送信
    ssize_t sent = write(usb_fd, packet, packet_size);

    free(packet);
    return (sent == packet_size) ? 0 : -1;
}
```

### PC 側 (Rust)

```rust
#[repr(C, packed)]
struct MjpegHeader {
    sync_word: u32,  // 0xCAFEBABE
    sequence: u32,   // Frame number
    size: u32,       // JPEG size
}

fn receive_frame(port: &mut SerialPort) -> Result<Vec<u8>> {
    // ヘッダー読み取り
    let mut header = MjpegHeader::default();
    port.read_exact(as_bytes_mut(&mut header))?;

    // 同期ワード確認
    if header.sync_word != 0xCAFEBABE {
        return Err(Error::SyncError);
    }

    // JPEG データ読み取り
    let mut jpeg_data = vec![0u8; header.size as usize];
    port.read_exact(&mut jpeg_data)?;

    // CRC 読み取り・検証
    let mut crc_bytes = [0u8; 2];
    port.read_exact(&mut crc_bytes)?;
    let recv_crc = u16::from_le_bytes(crc_bytes);

    let calc_crc = crc16_ccitt(&header, &jpeg_data);
    if recv_crc != calc_crc {
        return Err(Error::CrcError);
    }

    Ok(jpeg_data)
}
```

## 将来の拡張

### バージョン 2.0 候補機能

1. **メタデータ追加**
   - タイムスタンプ (microseconds)
   - 露出情報
   - センサーデータ

2. **圧縮品質制御**
   - JPEG 品質パラメータ
   - 動的品質調整

3. **複数ストリーム**
   - メインストリーム (HD)
   - サブストリーム (QVGA)

4. **双方向通信**
   - コマンド送信 (PC → Spresense)
   - 応答受信 (Spresense → PC)

## まとめ

このプロトコルは:

✅ **シンプル**: 14 bytes の固定オーバーヘッド
✅ **高速**: 1.4 Mbps (USB の 11.7% 使用)
✅ **堅牢**: CRC + 同期ワード + シーケンス番号
✅ **効率的**: 0.24% のオーバーヘッド
✅ **拡張可能**: 将来の機能追加に対応

次のステップ: 実装!
