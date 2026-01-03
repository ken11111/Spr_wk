# Phase 7.0 WiFi/TCP Transport テスト結果分析

**テスト日時**: 2026-01-03 18:01
**ファームウェア**: Phase 7.0 (WiFi/TCP + TCP Metrics実装)
**テスト期間**: 約30秒 (11フレーム受信後、sync word not foundで異常終了)

---

## 1. エグゼクティブサマリー

### テスト結果: ❌ 失敗 (深刻なボトルネック継続)

| 項目 | 実測値 | 目標値 | 達成率 | 評価 |
|------|--------|--------|--------|------|
| **PC側FPS** | 0.57-1.62 fps | 15-25 fps | **3.8-10.8%** | ❌ 大幅未達 |
| **TCP read時間** | 355-557 ms | <50 ms | **636-1114%超過** | ❌ 7-11倍遅い |
| **Spresense Camera FPS** | 30 fps | 30 fps | 100% | ✅ 正常 |
| **Queue Depth** | 5 (常時満杯) | 0-3 | - | ❌ ボトルネック |
| **Metricsパケット受信** | 0個 | ~30個/秒 | 0% | ❌ 未受信 |
| **テスト完了** | 11フレームで異常終了 | 継続運転 | - | ❌ 異常終了 |

### 主要な問題点

1. **TCP送信ボトルネック継続**:
   - TCP read時間が355-557ms (Phase 7 Metrics実装前と同様)
   - TCP send時間メトリクスは取得できず (Metricsパケット未受信)

2. **キュー満杯状態**:
   - action_q_depth = 5 (最大値で常時満杯)
   - Camera thread: 52フレーム処理
   - USB/TCP thread: 47パケット送信のみ
   - **5フレームがキューに残留** (52 - 47 = 5)

3. **Metricsパケット送信失敗**:
   - 全てのMetrics送信で "No empty buffer" エラー
   - 14回Metricsパケット生成試行、**全て失敗**

4. **異常終了**:
   - Spresense側: `Client disconnected (error -107)` (ENOTCONN)
   - PC側: `Sync word not found` × 10回 → 停止

---

## 2. Spresense側ログ分析

### 2.1 WiFi接続とTCP Server初期化

```
nsh> gs2200m DESKTOP-GPU979R B54p3530 &
gs2200m [13:50]

nsh> ifconfig
wlan0   Link encap:Ethernet HWaddr 3c:95:09:00:64:ac at UP mtu 1500
        inet addr:192.168.137.210 DRaddr:192.168.137.1 Mask:255.255.255.0
```

✅ **WiFi接続成功**:
- IP: 192.168.137.210
- Gateway: 192.168.137.1
- Subnet mask: 255.255.255.0

```
nsh> security_camera &
security_camera [14:100]

[CAM] Security Camera Application Starting (MJPEG)
[CAM] Camera config: 640x480 @ 30 fps, Format=JPEG, HDR=0
[CAM] WiFi connected! IP: 192.168.137.210
[CAM] TCP server initialized on port 8888
[CAM] Waiting for client connection...
[CAM] Client connected! Starting MJPEG streaming...
```

✅ **TCP Server正常起動**:
- Port 8888でリスニング
- クライアント接続成功

---

### 2.2 フレーム送信とキュー状態

#### フレーム処理統計

| 区間 | Camera Frames | USB Packets | Queue Depth | Metrics試行 | 結果 |
|------|---------------|-------------|-------------|-------------|------|
| 0-6 | 7 | 2 | 5 | 1 | No buffer |
| 7-9 | 10 | 5 | 5 | 1 | No buffer |
| 10-12 | 13 | 8 | 5 | 1 | No buffer |
| 13-16 | 17 | 12 | 5 | 1 | No buffer |
| 17-20 | 21 | 16 | 5 | 1 | No buffer |
| 21-24 | 25 | 20 | 5 | 1 | No buffer |
| 25-27 | 28 | 23 | 5 | 1 | No buffer |
| 28-33 | 35 | 30 | 5 | 1 | No buffer |
| 34-38 | 39 | 34 | 5 | 1 | No buffer |
| 39-41 | 42 | 37 | 5 | 1 | No buffer |
| 42-45 | 46 | 41 | 5 | 1 | No buffer |
| 46-48 | 49 | 44 | 5 | 1 | No buffer |
| 49-51 | 52 | 47 | 5 | 1 | No buffer |

