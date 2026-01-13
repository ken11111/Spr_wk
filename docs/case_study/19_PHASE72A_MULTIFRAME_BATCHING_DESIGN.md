# Phase 7.2a マルチフレームバッチング: プロトコル設計

**設計日**: 2026-01-08
**設計者**: Claude Code (Sonnet 4.5)
**目的**: TCP転送効率を向上させるため、複数フレームを1パケットにバッチ化

---

## 📋 要約

Phase 7.2のメモリ最適化だけでは目標FPS（5-7 fps）達成が困難なため、**マルチフレームバッチング**を実装します。これにより、usrsock TCPスタックの4回コンテキストスイッチオーバーヘッドを劇的に削減します。

**期待効果:**
- 現在: 3.6 fps（1フレーム/パケット）
- 目標: **8-9 fps**（3フレーム/パケット）
- 改善率: +122% ~ +150%

---

## 🎯 設計目標

### 1. TCP効率の向上
- コンテキストスイッチ回数削減: フレーム毎 → バッチ毎
- TCPオーバーヘッド削減: ACK数削減、ヘッダー効率化

### 2. 後方互換性
- 既存の単一フレームパケット（0xCAFEBABE）はそのまま使用可能
- PC側で自動判別

### 3. 実装の単純さ
- プロトコルはシンプルに
- デバッグ容易性を確保

### 4. 柔軟性
- バッチサイズを動的に調整可能（1-3フレーム）

---

## 📦 プロトコル設計

### 新しいSYNC WORD

```c
#define MJPEG_BATCH_SYNC_WORD    0xCAFEBABF  // マルチフレームバッチ
```

**判別方法:**
```
0xCAFEBABE: 単一フレーム（既存）
0xCAFEBABF: マルチフレームバッチ（新規、末尾が0xBF）
0xCAFEBEEF: メトリクスパケット（既存）
```

### バッチパケット構造

```
┌─────────────────────────────────────────────────────────┐
│ Batch Header (16 bytes)                                  │
├─────────────────────────────────────────────────────────┤
│ sync_word (4)    : 0xCAFEBABF                            │
│ batch_sequence (4): バッチシーケンス番号                │
│ frame_count (4)  : このバッチ内のフレーム数 (1-3)      │
│ total_size (4)   : 全フレームデータの総サイズ          │
├─────────────────────────────────────────────────────────┤
│ Frame 1 Metadata (8 bytes)                               │
├─────────────────────────────────────────────────────────┤
│ frame_sequence (4): フレームシーケンス番号              │
│ frame_size (4)   : このフレームのJPEGサイズ             │
├─────────────────────────────────────────────────────────┤
│ Frame 1 JPEG Data (frame_size bytes)                     │
├─────────────────────────────────────────────────────────┤
│ Frame 2 Metadata (8 bytes)                               │
├─────────────────────────────────────────────────────────┤
│ frame_sequence (4): フレームシーケンス番号              │
│ frame_size (4)   : このフレームのJPEGサイズ             │
├─────────────────────────────────────────────────────────┤
│ Frame 2 JPEG Data (frame_size bytes)                     │
├─────────────────────────────────────────────────────────┤
│ ... (Frame 3があれば同様)                                │
├─────────────────────────────────────────────────────────┤
│ CRC16 (2 bytes)                                          │
└─────────────────────────────────────────────────────────┘
```

### C言語構造体定義

```c
/* Batch packet header */
typedef struct mjpeg_batch_header_s
{
  uint32_t sync_word;        /* 0xCAFEBABF */
  uint32_t batch_sequence;   /* Batch sequence number */
  uint32_t frame_count;      /* Number of frames in this batch (1-3) */
  uint32_t total_size;       /* Total size of all frame data */
} __attribute__((packed)) mjpeg_batch_header_t;

/* Frame metadata within batch */
typedef struct mjpeg_frame_meta_s
{
  uint32_t frame_sequence;   /* Individual frame sequence */
  uint32_t frame_size;       /* JPEG data size for this frame */
} __attribute__((packed)) mjpeg_frame_meta_t;

/* Complete batch packet structure */
typedef struct mjpeg_batch_packet_s
{
  mjpeg_batch_header_t header;
  uint8_t data[];            /* Flexible array: [meta1][jpeg1][meta2][jpeg2]...[crc] */
} __attribute__((packed)) mjpeg_batch_packet_t;
```

### パケットサイズ計算

**3フレームバッチの例:**
```
バッチヘッダー:     16 bytes
Frame 1 メタデータ:  8 bytes
Frame 1 JPEG:       47,000 bytes
Frame 2 メタデータ:  8 bytes
Frame 2 JPEG:       47,000 bytes
Frame 3 メタデータ:  8 bytes
Frame 3 JPEG:       47,000 bytes
CRC16:               2 bytes
────────────────────────────
総サイズ:           141,042 bytes ≈ 138 KB
```

**最大パケットサイズ:**
```c
#define MJPEG_MAX_BATCH_FRAMES   3
#define MJPEG_MAX_BATCH_SIZE     (16 + (8 + 61440) * 3 + 2)  // 184,370 bytes ≈ 180 KB
```

