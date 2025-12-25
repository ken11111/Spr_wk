# 性能検証ログ仕様書

**Version**: 1.0
**Date**: 2025-12-25
**Target**: Phase 1.5 VGA実装

---

## 1. 概要

Phase 1.5 VGA実装における帯域幅とレイテンシの性能検証のため、詳細な性能ログ機能を実装しました。本ドキュメントでは、性能ログの仕様、出力フォーマット、および使用方法を説明します。

---

## 2. 実装ファイル

### 2.1 新規追加ファイル

| ファイル | 説明 |
|---------|------|
| `perf_logger.h` | 性能ログAPI定義 |
| `perf_logger.c` | 性能ログ実装 |

### 2.2 修正ファイル

| ファイル | 修正内容 |
|---------|---------|
| `camera_app_main.c` | 性能測定タイムスタンプの記録 |
| `Makefile` | `perf_logger.c`をビルドに追加 |

---

## 3. E2E通信経路と計測ポイント

```
┌─────────────────────────────────────────────────────────────────┐
│                    Spresense (CXD5602)                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ISX012 Camera (JPEG HW Encoder)                               │
│         │                                                       │
│         ▼  CSI-2                                                │
│  ┌──────────────────────────────────────┐                      │
│  │ [1] Camera Capture                   │                      │
│  │  - poll() wait (ts_camera_poll)      │  ← 計測ポイント1    │
│  │  - VIDIOC_DQBUF (ts_camera_dqbuf)    │  ← 計測ポイント2    │
│  │  - JPEG frame size                   │                      │
│  └──────────────┬───────────────────────┘                      │
│                 │ Memory (Triple buffer: 128KB × 3)            │
│                 ▼                                               │
│  ┌──────────────────────────────────────┐                      │
│  │ [2] MJPEG Packet Creation            │                      │
│  │  - mjpeg_pack_frame() (ts_pack)      │  ← 計測ポイント3    │
│  │  - Header + CRC calculation          │                      │
│  │  - Packet size (JPEG + overhead)     │                      │
│  └──────────────┬───────────────────────┘                      │
│                 │                                               │
│                 ▼                                               │
│  ┌──────────────────────────────────────┐                      │
│  │ [3] USB CDC-ACM Transmission         │                      │
│  │  - write() to /dev/ttyACM0           │  ← 計測ポイント4    │
│  │  - Retry count (ts_usb_write)        │                      │
│  │  - Actual bytes written              │                      │
│  └──────────────┬───────────────────────┘                      │
│                 │                                               │
└─────────────────┼───────────────────────────────────────────────┘
                  │
                  │  USB 2.0 Full Speed (12 Mbps)
                  │  ⚠️ ボトルネック
                  │
┌─────────────────┼───────────────────────────────────────────────┐
│                 ▼        PC (Linux)                             │
│  ┌──────────────────────────────────────┐                      │
│  │ Rust Application                     │                      │
│  │  - /dev/ttyACM0 read                 │                      │
│  │  - MJPEG packet parser               │                      │
│  └──────────────────────────────────────┘                      │
└─────────────────────────────────────────────────────────────────┘
```

---

## 4. 計測メトリクス

### 4.1 フレーム単位メトリクス

| メトリクス | 説明 | 単位 |
|-----------|------|------|
| `frame_num` | フレーム番号 | - |
| `jpeg_size` | JPEGフレームサイズ | bytes |
| `packet_size` | MJPEGパケットサイズ (ヘッダ+CRC含む) | bytes |
| `usb_written` | USB実送信バイト数 | bytes |
| `latency_camera_poll` | カメラpoll()待機時間 | μs |
| `latency_camera_dqbuf` | VIDIOC_DQBUF実行時間 | μs |
| `latency_pack` | MJPEGパケット化時間 | μs |
| `latency_usb_write` | USB write()実行時間 | μs |
| `latency_total` | フレーム処理総時間 | μs |
| `interval_us` | フレーム間隔 | μs |
| `usb_retry_count` | USBリトライ回数 | count |

### 4.2 集計メトリクス（30フレーム毎）

