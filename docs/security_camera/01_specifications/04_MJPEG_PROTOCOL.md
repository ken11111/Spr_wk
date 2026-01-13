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

---

## Phase 7.2a: マルチフレームバッチングプロトコル拡張

**バージョン**: 1.1 (Phase 7.2a)
**日付**: 2026-01-08
**目的**: TCP転送効率向上のため、複数フレームを1パケットにバッチ化

### 背景と目的

Phase 7のWiFi/TCP実装において、usrsock TCPスタックのコンテキストスイッチオーバーヘッドがボトルネックとなり、FPSが3.6 fpsに低下しました（目標15-25 fps）。

**問題点**:
- usrsockスタック: 4回のコンテキストスイッチ（app→kernel→daemon→SPI→module）
- 各フレーム送信に233ms（うち100ms以上がusrsockオーバーヘッド）
- 単一フレーム送信では効率が悪い

**解決策**:
複数フレーム（1-3フレーム）を1つのバッチパケットにまとめることで、usrsockコールを削減し、TCP送信効率を向上させる。

**期待効果**:
- FPS: 3.6 fps → 8-9 fps (+122% ~ +150%)
- usrsockコール: フレーム毎 → バッチ毎（-66%削減）
- TCP送信時間: 233ms/フレーム → 350ms/3フレーム

### バッチパケットフォーマット

```
┌─────────────────────────────────────────────────────────────┐
│                  Batch Packet Structure                      │
├─────────────────────────────────────────────────────────────┤
│ Batch Header (16 bytes)                                      │
├──────────┬──────────┬──────────┬──────────┐                 │
│ SYNC     │ BATCH_SEQ│FRAME_CNT │TOTAL_SIZE│                 │
│(4 bytes) │(4 bytes) │(4 bytes) │(4 bytes) │                 │
├──────────┴──────────┴──────────┴──────────┤                 │
│ 0xCAFE     uint32     uint32     uint32    │                 │
│ BABF    (batch seq) (1-3)    (sum of JPEGs)│                 │
└──────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│ Frame 1 Metadata (8 bytes)                                   │
├──────────┬──────────┐                                        │
│FRAME_SEQ │FRAME_SIZE│                                        │
│(4 bytes) │(4 bytes) │                                        │
├──────────┴──────────┤                                        │
│  uint32    uint32   │                                        │
└───────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│ Frame 1 JPEG Data (FRAME_SIZE bytes)                        │
│ [0xFF 0xD8 ... 0xFF 0xD9]                                   │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│ Frame 2 Metadata (8 bytes)                                   │
│ ... (same structure)                                         │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│ Frame 2 JPEG Data (FRAME_SIZE bytes)                        │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│ ... (Frame 3 if exists)                                      │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│ CRC16 (2 bytes)                                              │
│ CRC-16-CCITT over entire packet (header to last JPEG byte)  │
└─────────────────────────────────────────────────────────────┘
```

**総サイズ**: 16 (header) + Σ(8 + JPEG_SIZE) + 2 (CRC)

### バッチヘッダーフィールド詳細

#### 1. SYNC_WORD (4 bytes)

**目的**: バッチパケット識別

```c
#define MJPEG_BATCH_SYNC_WORD  0xCAFEBABF  // バッチパケット
```

- **値**: `0xCAFEBABF` (固定、末尾が0xBFで単一フレームと区別)
- **エンディアン**: Little Endian
- **バイト配列**: `[0xBF, 0xBA, 0xFE, 0xCA]`

**既存プロトコルとの区別**:
- `0xCAFEBABE`: 単一フレーム（既存）
- `0xCAFEBABF`: バッチフレーム（新規、Phase 7.2a）
- `0xCAFEBEEF`: メトリクスパケット（Phase 4.1）

#### 2. BATCH_SEQUENCE (4 bytes)

**目的**: バッチパケットのシーケンス番号

- **型**: `uint32_t`
- **範囲**: 0 ~ 2^32-1
- **初期値**: 0
- **動作**: バッチ送信毎にインクリメント（+1）

**注意**: 個別フレームのシーケンス番号とは別管理

#### 3. FRAME_COUNT (4 bytes)

**目的**: このバッチに含まれるフレーム数

- **型**: `uint32_t`
- **範囲**: 1 ~ 3
- **妥当性チェック**: 受信側で範囲外の場合はエラー

#### 4. TOTAL_SIZE (4 bytes)

**目的**: 全フレームのJPEG dataの合計サイズ

- **型**: `uint32_t`
- **計算**: Σ(FRAME_SIZE[i]) for i=0 to FRAME_COUNT-1
- **最大値**: ~180,000 bytes (3フレーム × 60KB)

### フレームメタデータフィールド

各フレームのメタデータ（8バイト）:

#### 1. FRAME_SEQUENCE (4 bytes)

**目的**: 個別フレームのシーケンス番号