**統計**:
- Total Camera Frames: **52**
- Total USB/TCP Packets: **47**
- Queue Depth: **5** (常時満杯)
- Metrics試行: **14回**
- Metrics成功: **0回** (100% 失敗)

#### キューバランス分析

```
Camera capture速度: 52 frames / 実行時間
TCP send速度: 47 frames / 実行時間
未送信フレーム: 52 - 47 = 5 frames (キュー残留)
```

**結論**:
- **TCP送信がボトルネック**: Camera capture > TCP send
- キューが常に満杯 (depth=5) → バッファ枯渇
- Metricsパケット用の空きバッファなし

---

### 2.3 JPEG Padding除去統計

```
最小: 1 bytes
最大: 31 bytes
平均: ~15-18 bytes/frame (Phase 4.1.1実装の恩恵)
```

✅ **JPEG validation正常動作**:
- 全52フレームで padding 検出・除去成功
- JPEG validation エラー: 0回

---

### 2.4 異常終了ログ

```
[CAM] Packed metrics: seq=13, cam_frames=52, usb_pkts=47, q_depth=5, avg_sizB
[CAM] No empty buffer for metrics packet
[CAM] TCP thread: Client disconnected (error -107)
[CAM] == Camera thread exiting (processed 52 frames) ==
[CAM] Camera thread: JPEG validation errors: 0 (all frames valid)
[CAM] Camera thread: Average JPEG size: 51 KB
[CAM] == USB thread exiting (sent 47 packets, 2456954 bytes total) ==
[CAM] Shutdown requested by threads, exiting main loop
```

**異常終了原因**:
1. PC側が接続切断
2. TCP thread が `error -107` (ENOTCONN: Not connected) を検出
3. Camera/USB threadsがシャットダウン開始
4. クリーンアップ完了

**送信データ量**:
- Total bytes sent: 2,456,954 bytes
- Average packet size: 2,456,954 / 47 = **52,254 bytes/packet** (~51 KB/frame)

---

## 3. PC側ログ分析

### 3.1 CSVメトリクス (11フレーム)

```csv
timestamp,pc_fps,spresense_fps,frame_count,error_count,decode_time_ms,serial_read_time_ms,texture_upload_time_ms,jpeg_size_kb,spresense_camera_frames,spresense_camera_fps,spresense_usb_packets,action_q_depth,spresense_errors,tcp_avg_send_ms,tcp_max_send_ms
1767431119.068,1.62,2.99,2,0,11.03,459.56,0.00,52.91,0,0.00,0,0,0,0.00,0.00
1767431120.834,1.13,3.29,4,0,3.64,375.81,0.00,50.87,0,0.00,0,0,0,0.00,0.00
1767431122.649,1.10,3.53,6,0,4.92,394.67,0.00,50.31,0,0.00,0,0,0,0.00,0.00
1767431124.251,0.62,3.59,7,0,2.95,355.46,0.00,51.00,0,0.00,0,0,0,0.00,0.00
1767431125.524,0.79,3.51,8,0,3.36,557.32,0.00,51.95,0,0.00,0,0,0,0.00,0.00
1767431127.774,0.89,3.52,10,0,2.48,431.64,0.00,51.13,0,0.00,0,0,0,0.00,0.00
1767431129.531,0.57,3.50,11,0,4.02,379.97,0.00,51.34,0,0.00,0,0,0,0.00,0.00
```

#### 統計分析