| メトリクス | 説明 | 単位 |
|-----------|------|------|
| `fps` | 実測フレームレート | fps |
| `avg_jpeg_kb` | 平均JPEGサイズ | KB |
| `min_jpeg_size` | 最小JPEGサイズ | bytes |
| `max_jpeg_size` | 最大JPEGサイズ | bytes |
| `throughput_mbps_jpeg` | JPEG帯域幅 | Mbps |
| `throughput_mbps_usb` | USB帯域幅（オーバーヘッド含む） | Mbps |
| `usb_utilization` | USB帯域使用率 (対12 Mbps) | % |
| `avg_latency_camera` | 平均カメラ取得時間 | μs |
| `avg_latency_pack` | 平均パケット化時間 | μs |
| `avg_latency_usb` | 平均USB送信時間 | μs |
| `avg_latency_total` | 平均総処理時間 | μs |
| `avg_interval` | 平均フレーム間隔 | μs |
| `min_interval` | 最小フレーム間隔 | μs |
| `max_interval` | 最大フレーム間隔 | μs |
| `total_usb_retries` | USB総リトライ回数 | count |

---

## 5. ログ出力例

### 5.1 フレーム単位ログ（30フレーム毎）

```
[CAM] [PERF] Frame 30: JPEG=58234 B, Pkt=58248 B, Lat(cam/pkt/usb/tot)=8234/156/2345/11567 us, Interval=33421 us
[CAM] [PERF] Frame 60: JPEG=62103 B, Pkt=62117 B, Lat(cam/pkt/usb/tot)=7892/164/2512/11892 us, Interval=33387 us
```

**解説**:
- `Frame 30`: 30フレーム目
- `JPEG=58234 B`: JPEG圧縮後サイズ 58.2 KB
- `Pkt=58248 B`: MJPEGパケットサイズ（ヘッダ12B + CRC2B = +14B）
- `Lat(cam/pkt/usb/tot)=8234/156/2345/11567 us`:
  - カメラ取得: 8.2 ms
  - パケット化: 0.16 ms
  - USB送信: 2.3 ms
  - 合計: 11.6 ms
- `Interval=33421 us`: フレーム間隔 33.4 ms（目標: 33.3 ms @ 30fps）

### 5.2 集計統計ログ（30フレーム毎）

```
[CAM] ========================================================
[CAM] [PERF STATS] Window: 30 frames in 1.00 sec (30.00 fps)
[CAM] --------------------------------------------------------
[CAM] [SIZE] JPEG: avg=59.45 KB, min=52341 B, max=68923 B
[CAM] [SIZE] Packet (w/overhead): avg=59.46 KB
[CAM] --------------------------------------------------------
[CAM] [THROUGHPUT] JPEG data: 14.27 Mbps
[CAM] [THROUGHPUT] USB (w/overhead): 14.30 Mbps
[CAM] [USB] Utilization: 119.2% of 12 Mbps Full Speed
[CAM] [USB] ⚠️  BANDWIDTH EXCEEDED! Target: <100%, Actual: 119.2%
[CAM] [USB] Recommend: Reduce FPS or JPEG quality
[CAM] --------------------------------------------------------
[CAM] [LATENCY] Camera: avg=8234 us
[CAM] [LATENCY] Pack: avg=156 us
[CAM] [LATENCY] USB Write: avg=2345 us
[CAM] [LATENCY] Total: avg=11567 us
[CAM] --------------------------------------------------------
[CAM] [INTERVAL] Frame: avg=33333 us, min=32987 us, max=34012 us
[CAM] [INTERVAL] Target: 33333 us (30 fps)
[CAM] --------------------------------------------------------
[CAM] [ERRORS] USB retries: 0 (0.00 per frame)
[CAM] [ERRORS] Camera timeouts: 0
[CAM] [ERRORS] Dropped frames: 0
[CAM] ========================================================
```

**重要な警告**:
- USB帯域幅が119.2%（12 Mbpsを超過）している場合、警告が表示されます
- この場合、フレームレート削減（30fps → 20fps）またはJPEG品質調整が必要です

### 5.3 帯域幅計算例

**VGA @ 30fps, 平均JPEG 60 KB/frame の場合**:

```
JPEG帯域幅 = 60 KB/frame × 30 fps × 8 bits/byte
          = 14.4 Mbps

USB帯域幅（オーバーヘッド含む）:
  - MJPEGヘッダ: 12 bytes
  - CRC: 2 bytes
  - 合計: 60014 bytes/frame

USB帯域幅 = 60014 bytes/frame × 30 fps × 8 bits/byte
          = 14.4 Mbps

USB使用率 = 14.4 Mbps / 12 Mbps × 100%
          = 120%  ← ⚠️ 帯域超過
```