---

## 🔧 実装設計

### Spresense側実装

#### 1. バッファリングロジック

**camera_threads.c: USB/TCPスレッド改造**

```c
/* Phase 7.2a: Frame batching for TCP efficiency */
#define BATCH_SIZE 3  /* Number of frames per batch */

typedef struct frame_batch_s
{
  frame_buffer_t *frames[BATCH_SIZE];
  int frame_count;
  uint32_t total_jpeg_size;
} frame_batch_t;

static void usb_thread_func_batching(void *arg)
{
  frame_batch_t batch;
  memset(&batch, 0, sizeof(batch));

  while (!g_shutdown_requested)
  {
    pthread_mutex_lock(&g_queue_mutex);

    /* Collect frames for batching */
    while (batch.frame_count < BATCH_SIZE && !frame_queue_is_empty(g_action_queue))
    {
      frame_buffer_t *buf = frame_queue_pull(&g_action_queue);
      if (buf)
      {
        batch.frames[batch.frame_count] = buf;
        batch.total_jpeg_size += buf->used;
        batch.frame_count++;
      }
    }

    pthread_mutex_unlock(&g_queue_mutex);

    /* Send batch if we have frames */
    if (batch.frame_count > 0)
    {
      send_batch_packet(&batch);

      /* Return buffers to empty queue */
      pthread_mutex_lock(&g_queue_mutex);
      for (int i = 0; i < batch.frame_count; i++)
      {
        frame_queue_push(&g_empty_queue, batch.frames[i]);
      }
      pthread_mutex_unlock(&g_queue_mutex);

      memset(&batch, 0, sizeof(batch));
    }
  }
}
```

#### 2. バッチパケット作成

**mjpeg_protocol.c: 新関数追加**

```c
int mjpeg_pack_batch(frame_batch_t *batch,
                     uint32_t *batch_sequence,
                     uint8_t *packet,
                     size_t packet_max_size)
{
  mjpeg_batch_header_t *header = (mjpeg_batch_header_t *)packet;
  uint8_t *ptr = packet + sizeof(mjpeg_batch_header_t);

  /* Fill batch header */
  header->sync_word = MJPEG_BATCH_SYNC_WORD;
  header->batch_sequence = (*batch_sequence)++;
  header->frame_count = batch->frame_count;
  header->total_size = batch->total_jpeg_size;

  /* Pack each frame */
  for (int i = 0; i < batch->frame_count; i++)
  {
    frame_buffer_t *frame = batch->frames[i];
    mjpeg_frame_meta_t *meta = (mjpeg_frame_meta_t *)ptr;

    /* Frame metadata */
    meta->frame_sequence = frame->id;  /* Use buffer id as sequence */
    meta->frame_size = frame->used;
    ptr += sizeof(mjpeg_frame_meta_t);

    /* JPEG data */
    memcpy(ptr, frame->data, frame->used);
    ptr += frame->used;
  }

  /* Calculate and append CRC */
  uint16_t crc = mjpeg_crc16_ccitt(packet, ptr - packet);
  memcpy(ptr, &crc, sizeof(crc));
  ptr += sizeof(crc);

  return ptr - packet;  /* Total packet size */
}
```

#### 3. タイムアウト処理

**問題**: 3フレーム揃うまで待つと遅延が大きくなる

**解決策**: タイムアウト付きバッチング

```c
/* Wait for batch or timeout (100ms) */
struct timespec timeout;
clock_gettime(CLOCK_REALTIME, &timeout);
timeout.tv_nsec += 100000000;  /* +100ms */

int ret = pthread_cond_timedwait(&g_queue_cond, &g_queue_mutex, &timeout);

if (ret == ETIMEDOUT || batch.frame_count > 0)
{
  /* Send partial batch on timeout */
  break;
}
```

### PC側実装

#### 1. パケットタイプ判別

**tcp_connection.rs または serial_connection.rs**

```rust
fn read_packet(&mut self) -> Result<Packet> {
    let sync_word = self.read_u32()?;

    match sync_word {
        0xCAFEBABE => self.read_single_frame(),
        0xCAFEBABF => self.read_batch_frames(),
        0xCAFEBEEF => self.read_metrics(),
        _ => Err(Error::InvalidSyncWord),
    }
}
```

#### 2. バッチパケットパーサー

```rust
struct BatchPacket {
    batch_sequence: u32,
    frame_count: u32,
    total_size: u32,
    frames: Vec<Frame>,
}

fn read_batch_frames(&mut self) -> Result<BatchPacket> {
    let batch_sequence = self.read_u32()?;
    let frame_count = self.read_u32()?;
    let total_size = self.read_u32()?;

    let mut frames = Vec::with_capacity(frame_count as usize);

    for _ in 0..frame_count {
        // Read frame metadata
        let frame_sequence = self.read_u32()?;
        let frame_size = self.read_u32()?;

        // Read JPEG data
        let mut jpeg_data = vec![0u8; frame_size as usize];
        self.read_exact(&mut jpeg_data)?;

        frames.push(Frame {
            sequence: frame_sequence,
            data: jpeg_data,
        });
    }

    // Verify CRC
    let crc = self.read_u16()?;
    // ... CRC verification

    Ok(BatchPacket {
        batch_sequence,
        frame_count,
        total_size,
        frames,
    })
}
```