- **型**: `uint32_t`
- **管理**: Camera threadで管理される元のフレームシーケンス番号
- **用途**: フレーム順序保証、ロス検出

#### 2. FRAME_SIZE (4 bytes)

**目的**: このフレームのJPEG dataサイズ

- **型**: `uint32_t`
- **範囲**: 1 ~ 61,440 bytes (60KB、Phase 7.2メモリ最適化後)
- **用途**: JPEG data境界の特定

### JPEG データ

各フレームのJPEG data:
- **サイズ**: FRAME_SIZE bytes
- **フォーマット**: 標準JPEG（SOI: 0xFF 0xD8、EOI: 0xFF 0xD9）
- **検証**: Phase 4.1.1のJPEG validation適用済み

### CRC16

**目的**: パケット全体の完全性検証

- **範囲**: Batch header ~ 最後のJPEG byte（CRC自身は除外）
- **アルゴリズム**: CRC-16-CCITT (Polynomial 0x1021, Init 0xFFFF)
- **サイズ**: 2 bytes

### パケットサイズ計算

**最小サイズ**（1フレーム、1KB JPEG）:
```
16 + 8 + 1024 + 2 = 1,050 bytes
```

**標準サイズ**（3フレーム、各47KB JPEG）:
```
16 + (8 + 47000) × 3 + 2 = 141,042 bytes ≈ 138 KB
```

**最大サイズ**（3フレーム、各60KB JPEG）:
```
16 + (8 + 61440) × 3 + 2 = 184,370 bytes ≈ 180 KB
```

### プロトコル定数

```c
/* Phase 7.2a: Multi-frame batching constants */
#define MJPEG_BATCH_SYNC_WORD    0xCAFEBABF  /* Batched frames */
#define MJPEG_BATCHING_ENABLED   1           /* 0: disabled, 1: enabled */
#define MJPEG_BATCH_SIZE         3           /* Number of frames per batch (1-3) */
#define MJPEG_BATCH_TIMEOUT_MS   100         /* Timeout for partial batch (ms) */
#define MJPEG_BATCH_HEADER_SIZE  16          /* batch header size */
#define MJPEG_FRAME_META_SIZE    8           /* per-frame metadata size */
#define MJPEG_MAX_BATCH_PACKET   (MJPEG_BATCH_HEADER_SIZE + \
                                  (MJPEG_FRAME_META_SIZE + MJPEG_MAX_JPEG_SIZE) * MJPEG_BATCH_SIZE + \
                                  MJPEG_CRC_SIZE)  /* Max: ~185 KB */
```

### データ構造（C言語）

```c
/* Batch packet header */
typedef struct mjpeg_batch_header_s
{
  uint32_t sync_word;        /* 0xCAFEBABF */
  uint32_t batch_sequence;   /* Batch sequence number */
  uint32_t frame_count;      /* Number of frames (1-3) */
  uint32_t total_size;       /* Total JPEG data size */
} __attribute__((packed)) mjpeg_batch_header_t;

/* Frame metadata within batch */
typedef struct mjpeg_frame_meta_s
{
  uint32_t frame_sequence;   /* Individual frame sequence */
  uint32_t frame_size;       /* JPEG data size */
} __attribute__((packed)) mjpeg_frame_meta_t;

/* Complete batch packet structure */
typedef struct mjpeg_batch_packet_s
{
  mjpeg_batch_header_t header;
  uint8_t data[];            /* [meta1][jpeg1][meta2][jpeg2]...[crc] */
} __attribute__((packed)) mjpeg_batch_packet_t;
```

### バッチング動作仕様

#### 収集ロジック（Spresense側）

1. **フレーム収集**:
   - Action queueから最大3フレームを収集
   - `pthread_cond_timedwait()`で100msタイムアウト
   - タイムアウト時は部分バッチ（1-2フレーム）を送信

2. **バッチパケット作成**:
   - `mjpeg_pack_batch()`でパケット化
   - CRC16計算（全データ）
   - 最大パケットサイズチェック

3. **送信**:
   - TCP/USB経由でバッチパケット送信
   - 送信後、全バッファをempty queueへ返却

#### パース処理（PC側）

1. **Sync word検出**:
   - `0xCAFEBABF`を検出してバッチパケット識別

2. **ヘッダー読み込み**:
   - バッチヘッダー（16バイト）読み込み
   - `frame_count`, `total_size`取得

3. **フレーム読み込み**:
   - 各フレームのメタデータ（8バイト）読み込み
   - JPEG data読み込み
   - ループを`frame_count`回繰り返し

4. **CRC検証**:
   - 全データのCRC16計算
   - パケット末尾のCRC16と比較

5. **フレーム処理**:
   - 各フレームを個別に処理（表示/保存）

### エラーハンドリング

#### Spresense側