---

## 6. 使用方法

### 6.1 有効化/無効化

`perf_logger.h`で制御:

```c
/* Performance logging enable/disable */
#define PERF_LOGGING_ENABLED         1    /* 1=有効, 0=無効 */
```

### 6.2 ログ出力間隔の変更

デフォルト: 30フレーム毎（30fps時は1秒毎）

```c
/* Performance log interval (frames) */
#define PERF_LOG_INTERVAL_FRAMES     30    /* 変更可能 */
```

### 6.3 ログ取得方法

#### Spresense側（シリアルコンソール）

```bash
# Spresenseに接続してログ取得
picocom -b 115200 /dev/ttyUSB1
```

#### PC側（ファイル保存）

```bash
# ログをファイルに保存
picocom -b 115200 /dev/ttyUSB1 | tee spresense_perf.log
```

---

## 7. テスト計画

### 7.1 VGA性能検証項目

| 検証項目 | 目標値 | 測定方法 |
|---------|-------|---------|
| 平均JPEGサイズ | 50-80 KB | `avg_jpeg_kb`を確認 |
| USB帯域使用率 | <100% (12 Mbps以下) | `usb_utilization`を確認 |
| フレームレート | 30 fps | `fps`を確認 |
| フレーム間隔 | 33.3 ± 1 ms | `avg_interval`を確認 |
| USB送信時間 | <3 ms | `avg_latency_usb`を確認 |
| USBリトライ | 0回/frame | `total_usb_retries`を確認 |

### 7.2 帯域超過時の対策

USB帯域幅が12 Mbpsを超過した場合の対策:

**Option 1**: フレームレート調整
```c
// config.h
#define CONFIG_CAMERA_FPS            20    /* 30 → 20fps */
```

**Option 2**: JPEG品質調整（ISX012カメラ設定）
- JPEG品質パラメータを下げてファイルサイズを削減

**Option 3**: 解像度調整（緊急時）
- VGA (640×480) → QVGA (320×240) に一時的に戻す

---

## 8. 性能データ分析

### 8.1 ログからのデータ抽出

```bash
# USB帯域使用率のみ抽出
grep "USB.*Utilization" spresense_perf.log

# 平均JPEGサイズのみ抽出
grep "SIZE.*JPEG:" spresense_perf.log | awk '{print $6}'

# 警告のみ抽出
grep "⚠️" spresense_perf.log
```

### 8.2 期待される性能プロファイル

**正常時（QVGA @ 30fps）**:
- 平均JPEG: 20-25 KB
- USB帯域: 4.8-6.0 Mbps (40-50%)
- フレーム間隔: 33.3 ms ± 0.5 ms
- USBリトライ: 0回

**VGA @ 30fps（帯域限界）**:
- 平均JPEG: 50-80 KB
- USB帯域: 12-19 Mbps (100-160%) ← **帯域超過の可能性**
- フレーム間隔: 33.3 ms ± 1 ms
- USBリトライ: 増加の可能性

**VGA @ 20fps（調整後）**:
- 平均JPEG: 50-80 KB
- USB帯域: 8-13 Mbps (67-108%)
- フレーム間隔: 50 ms ± 1 ms
- USBリトライ: 0回（期待）

---

## 9. まとめ

Phase 1.5 VGA実装において、E2E通信経路の各段階で詳細な性能測定を実施できる性能ログシステムを実装しました。

**主な機能**:
- カメラ取得、パケット化、USB送信の各段階のレイテンシ測定
- リアルタイムUSB帯域使用率の監視
- 帯域超過時の自動警告
- フレーム間隔の精密測定

**使用目的**:
1. VGA @ 30fpsがUSB Full Speed (12 Mbps)の制約内で動作可能かの検証
2. 帯域超過時のボトルネック特定
3. フレームレート調整またはJPEG品質調整の判断材料

**次のステップ**:
- Spresense実機でVGA @ 30fpsテストを実行
- 性能ログを収集・分析
- 必要に応じてフレームレート/品質を調整
- Phase 1.5完了基準（USB帯域<100%）を達成