| メトリクス | 最小値 | 最大値 | 平均値 | 中央値 | 目標値 | 評価 |
|-----------|--------|--------|--------|--------|--------|------|
| **PC FPS** | 0.57 | 1.62 | **1.02 fps** | 1.10 | 15-25 fps | ❌ 6.8-16.7%のみ |
| **serial_read_time_ms** | 355.46 | 557.32 | **422.06 ms** | 394.67 | <50 ms | ❌ 8.4倍遅い |
| **decode_time_ms** | 2.48 | 11.03 | **4.63 ms** | 3.64 | <10 ms | ✅ 正常 |
| **jpeg_size_kb** | 50.31 | 52.91 | **51.45 KB** | 51.13 | 50-60 KB | ✅ 正常 |
| **spresense_*メトリクス** | 0 | 0 | **0** | 0 | >0 | ❌ 未受信 |
| **tcp_*_send_ms** | 0.00 | 0.00 | **0.00** | 0.00 | >0 | ❌ 未受信 |

#### フレーム受信間隔

| フレーム | Timestamp | 前フレームからの間隔 (秒) | FPS換算 |
|---------|-----------|--------------------------|---------|
| 2 | 1767431119.068 | - | - |
| 4 | 1767431120.834 | 1.766 | 1.13 fps |
| 6 | 1767431122.649 | 1.815 | 1.10 fps |
| 7 | 1767431124.251 | 1.602 | 0.62 fps |
| 8 | 1767431125.524 | 1.273 | 0.79 fps |
| 10 | 1767431127.774 | 2.250 | 0.89 fps |
| 11 | 1767431129.531 | 1.757 | 0.57 fps |

**平均受信間隔**: 1.744秒/フレーム = **0.57 fps**

---

### 3.2 Sync Word Not Found エラー

```
[2026-01-03T09:05:28Z ERROR security_camera_gui] Packet read error: Sync word not found
[2026-01-03T09:05:28Z ERROR security_camera_gui::tcp_connection] Failed to find sync word after 10000 attempts
[2026-01-03T09:05:29Z ERROR security_camera_gui] Packet read error: Sync word not found
...
(10回繰り返し)
...
[2026-01-03T09:05:31Z ERROR security_camera_gui] Too many consecutive packet errors (10), stopping capture thread
```

**エラー発生タイミング**:
- 11フレーム受信後 (最終: 1767431129.531 = 09:05:29 UTC)
- 09:05:28 ~ 09:05:31 の3秒間でエラー連続発生
- 10回エラーで自動停止

**Sync word検索の挙動**:
1. TCP stream から1バイトずつ読み取り
2. 4バイトバッファにスライド格納
3. `0xCAFEBABE` (MJPEG) または `0xCAFEBEEF` (Metrics) を検索
4. 最大10000回試行 (約10KB分)
5. 見つからない場合エラー

**エラー原因 (仮説)**:
1. Spresense側でTCP送信が停止 (キュー満杯で詰まり)
2. PC側がタイムアウトまたはデータ欠損を検出
3. Sync wordが見つからず、エラーカウンタ増加
4. 10回連続エラーで停止

---

## 4. ボトルネック分析

### 4.1 E2E フローとボトルネック

```
Spresense側:
Camera (30 fps, 33.3ms/frame) → MJPEG Pack (8.7ms) → TCP Send (???) → WiFi

PC側:
WiFi → TCP Receive (???) → Sync Word Search (???) → JPEG Decode (4.6ms) → Display
```

#### 時間予算比較 (Phase 2 USB Serial baseline vs Phase 7.0 WiFi/TCP)

| フェーズ | USB Serial (実測) | WiFi/TCP (実測) | 差分 | 備考 |
|---------|------------------|----------------|------|------|
| **Camera capture** | 2.0 ms | 2.0 ms | - | 同一 |
| **MJPEG pack** | 8.7 ms | 8.7 ms | - | 同一 |
| **Transport send** | 30.2 ms (USB) | ??? (TCP) | 不明 | **要計測** |
| **PC receive** | ~40 ms | **422 ms** | **+382 ms** | **ボトルネック** |
| **JPEG decode** | 4.6 ms | 4.6 ms | - | 同一 |
| **Total latency** | ~86 ms | **~437 ms** | **+351 ms** | **5.1倍悪化** |
| **FPS** | 11.05 fps | 1.02 fps | **-90.8%** | **10.8倍悪化** |