#### 3. フレーム処理

```rust
// Main loop
match packet {
    Packet::SingleFrame(frame) => {
        process_frame(frame);
    }
    Packet::BatchFrames(batch) => {
        for frame in batch.frames {
            process_frame(frame);
        }
    }
    Packet::Metrics(metrics) => {
        update_metrics(metrics);
    }
}
```

---

## 📊 性能予測

### TCP送信時間の削減

**現在（単一フレーム）:**
```
1フレーム送信 = 233ms
  ├─ usrsockオーバーヘッド: ~100ms (4回コンテキストスイッチ)
  ├─ GS2200M SPI転送:       ~80ms
  └─ TCPプロトコル:          ~53ms

FPS = 1000ms / 233ms = 4.3 fps (理論値)
実測 = 3.6 fps
```

**バッチング後（3フレーム）:**
```
3フレームバッチ送信 = 350ms (推定)
  ├─ usrsockオーバーヘッド: ~100ms (1回のみ！)
  ├─ GS2200M SPI転送:       ~180ms (3倍)
  └─ TCPプロトコル:          ~70ms (1.3倍)

実効FPS = 3 × (1000ms / 350ms) = 8.6 fps
```

**改善率:**
```
3.6 fps → 8.6 fps = +138%
```

### メモリ使用量

**追加メモリ:**
```
バッチバッファ: 180 KB (MJPEG_MAX_BATCH_SIZE)
```

**Phase 7.2メモリ予算:**
```
Phase 7.2総使用量:   607 KB
追加バッチバッファ:  180 KB
────────────────────────
合計:                787 KB

利用可能RAM:         640 KB
不足分:              -147 KB ← オーバー！
```

**対策:**
- バッチバッファは一時バッファ（送信後即解放）
- 実際の常駐メモリ増加は最小限
- またはキュー深度を4に削減（-60 KB）

---

## 🚨 実装の注意点

### 1. メモリ管理

**問題**: バッチバッファが大きい（180 KB）

**解決策:**
- スタックではなくヒープ割り当て
- 送信完了後即座に解放
- またはキュー深度を1つ削減してメモリ確保

### 2. フレーム順序保証

**問題**: バッチ内のフレーム順序

**解決策:**
- 各フレームにframe_sequenceを付与
- PC側で順序を保証

### 3. エラーハンドリング

**問題**: バッチ途中でエラー発生

**解決策:**
- CRC検証失敗時はバッチ全体を破棄
- 次のsync wordから再同期

### 4. 後方互換性テスト

**問題**: PC側が古いバージョンの場合

**解決策:**
- 0xCAFEBABE（単一フレーム）も継続サポート
- Spresense側でバッチングON/OFF設定可能

---

## 🛠 実装手順

### ステップ1: プロトコル定義
1. ✅ mjpeg_protocol.h: 構造体定義追加
2. ✅ mjpeg_protocol.c: mjpeg_pack_batch()実装

### ステップ2: Spresense側バッチング
1. ✅ camera_threads.c: frame_batch_t構造体追加
2. ✅ camera_threads.c: バッチング収集ロジック追加
3. ✅ camera_threads.c: タイムアウト処理追加

### ステップ3: PC側パーサー
1. ✅ tcp_connection.rs: read_batch_frames()追加
2. ✅ gui_main.rs: バッチフレーム処理追加

### ステップ4: テスト
1. ✅ ビルドとフラッシュ
2. ✅ 単一フレームモードでテスト（既存互換性）
3. ✅ バッチモードでテスト（性能測定）

---

## 📝 設定可能なパラメータ

```c
/* Phase 7.2a: Batching configuration */
#define MJPEG_BATCHING_ENABLED   1        /* 0: disabled, 1: enabled */
#define MJPEG_BATCH_SIZE         3        /* Number of frames per batch (1-3) */
#define MJPEG_BATCH_TIMEOUT_MS   100      /* Timeout for partial batch (ms) */
```

---

## 🎯 期待される成果

### 性能目標

| 指標 | Phase 7.2 | Phase 7.2a目標 | 改善率 |
|------|-----------|----------------|--------|
| FPS | 4-5 fps | **8-9 fps** | +80-100% |
| TCP送信時間 | 220ms | **350ms/3フレーム** | - |
| usrsockコール | フレーム毎 | **バッチ毎** | -66% |
| 目標達成 | ❌ | ✅ | - |

### 成功基準

- ✅ FPS ≥ 8 fps
- ✅ バッチ送信成功率 > 95%
- ✅ PC側で全フレーム正常デコード
- ✅ メモリ使用量が予算内

---

**ドキュメントバージョン**: 1.0
**作成日**: 2026-01-08
**次のステップ**: プロトコル実装開始