1. **バッチパック失敗**:
   - 全バッファを即座にempty queueへ返却
   - エラーカウント更新

2. **TCP送信失敗**:
   - 接続切断検出（ENOTCONN, ECONNRESET, EPIPE）
   - バッチ内全バッファを返却後シャットダウン

3. **連続エラー**:
   - 10回連続エラーで自動シャットダウン

#### PC側

1. **パケット読み込みエラー**:
   - タイムアウト、接続切断検出
   - リトライまたは終了

2. **バッチパース失敗**:
   - ヘッダー検証失敗、CRC不一致
   - パケット破棄、次のsync wordから再同期

3. **フレーム検証失敗**:
   - JPEG マーカー（SOI/EOI）検証失敗
   - 警告ログ、フレームスキップ

### パフォーマンス特性

#### TCP送信時間削減

**単一フレームモード（既存）**:
```
1フレーム送信 = 233ms
├─ usrsockオーバーヘッド: ~100ms (4回コンテキストスイッチ)
├─ GS2200M SPI転送:       ~80ms
└─ TCPプロトコル:          ~53ms

FPS = 1000ms / 233ms = 4.3 fps
```

**バッチングモード（Phase 7.2a）**:
```
3フレームバッチ送信 = 350ms (推定)
├─ usrsockオーバーヘッド: ~100ms (1回のみ！)
├─ GS2200M SPI転送:       ~180ms (3倍)
└─ TCPプロトコル:          ~70ms (1.3倍)

実効FPS = 3 × (1000ms / 350ms) = 8.6 fps
```

**改善率**:
- FPS: +138% (3.6 fps → 8.6 fps)
- usrsockコール: -66% (フレーム毎 → バッチ毎)
- コンテキストスイッチ: -66%

#### メモリ使用量

**バッチバッファ**: ~185 KB (一時バッファ、送信後即解放)
**キュー深度影響**: Phase 7.2で5バッファに削減（-120KB）

### 後方互換性

バッチングは**完全に後方互換**:

| Sync Word | パケットタイプ | 処理 |
|-----------|----------------|------|
| `0xCAFEBABE` | 単一フレーム | 既存コードで処理 ✅ |
| `0xCAFEBABF` | バッチフレーム | 新コードで処理 ✅ |
| `0xCAFEBEEF` | メトリクス | 既存コードで処理 ✅ |

PC側は3種類のパケットを自動判別して処理。

### 設定オプション

```c
/* mjpeg_protocol.h */
#define MJPEG_BATCHING_ENABLED   1  /* 0: 無効化して単一フレームモードに戻す */
#define MJPEG_BATCH_SIZE         3  /* 1-3: バッチサイズ調整 */
#define MJPEG_BATCH_TIMEOUT_MS   100 /* タイムアウト調整（ms） */
```

### 実装ファイル

#### Spresense側

- `apps/examples/security_camera/mjpeg_protocol.h`: 構造体定義、定数
- `apps/examples/security_camera/mjpeg_protocol.c`: `mjpeg_pack_batch()`実装
- `apps/examples/security_camera/camera_threads.c`: バッチング収集ロジック

#### PC側（Rust）

- `src/protocol.rs`: `BatchPacket`構造体、`BatchPacket::parse()`
- `src/tcp_connection.rs`: バッチパケット読み込み
- `src/main.rs`: バッチパケット処理

### テスト仕様

#### 機能テスト

1. **バッチ送受信**:
   - 3フレームバッチの正常送受信
   - 部分バッチ（1-2フレーム）の送受信

2. **CRC検証**:
   - 正常パケットのCRC検証成功
   - 破損パケットのCRC検証失敗検出

3. **エラーハンドリング**:
   - TCP切断時の正常終了
   - パケット破損時の再同期

#### 性能テスト

1. **FPS測定**:
   - 目標: 8-9 fps達成確認
   - 測定期間: 最低5分間

2. **メモリ使用量**:
   - バッチバッファのメモリ影響確認
   - 640KB予算内の確認

3. **エラー率**:
   - バッチパケット成功率 > 95%
   - CRC検証成功率 > 99%

### 制限事項

1. **バッチサイズ**: 最大3フレーム（メモリ制約）
2. **パケットサイズ**: 最大~185KB（TCP MTUより大きい、分割送信される）
3. **遅延**: バッチ待機により最大100ms遅延追加
4. **メモリ**: バッチバッファ~185KB必要

### まとめ

Phase 7.2aマルチフレームバッチングプロトコルは:

✅ **高効率**: usrsockコール -66%削減
✅ **高速化**: FPS +138%向上（3.6 → 8.6 fps目標）
✅ **後方互換**: 既存パケットと共存可能
✅ **堅牢**: CRC16検証、完全なエラーハンドリング
✅ **柔軟**: バッチサイズ、タイムアウト調整可能

**次のステップ**: 実機テストで性能検証!