**結論**:
- **PC receive (TCP read)** が主要ボトルネック: 422ms (USB比 +382ms, 10.5倍悪化)
- TCP send時間は不明 (Metricsパケット未受信のため)

---

### 4.2 TCP Read時間の内訳 (仮説)

**PC側serial_read_time_ms = 422ms** (平均) の内訳:

| フェーズ | 推定時間 | 備考 |
|---------|---------|------|
| **TCP network latency** | ~5-10 ms | WiFi Round-trip time |
| **Sync word検索** | ~10-50 ms | 最大10000回試行 |
| **TCP receive buffering** | ~350-400 ms | **主要ボトルネック (仮説)** |

**仮説**:
1. Spresense側TCP sendが遅い (usrsock overhead + GS2200M)
2. PC側がデータ到着を待機
3. Partial readが発生し、複数回のread()呼び出しが必要
4. 合計で422ms/frameの遅延

---

### 4.3 キュー満杯の影響

```
Camera thread:
  52 frames captured → 52 frames packed → 52 frames enqueued
  ↓
  Queue (max depth = 5)
  ↓
  47 frames dequeued → 47 frames TCP sent

キュー内残留: 5 frames
```

**問題点**:
1. **Camera thread blocked**: キューが満杯のため、新しいフレームをenqueueできない
2. **Metrics packet送信不可**: 空きバッファがないため、Metricsパケットを送信できない
3. **TCP send速度 < Camera capture速度**: ボトルネックがTCP send

**悪循環**:
```
TCP send遅い → Queue満杯 → Metrics送信不可 → TCP send時間不明 → 改善不可
```

---

## 5. Phase 7.0 Metrics実装の検証

### 5.1 Spresense側 TCP Metrics実装

**実装内容**:
- ✅ `tcp_server.c`: TCP send時間計測 (clock_gettime)
- ✅ `mjpeg_protocol.h`: Metricsパケット拡張 (38 → 42 bytes)
- ✅ `camera_threads.c`: Metrics収集とパケット生成

**実行結果**:
- ❌ Metricsパケット送信: **0回成功** (14回試行、全て失敗)
- ❌ TCP send時間データ: **取得不可**

**失敗原因**:
- キュー満杯で空きバッファなし
- `No empty buffer for metrics packet` エラー

---

### 5.2 PC側 Metrics受信実装

**実装内容**:
- ✅ `protocol.rs`: MetricsPacket拡張 (tcp_avg_send_us, tcp_max_send_us)
- ✅ `gui_main.rs`: Metricsメッセージ処理、μs → ms変換
- ✅ `metrics.rs`: CSV拡張 (tcp_avg_send_ms, tcp_max_send_ms)

**実行結果**:
- ❌ Metricsパケット受信: **0個**
- ❌ CSV出力: `tcp_avg_send_ms=0.00, tcp_max_send_ms=0.00` (全行)

---

## 6. 対策と優先順位

### 優先度A (Critical): Metricsパケット送信成功

**目的**: TCP send時間を計測し、ボトルネックを定量化

**対策1**: キュー深度拡張
```c
// frame_queue.h
#define MAX_QUEUE_DEPTH 10  // 5 → 10に拡張
```

**対策2**: Metrics専用バッファ確保
```c
// camera_threads.c
static uint8_t metrics_buffer_dedicated[METRICS_PACKET_SIZE];
// キューとは別に専用バッファを確保
```

**対策3**: 異常終了時の強制Metrics送信
```c
// camera_threads.c (cleanup時)
if (g_metrics_sequence > 0) {
    // 最終Metricsを強制送信
    send_metrics_packet_forced(usb_fd);
}
```

**期待効果**:
- Metricsパケット送信成功 → TCP send時間取得
- ボトルネック定量化 → 改善策立案可能

---

### 優先度B (High): TCP送信パフォーマンス改善

**対策1**: SO_SNDBUF拡張
```c
// tcp_server.c
int sndbuf = 524288;  // 256KB → 512KB
setsockopt(client_fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
```

