# Phase 7 WiFi/TCP E2E Architecture Analysis

**作成日**: 2026-01-03
**対象**: Phase 7 WiFi/TCP transport implementation
**目的**: システム全体（PC～Spresense）のボトルネック分析とメトリクス追加提案

---

## 目次

1. [システムアーキテクチャ全体図](#システムアーキテクチャ全体図)
2. [各コンポーネント詳細](#各コンポーネント詳細)
3. [データフローとレイテンシ分析](#データフローとレイテンシ分析)
4. [ボトルネック分析](#ボトルネック分析)
5. [メトリクス追加提案](#メトリクス追加提案)
6. [改善案](#改善案)
7. [測定計画](#測定計画)

---

## 1. システムアーキテクチャ全体図

![E2E Architecture](diagrams/15_PHASE7_E2E_ARCHITECTURE_ANALYSIS.puml)

### 1.1 レイヤー構成

**Spresense側（7レイヤー）**:
1. **Camera Driver層**: ISX012 → V4L2 (3 buffers)
2. **Application層**: Camera Thread → Frame Queue → TCP/USB Thread
3. **Transport層**: TCP Server / USB Serial
4. **TCP/IP Stack層**: usrsock architecture (NuttX)
5. **Daemon層**: gs2200m daemon (user-space)
6. **WiFi Driver層**: GS2200M WiFi driver
7. **Physical層**: SPI DMA → 802.11n

**Network層**:
- WiFi (802.11n, 40-70Mbps理論値)
- TCP/IP

**PC側（4レイヤー）**:
1. **Network層**: TCP Client (OS TCP stack)
2. **Protocol層**: Sync word search → Packet parser
3. **Decode層**: JPEG decoder
4. **UI層**: GUI renderer (egui)

---

## 2. USB Serial通信仕様と実績（Baseline）

### 2.1 USB Serial仕様

| 項目 | 仕様値 | 備考 |
|---|---|---|
| USB規格 | USB 2.0 Full Speed | CXD56xx制限 |
| 理論帯域 | 12 Mbps | 1.5 MB/s |
| プロトコル | CDC-ACM | Communication Device Class |
| プロトコルオーバーヘッド | ~30% | CDC-ACMヘッダ、フレーミング |
| 実効帯域 | ~8.5 Mbps | 1.06 MB/s (70% efficiency) |
| 最大スループット | ~8.5 Mbps | 理論上限 |

**USB 2.0 Full Speed制限の理由**:
- CXD56xxチップはUSB 2.0 Full Speedのみサポート
- High Speed (480 Mbps) は未サポート

### 2.2 USB Serial実績（Phase 1.5: Sequential）

**テスト条件**: VGA 640x480, Sequential処理（パイプラインなし）

| メトリクス | 実測値 | 理論値との差分 | 評価 |
|---|---|---|---|
| **FPS** | **6.61 fps** | - | Baseline |
| **Total Latency** | 151.3 ms/frame | - | Camera 6.4ms + Pack 38.4ms + USB Write 60.1ms + Others 46.4ms |
| **USB Write Latency** | 60.12 ms | - | USB転送時間 |
| **JPEG Size (avg)** | 64.00 KB | - | シーン依存 |
| **USB Throughput** | 3.65 Mbps | 8.5 Mbps (理論値) | **42.9%** (57.1%未使用) |
| **USB Utilization** | 30.4% of 12 Mbps | 100% | 帯域余裕あり |
| **Frame Interval** | 151.3 ms | 33ms (30fps目標) | **4.6倍遅い** |

**ボトルネック分析（Phase 1.5）**:
- USB転送: 60.12 ms (39.7% of total)
- MJPEG Pack: 38.43 ms (25.4%)
- その他: 52.75 ms (34.9%) - Camera DQBUF/QBUF, Protocol overhead

**結論**: USB帯域は十分（30.4%使用）。ボトルネックは処理レイテンシ（Sequential実行）。

### 2.3 USB Serial実績（Phase 2: Pipelined Threading）

**テスト条件**: VGA 640x480, Multi-threaded pipelining (camera_thread + usb_thread)
**テスト期間**: 2.7時間連続運転 (107,712 frames)

| メトリクス | 実測値 | Phase 1.5比 | 評価 |
|---|---|---|---|
| **FPS (avg)** | **11.05 fps** | +67.2% | ✅ パイプライン効果 |
| **FPS (simple scene)** | **12.54 fps** | +89.7% | ✅ **目標達成** (12.5fps) |
| **Frame Interval (avg)** | 90.5 ms | -40.1% | Phase 1.5: 151.3ms |
| **Serial Read Time (complex)** | 86.2 ms | - | PC側測定 (USB転送 + Protocol) |
| **Serial Read Time (simple)** | 69.3 ms | -19.6% | JPEG size減少の効果 |
| **JPEG Size (complex)** | 52.3 KB | -18.3% | Phase 1.5: 64.0KB |
| **JPEG Size (simple)** | 42.6 KB | -33.4% | シーン依存 |
| **Success Rate** | 99.89% | - | 113 errors / 107,712 frames |
| **Queue Depth** | 1 (99.99%) | - | **Perfect balance** ✅ |

**パイプライン効果の内訳**:
```
Phase 1.5 (Sequential):
  Camera (6.4ms) → Pack (38.4ms) → USB (60.1ms) = 104.9ms
  Total latency: 151.3ms/frame (6.61 fps)

Phase 2 (Pipelined):
  Camera Thread: Camera (6.4ms) + Pack (38.4ms) = 44.8ms
  USB Thread:    USB Write (60.1ms)
  Critical path: max(44.8, 60.1) = 60.1ms
  Actual: 90.5ms/frame (11.05 fps)

Expected speedup: 151.3 / 60.1 = 2.52x
Actual speedup:   151.3 / 90.5 = 1.67x
Efficiency:       1.67 / 2.52 = 66.3%
```

**パイプライン効率66.3%の原因**:
- Context switch overhead
- Mutex contention
- Queue synchronization overhead
- USB Serial読み取りブロッキング（PC側）

### 2.4 USB Serial vs WiFi/TCP 比較

| メトリクス | USB Serial<br>(Phase 2) | WiFi/TCP<br>(Phase 7) | 差分 | 評価 |
|---|---|---|---|---|
| **FPS** | **11.05 fps** | **1-2 fps** | **5.5-11倍遅い** | 🔴 Critical |
| **Frame Interval** | 90.5 ms | 500-1000 ms | **5.5-11倍遅い** | 🔴 Critical |
| **Transport Latency** | 60 ms (USB Write) | 推定500-700 ms (TCP Send) | **8-12倍遅い** | 🔴 P0 Bottleneck |
| **Throughput** | 3.65 Mbps | 不明 (要測定) | - | 要調査 |
| **Protocol Overhead** | CDC-ACM (~30%) | TCP + usrsock (不明) | - | 要調査 |
| **Success Rate** | 99.89% | 不明 | - | メトリクス未受信 |
| **Stability** | ✅ 2.7時間安定 | ❌ 数秒で切断 | - | 不安定 |

**WiFi/TCP性能劣化の原因仮説**:

| 原因 | 推定影響 | 優先度 | 根拠 |
|---|---|---|---|
| usrsock architecture overhead | 300-500 ms | 🔴 P0 | Context switch + Unix socket通信 |
| GS2200M WiFi throughput不足 | 50-100 ms | 🟡 P2 | SPI帯域40Mbps制限 |
| PC側Sync word検索 | 100 ms | 🟠 P1 | 10KB検索 (実測) |
| TCP protocol overhead | 50 ms | 🟢 P3 | ACK待ち、再送 |

**USB SerialとWiFi/TCPの根本的な違い**:

| 項目 | USB Serial | WiFi/TCP |
|---|---|---|
| **通信スタック** | Kernel直接 (CDC-ACM driver) | **User-space経由 (usrsock)** ← Overhead |
| **Context Switch** | 1回 (App → Kernel) | **4回** (App → Kernel → Daemon → Kernel → Driver) |
| **Data Copy** | 1回 | **3-4回** (usrsock + Unix socket + SPI) |
| **Blocking I/O** | write() = Kernel内完結 | write() = **Daemon経由** (遅い) |
| **Throughput** | 8.5 Mbps (実効) | 不明 (SPI 40Mbps制限?) |
| **Latency** | 60 ms (実測) | 500-700 ms (推定) |

---

## 3. 各コンポーネント詳細

### 3.1 Spresense: Camera Driver層

| コンポーネント | 仕様 | レイテンシ | 備考 |
|---|---|---|---|
| ISX012 Camera | JPEG 1280x720@30fps | - | Hardware JPEG encoder |
| V4L2 Driver | Triple buffering (3 buffers) | - | ioctl: DQBUF/QBUF |
| V4L2 Buffers | 3 x 64KB = 192KB | - | Kernel memory |

**Key Points**:
- V4L2バッファは3つ固定（`CAMERA_BUFFER_NUM=3`）
- JPEG圧縮はハードウェア（ISX012内蔵encoder）
- 平均JPEG size: 52-61KB（シーン依存）

### 3.2 Spresense: Application層

| コンポーネント | 仕様 | レイテンシ | 備考 |
|---|---|---|---|
| Camera Thread | Priority 110 (HIGH) | - | Producer |
| TCP/USB Thread | Priority 100 (LOWER) | - | Consumer |
| Frame Queue | 5 buffers x ~100KB = 500KB | - | Pipelined threading |
| Empty Queue | Return path | - | Buffer recycling |

**Threading Model**:
```c
Camera Thread (110):
  1. DQBUF from V4L2           // ~2ms
  2. MJPEG pack                // ~9ms
  3. Enqueue to Frame Queue
  4. QBUF to V4L2              // Return empty buffer

TCP/USB Thread (100):
  1. Dequeue from Frame Queue
  2. Send via TCP or USB       // USB: ~30ms, TCP: ???ms
  3. Enqueue to Empty Queue
```

**Current Problem**:
- Frame Queue常に満杯（5/5）→ メトリクスパケット送信不可
- 原因: TCP送信速度 << カメラ取得速度

### 3.3 Spresense: TCP/IP Stack層（usrsock architecture）

| コンポーネント | 仕様 | レイテンシ | 備考 |
|---|---|---|---|
| Socket API | POSIX socket API | - | write(sockfd, ...) |
| usrsock Client | User-space socket | **高い** | Unix socket経由 |
| TCP Send Buffer | 256KB (SO_SNDBUF) | - | TCP最適化済み |
| TCP_NODELAY | Enabled | - | Nagle無効化 |

**usrsock Architecture**:
```
Application (security_camera)
    ↓ write(sockfd)
Socket API (libc)
    ↓ ioctl()
usrsock Client (kernel)
    ↓ Unix domain socket
gs2200m daemon (user-space)
    ↓ ioctl()
GS2200M Driver (kernel)
    ↓ SPI DMA
GS2200M WiFi Module
```

**Overhead Sources**:
1. **Context switch**: Kernel ↔ User (gs2200m daemon)
2. **Unix socket**: usrsock client ↔ daemon 通信
3. **Serialization**: データのコピー多発

### 3.4 Spresense: WiFi Driver層

| コンポーネント | 仕様 | Throughput | 備考 |
|---|---|---|---|
| gs2200m daemon | User-space daemon | - | Manages WiFi connection |
| GS2200M Driver | SPI DMA enabled | - | CXD56_DMAC_SPI5_TX/RX |
| SPI Bus | 40 Mbps max | **制限あり** | SPI clock speed限界 |
| GS2200M Module | 802.11n (2.4GHz) | 40-70 Mbps理論値 | WiFi throughput |

**Potential Bottlenecks**:
1. **SPI帯域**: 40 Mbps max（5 MB/s）
   - 30fps x 57KB/frame = 13.7 Mbps → SPI帯域は十分
2. **WiFi throughput**: 実測値不明
3. **usrsock overhead**: Context switch + Unix socket

### 3.5 PC側: Protocol層

| コンポーネント | 仕様 | レイテンシ | 備考 |
|---|---|---|---|
| TCP Client | TcpStream (Rust std) | - | OS TCP stack使用 |
| Sync Word Search | Max 10KB search | **~100ms** | 現在のボトルネック |
| Packet Parser | Protocol decode | ~1ms | MJPEG/Metrics判別 |

**Sync Word Search Problem**:
- **原因**: TCP送信遅延 → パケット境界ずれ → 毎回10KB検索
- **影響**: 100ms/frame の無駄なオーバーヘッド
- **根本原因**: Spresense側TCP送信が遅すぎてバッファ詰まり

### 3.6 PC側: Decode & UI層

| コンポーネント | 仕様 | レイテンシ | 備考 |
|---|---|---|---|
| JPEG Decoder | image crate (Rust) | ~10ms | 1280x720 JPEG |
| GUI Renderer | egui (Rust) | ~5ms | OpenGL backend |
| Metrics Logger | CSV writer | <1ms | Async I/O |

---

## 4. データフローとレイテンシ分析

### 4.1 USB Serial Mode（Baseline）

| ステップ | 処理 | レイテンシ | 累積 |
|---|---|---|---|
| 1 | Camera DQBUF | 2ms | 2ms |
| 2 | MJPEG Pack | 9ms | 11ms |
| 3 | USB Serial Send | 30ms | 41ms |
| 4 | PC USB Read | 5ms | 46ms |
| 5 | JPEG Decode | 10ms | 56ms |
| 6 | GUI Render | 5ms | 61ms |
| **Total** | | **61ms** | **16.4 fps** |

**実測**: 11.0 fps（Phase 2パイプライン最適化後）

### 4.2 WiFi/TCP Mode（Current）

| ステップ | 処理 | レイテンシ | 累積 |
|---|---|---|---|
| 1 | Camera DQBUF | 2ms | 2ms |
| 2 | MJPEG Pack | 9ms | 11ms |
| 3 | **TCP Send** | **???ms** | **???ms** |
| 4 | WiFi TX | ???ms | ???ms |
| 5 | PC TCP Read | ???ms | ???ms |
| 6 | **Sync Word Search** | **100ms** | **???ms** |
| 7 | JPEG Decode | 10ms | ???ms |
| 8 | GUI Render | 5ms | ???ms |
| **Total** | | **???ms** | **1-2 fps** |

**実測**: 1-2 fps（metrics_20260103_033832.csv）

**CSVデータからの推定**:
- `serial_read_time_ms`: 平均1000ms → TCP read + Sync search
- Total latency: ~1000ms/frame → **1 fps**

### 4.3 レイテンシ内訳推定

```
Total: 1000ms/frame (実測)
├─ Camera + Pack: 11ms
├─ TCP Send: ???ms (要計測)
├─ WiFi TX: ???ms (要計測)
├─ PC TCP Read: ???ms (要計測)
├─ Sync Word Search: 100ms (推定)
└─ JPEG Decode + Render: 15ms

Unknown: 1000 - 11 - 100 - 15 = 874ms
→ TCP Send + WiFi TX + PC TCP Read ≈ 874ms
```

**仮説**:
- TCP Send（Spresense側）: 500-700ms ← **メインボトルネック**
- WiFi TX: 50-100ms
- PC TCP Read: 100-174ms

---

## 5. ボトルネック分析

### 5.1 ボトルネック優先度

![Bottleneck Timeline](diagrams/15_PHASE7_E2E_BOTTLENECK_ANALYSIS.puml)

| 優先度 | ボトルネック | 推定影響 | 場所 | 対策難易度 |
|---|---|---|---|---|
| **🔴 P0** | TCP送信遅延（usrsock） | 500-700ms | Spresense TCP stack | 高 |
| **🟠 P1** | PC側Sync word検索 | 100ms | PC tcp_connection.rs | 中 |
| **🟡 P2** | WiFi throughput不足 | 50-100ms? | GS2200M module | 高 |
| 🟢 P3 | Frame Queue詰まり | 副次的 | Spresense app | 低 |

### 5.2 P0: TCP送信遅延（usrsock overhead）

**症状**:
- TCP send時間が異常に長い（推定500-700ms/frame）
- Frame Queue常に満杯（5/5）
- Metricsパケット送信不可（"No empty buffer"）

**原因仮説**:
1. **usrsock architectureのオーバーヘッド**
   - Context switch: User (app) → Kernel (usrsock) → User (daemon) → Kernel (driver)
   - Unix socket通信: usrsock client ↔ gs2200m daemon
   - データコピー多発

2. **gs2200m daemonの処理遅延**
   - デーモンがCPU時間を十分に取得できていない?
   - SPI通信の待ち時間?

3. **TCP send bufferの詰まり**
   - 256KB buffer設定済みだが機能していない?
   - TCP ACK待ちでブロック?

**検証方法**:
- メトリクス追加（後述）

### 5.3 P1: PC側Sync word検索

**症状**:
- 10KB検索 = ~100ms/frame
- "Failed to find sync word after 10000 attempts"エラー頻発

**原因**:
- TCP送信遅延 → パケット境界ずれ → 毎回検索必要

**対策**:
1. **Spresense側**: TCP送信を安定化（P0解決で自動的に改善）
2. **PC側**: より効率的なsync検索アルゴリズム
   - Boyer-Moore法?
   - SIMD命令使用?

### 5.4 P2: WiFi throughput不足

**症状**:
- 実測throughput不明

**理論値計算**:
```
Required bandwidth (30fps):
  30 fps × 57 KB/frame = 1710 KB/s = 13.7 Mbps

GS2200M spec:
  802.11n: 40-70 Mbps理論値
  SPI bus: 40 Mbps max

→ 理論上は十分（13.7 < 40 Mbps）
```

**検証方法**:
- WiFi throughput実測（iperf3など）
- SPI bus使用率測定

### 5.5 P3: Frame Queue詰まり

**症状**:
- action_q常に3-5（満杯）
- empty_q常に0

**原因**:
- TCP送信速度 << カメラ取得速度（P0の副次的影響）

**影響**:
- Metricsパケット送信不可
- カメラスレッドブロック（バッファ待ち）

**対策**:
- P0解決で自動的に改善
- Buffer数を増やしても根本解決にならない（既に3→5に増やしたが効果なし）

---

## 6. メトリクス追加提案

### 6.1 現在のメトリクス（Phase 4.1）

**Spresense側（Metricsパケット）**:
```c
typedef struct {
  uint32_t sync_word;           // 0xCAFEBEEF
  uint16_t seq;                 // Sequence number
  uint32_t camera_frames;       // Total frames from camera
  uint32_t camera_fps;          // Camera FPS (calculated)
  uint32_t usb_packets;         // Packets sent via USB/TCP
  uint16_t action_q_depth;      // Frame queue depth
  uint16_t spresense_errors;    // Total errors
  uint16_t crc;                 // CRC16
} __attribute__((packed)) metrics_packet_t;
```

**PC側（CSV）**:
- `pc_fps`, `spresense_fps`, `frame_count`, `error_count`
- `decode_time_ms`, `serial_read_time_ms`, `texture_upload_time_ms`
- `jpeg_size_kb`
- `spresense_camera_frames`, `spresense_camera_fps`
- `spresense_usb_packets`, `action_q_depth`, `spresense_errors`

### 6.2 追加すべきメトリクス

#### 6.2.1 Spresense側（優先度順）

| 優先度 | メトリクス名 | 計測内容 | 目的 | 実装場所 |
|---|---|---|---|---|
| **P0** | `tcp_send_time_us` | TCP send時間（μs） | TCP送信ボトルネック定量化 | tcp_server.c:tcp_server_send() |
| **P0** | `usrsock_latency_us` | usrsock往復時間（μs） | usrsockオーバーヘッド計測 | tcp_server.c:tcp_server_send() |
| P1 | `queue_enqueue_wait_us` | Queue enqueue待ち時間 | Queue詰まり検出 | camera_threads.c:camera_thread_func() |
| P1 | `queue_dequeue_wait_us` | Queue dequeue待ち時間 | TCP送信ブロック検出 | camera_threads.c:usb_thread_func() |
| P2 | `wifi_throughput_bps` | WiFi実効throughput（bps） | WiFi帯域使用率 | tcp_server.c |
| P2 | `tcp_retransmit_count` | TCP再送回数 | TCP品質評価 | NuttX stats |
| P3 | `spi_transfer_time_us` | SPI転送時間 | SPI帯域評価 | gs2200m driver |

#### 6.2.2 PC側（優先度順）

| 優先度 | メトリクス名 | 計測内容 | 目的 | 実装場所 |
|---|---|---|---|---|
| **P0** | `tcp_read_time_ms` | TCP read時間（ms） | PC側TCP受信時間 | tcp_connection.rs:read_packet() |
| **P0** | `sync_search_time_ms` | Sync検索時間（ms） | Sync検索ボトルネック | tcp_connection.rs:read_packet() |
| **P0** | `sync_search_attempts` | Sync検索試行回数 | パケット同期ずれ検出 | tcp_connection.rs:read_packet() |
| P1 | `tcp_bytes_per_read` | Read 1回あたりのバイト数 | TCP受信効率 | tcp_connection.rs:read_packet() |
| P2 | `network_latency_ms` | ネットワーク遅延（RTT） | WiFi遅延評価 | 別途ping測定 |

### 6.3 メトリクス実装案

#### 6.3.1 Spresense側: TCP send時間計測

**実装場所**: `apps/examples/security_camera/tcp_server.c`

```c
int tcp_server_send(tcp_server_t *server, const void *data, size_t len)
{
  struct timespec start, end;
  uint64_t send_time_us;

  // 開始時刻
  clock_gettime(CLOCK_MONOTONIC, &start);

  // 既存の送信処理
  ssize_t sent;
  size_t total_sent = 0;
  // ... (既存コード) ...

  // 終了時刻
  clock_gettime(CLOCK_MONOTONIC, &end);

  // 送信時間計算（μs）
  send_time_us = (end.tv_sec - start.tv_sec) * 1000000ULL +
                 (end.tv_nsec - start.tv_nsec) / 1000ULL;

  // グローバル統計に記録
  g_tcp_stats.total_send_time_us += send_time_us;
  g_tcp_stats.send_count++;

  // 異常値検出（100ms超え）
  if (send_time_us > 100000) {
    _warn("TCP send took %lu us (%lu ms)\n",
          send_time_us, send_time_us / 1000);
  }

  return total_sent;
}
```

**Metricsパケットに追加**:
```c
typedef struct {
  // ... (既存フィールド) ...
  uint32_t avg_tcp_send_us;   // 平均TCP送信時間（μs）
  uint32_t max_tcp_send_us;   // 最大TCP送信時間（μs）
  uint16_t tcp_send_count;    // 送信回数
  uint16_t crc;
} __attribute__((packed)) metrics_packet_t;
```

#### 6.3.2 PC側: TCP read + Sync検索時間計測

**実装場所**: `src/tcp_connection.rs`

```rust
pub fn read_packet(&mut self, buffer: &mut [u8]) -> io::Result<usize> {
    let start = Instant::now();
    let mut sync_attempts = 0;

    // Phase 1: Sync word検索
    let sync_start = Instant::now();
    // ... (既存のsync検索コード) ...
    let sync_duration = sync_start.elapsed();

    // Phase 2: データ読み込み
    // ... (既存の読み込みコード) ...

    let total_duration = start.elapsed();

    // メトリクス記録
    info!(
        "TCP read: total={}ms, sync_search={}ms, attempts={}, data_read={}ms",
        total_duration.as_millis(),
        sync_duration.as_millis(),
        sync_attempts,
        (total_duration - sync_duration).as_millis()
    );

    // CSVに記録（既存のMetricsLoggerに追加）
    // ...

    Ok(total_read)
}
```

**CSV列追加**:
```csv
timestamp, ..., tcp_read_time_ms, sync_search_time_ms, sync_search_attempts, tcp_bytes_per_read
```

---

## 7. 改善案

### 7.1 短期対策（P0/P1ボトルネック緩和）

#### 7.1.1 TCP送信最適化（Spresense側）

**現状**: 既に実施済み
- ✅ `TCP_NODELAY = 1`（Nagle無効化）
- ✅ `SO_SNDBUF = 256KB`（送信バッファ増量）

**追加対策案**:

1. **非同期TCP送信（検討中）**
   ```c
   // 現在: 同期送信（write()でブロック）
   write(sockfd, data, len);  // ← ここで500ms待つ?

   // 案: 非同期送信
   // ただしNuttXのTCP/IPスタックが非同期I/Oをサポートしているか要確認
   ```

2. **送信スレッド分離（検討中）**
   ```
   現在:
     TCP Thread: Dequeue → TCP send (blocking)

   案:
     TCP Thread: Dequeue → Copy to send buffer → Enqueue to send_queue
     Send Thread: Dequeue from send_queue → TCP send (blocking)
   ```
   - メリット: Frame Queueの詰まり緩和
   - デメリット: スレッド数増加、複雑度増加

3. **usrsock bypass（要調査）**
   - GS2200Mドライバを直接呼び出す（usrsock経由しない）
   - デメリット: POSIX socket APIが使えない、実装困難

#### 7.1.2 Sync検索最適化（PC側）

**現状**:
```rust
// 1バイトずつスライドして検索
for i in 0..10000 {
    let sync_word = u32::from_le_bytes([...]);
    if sync_word == 0xCAFEBABE || sync_word == 0xCAFEBEEF {
        break;
    }
}
```

**改善案**:

1. **Boyer-Moore法**（文字列検索の高速化）
   - 理論上3-4倍高速化
   - ただし実装複雑度高い

2. **SIMD命令使用**（Rust: `std::arch`）
   - 4バイト同時比較 → 4倍高速化
   - ただしプラットフォーム依存

3. **検索範囲削減**
   - 現在: 10KB（10000バイト）
   - 提案: 1KB（1000バイト）→ 10倍高速化
   - 前提: TCP送信が安定化していること（P0解決後）

#### 7.1.3 Frame Queue拡張（副次的）

**現状**: 5 buffers（既に3→5に増やしたが効果なし）

**提案**: これ以上増やしても効果なし
- 根本原因はTCP送信遅延（P0）
- Buffer増やしても遅延が累積するだけ

### 7.2 中期対策（WiFi throughput調査）

1. **WiFi throughput実測**
   ```bash
   # PC側でiperf3 server起動
   iperf3 -s

   # Spresense側でiperf3 client実行（要移植）
   iperf3 -c <PC_IP> -t 30
   ```

2. **SPI帯域使用率測定**
   - gs2200mドライバにカウンタ追加
   - SPI転送量を記録

3. **WiFi設定最適化**
   - チャネル変更（干渉回避）
   - 802.11n設定確認（2.4GHz, 40MHz幅）

### 7.3 長期対策（アーキテクチャ変更）

1. **WiFiモジュール変更**
   - GS2200M（usrsock） → NRC7292（native TCP/IP stack）
   - ただしWAPI互換性問題あり（Case Study 17参照）

2. **UDP化**
   - TCP → UDP（信頼性低下、実装簡略化）
   - フレーム欠損許容

3. **H.264圧縮**
   - MJPEG → H.264（帯域1/3に削減）
   - ただしSpresenseにH.264 encoderなし（要外部chip）

---

## 8. 測定計画

### 8.1 Phase 1: メトリクス実装（優先度P0のみ）

**目標**: ボトルネック定量化

**実装対象**:
1. Spresense側: `tcp_send_time_us`, `usrsock_latency_us`
2. PC側: `tcp_read_time_ms`, `sync_search_time_ms`, `sync_search_attempts`

**期待結果**:
```
TCP send時間分布:
  Min: 10ms
  Avg: 500ms ← ボトルネック
  Max: 1000ms

Sync検索時間:
  Min: 1ms
  Avg: 100ms
  Max: 200ms

→ TCP send時間が支配的であることを確認
```

### 8.2 Phase 2: ボトルネック調査

**調査項目**:
1. **usrsock latency**
   - usrsock client → gs2200m daemon 往復時間
   - 測定: `clock_gettime()` before/after `write(sockfd)`

2. **gs2200m daemon CPU使用率**
   - `top` コマンドでdaemon CPU時間確認
   - 仮説: Daemonが十分なCPU時間を取得できていない

3. **SPI transfer時間**
   - gs2200mドライバにログ追加
   - SPI DMA完了までの時間測定

### 8.3 Phase 3: 改善実装とA/Bテスト

**A/Bテスト案**:
- **A（現状）**: usrsock architecture
- **B（改善）**: 非同期TCP送信 or 送信スレッド分離

**評価指標**:
- FPS: 1-2 fps → **8-15 fps** （目標: USB Serialの70%）
- TCP send時間: 500ms → **50ms** （目標: 10倍改善）
- Frame Queue深度: 5/5 → **1-2/5** （目標: 余裕あり）

---

## 9. まとめ

### 9.1 現状認識

| 項目 | 現状 | 目標 | ギャップ |
|---|---|---|---|
| FPS | 1-2 fps | 30 fps | **15-30倍遅い** |
| Frame latency | 500-1000ms | 33ms | **15-30倍遅い** |
| TCP send時間 | 推定500ms | <50ms | **10倍遅い** |
| Sync検索時間 | 100ms | <10ms | **10倍遅い** |

### 9.2 ボトルネック優先度

1. **🔴 P0**: TCP送信遅延（usrsock overhead） - 推定500-700ms
2. **🟠 P1**: PC側Sync word検索 - 100ms
3. **🟡 P2**: WiFi throughput不足 - 要調査
4. 🟢 P3: Frame Queue詰まり - P0の副次的影響

### 9.3 次のステップ

**Immediate（今すぐ）**:
1. メトリクス実装（P0: TCP send時間、Sync検索時間）
2. 測定実行（30秒テスト）
3. ボトルネック定量化

**Short-term（1-2日）**:
1. usrsock latency詳細調査
2. Sync検索最適化実装（1KB制限）
3. A/Bテスト実施

**Mid-term（1週間）**:
1. WiFi throughput実測
2. 非同期TCP送信検討
3. アーキテクチャ改善案評価

---

## 付録A: 関連ファイル

### A.1 Spresense側

| ファイル | 役割 | 関連箇所 |
|---|---|---|
| `camera_threads.c` | Camera/TCP thread実装 | Line 64-90: Performance strategy |
| `tcp_server.c` | TCP server実装 | Line 172-231: tcp_server_send() |
| `frame_queue.c/h` | Frame queue管理 | Line 50: MAX_QUEUE_DEPTH=5 |
| `wifi_manager.c` | WiFi接続管理 | Line 36-63: Connection handling |

### A.2 PC側

| ファイル | 役割 | 関連箇所 |
|---|---|---|
| `src/tcp_connection.rs` | TCP client実装 | Line 75-231: read_packet() |
| `src/gui_main.rs` | GUI + metrics logging | Line 100-105: SpresenseMetrics |
| `src/metrics.rs` | CSV logging | Metrics collection |

### A.3 ドキュメント

| ドキュメント | 内容 |
|---|---|
| `docs/case_study/17_PHASE7_WIFI_WAPI_COMPATIBILITY.md` | WAPI互換性問題 |
| `docs/security_camera/01_specifications/05_SOFTWARE_SPEC_SPRESENSE.md` | Spresense仕様 |
| `PHASE4_SPEC.md` | Phase 4メトリクス仕様 |

---

**End of Document**