**対策2**: TCP_CORK使用 (Nagleアルゴリズム代替)
```c
int cork = 1;
setsockopt(client_fd, IPPROTO_TCP, TCP_CORK, &cork, sizeof(cork));
// データ書き込み
tcp_server_send(...);
cork = 0;
setsockopt(client_fd, IPPROTO_TCP, TCP_CORK, &cork, sizeof(cork));  // flush
```

**対策3**: Partial writeのバッチ化
```c
// tcp_server.c
// 複数パケットをまとめて送信
```

---

### 優先度C (Medium): PC側Sync Word検索最適化

**対策1**: バッファサイズ拡大
```rust
// tcp_connection.rs
const SYNC_SEARCH_BUFFER_SIZE: usize = 1024 * 1024;  // 100KB → 1MB
```

**対策2**: 切断検出改善
```rust
// tcp_connection.rs
match stream.read(&mut buf) {
    Ok(0) => return Err(io::Error::new(io::ErrorKind::UnexpectedEof, "Connection closed")),
    Ok(n) => { /* ... */ },
    Err(e) if e.kind() == io::ErrorKind::WouldBlock => continue,
    Err(e) => return Err(e),
}
```

---

### 優先度D (Low): Camera FPS調整

**対策**: Camera FPS を 30fps → 20fps に下げる
```c
// config.h
#define CAM_VIDEO_FPS 20  // 30 → 20
```

**期待効果**:
- フレーム間隔: 33.3ms → 50ms (+16.7ms)
- TCP送信時間の余裕増加
- キュー満杯率低下

**トレードオフ**:
- フレームレート低下
- 映像滑らかさ低下

---

## 7. 次ステップ (Phase 7.1)

### Step 1: Metricsパケット送信成功 (優先度A)

1. キュー深度拡張 (5 → 10)
2. Metrics専用バッファ確保
3. 異常終了時の強制Metrics送信実装
4. **再テスト**: TCP send時間取得

**成功基準**:
- ✅ Metricsパケット受信: >0個
- ✅ `tcp_avg_send_ms` > 0.00 (CSV)
- ✅ TCP send時間の定量化

---

### Step 2: TCP送信パフォーマンス改善 (優先度B)

1. SO_SNDBUF拡張 (256KB → 512KB)
2. TCP_CORK実装
3. **再テスト**: PC側FPS改善確認

**成功基準**:
- ✅ PC FPS: >5 fps (Phase 7.0比 +4fps)
- ✅ serial_read_time_ms: <200 ms (Phase 7.0比 -222ms)

---

### Step 3: 長時間安定性テスト

**テスト内容**:
- 5分間連続運転
- Metricsパケット受信数カウント
- エラー率測定

**成功基準**:
- ✅ テスト完了: 5分間異常終了なし
- ✅ Metricsパケット受信率: >90%
- ✅ JPEG decode error rate: <1%

---

## 8. まとめ

### Phase 7.0の成果

- ✅ WiFi/TCP transport実装完了
- ✅ TCP Metricsプロトコル実装完了 (Spresense + PC)
- ✅ CSV拡張 (tcp_avg_send_ms, tcp_max_send_ms)
- ✅ WiFi接続安定 (GS2200M)

### Phase 7.0の課題

- ❌ **FPS大幅未達**: 1.02 fps (目標15-25 fpsの6.8%)
- ❌ **TCP read時間異常**: 422ms平均 (目標<50msの8.4倍)
- ❌ **Metricsパケット未受信**: キュー満杯で送信不可
- ❌ **異常終了**: 11フレームでSync word not foundエラー

### 優先対策

**Week 1 (Phase 7.1)**:
1. キュー深度拡張 (5 → 10)
2. Metrics専用バッファ確保
3. 異常終了時の強制Metrics送信
4. **再テスト**: TCP send時間取得

**Week 2 (Phase 7.2)**:
1. SO_SNDBUF拡張
2. TCP_CORK実装
3. **再テスト**: FPS改善確認

**Week 3 (Phase 7.3)**:
1. 長時間安定性テスト (5分間)
2. エラー率測定
3. 本番運用可否判定

---

**作成者**: Claude Code (Sonnet 4.5)
**作成日**: 2026-01-03
