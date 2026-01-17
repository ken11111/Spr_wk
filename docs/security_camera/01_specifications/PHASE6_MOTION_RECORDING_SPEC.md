# Phase 6: 動体検知録画機能仕様

## 概要

Phase 6では、動体検知とプリバッファ録画機能を実装し、「動きがあった10秒前から」の録画を実現する。

## 機能要件

### 1. 動体検知

- フレーム間差分による動き検知
- 設定可能な検知感度（0.0 - 1.0）
- 検知時にログ出力

### 2. プリバッファ（リングバッファ）

- 常に最新N秒分のフレームをメモリに保持
- 動体検知時に過去フレームをファイルに書き込み
- デフォルト: 10秒 @ 11fps = 110フレーム

### 3. 録画フォーマット

| フォーマット | 拡張子 | 特徴 |
|--------------|--------|------|
| MJPEG | .mjpeg | 軽量、後でMP4変換可能 |
| MP4 | .mp4 | そのまま再生可能（ffmpeg使用） |

## 設計

### RingBuffer (ring_buffer.rs)

```rust
/// JPEGフレーム
#[derive(Clone)]
pub struct JpegFrame {
    /// JPEG画像データ
    pub jpeg_data: Vec<u8>,
    /// 受信時刻
    pub timestamp: Instant,
}

/// リングバッファ
pub struct RingBuffer {
    /// フレームキュー（古い順）
    frames: VecDeque<JpegFrame>,
    /// 最大フレーム数
    capacity: usize,
    /// 現在のバッファ内総バイト数
    total_bytes: usize,
}
```

#### 主要メソッド

| メソッド | 説明 |
|----------|------|
| `new(capacity)` | 指定容量でバッファ作成 |
| `from_seconds(seconds, fps)` | 秒数とFPSから容量計算 |
| `push(frame)` | フレーム追加（古いフレームは自動削除） |
| `flush_to_file(file)` | 全フレームをファイルに書き込み（MJPEG用） |
| `iter_frames()` | フレームをイテレート（MP4用） |
| `clear()` | バッファクリア |

### 録画開始フロー

```
┌────────────────────────────────────────────────────────────────┐
│ 動体検知発生                                                    │
├────────────────────────────────────────────────────────────────┤
│                                                                │
│  ┌──────────────────┐                                          │
│  │ 録画フォーマット │                                          │
│  │ チェック         │                                          │
│  └────────┬─────────┘                                          │
│           │                                                    │
│     ┌─────┴─────┐                                              │
│     │           │                                              │
│     ▼           ▼                                              │
│  MJPEG        MP4                                              │
│     │           │                                              │
│     ▼           ▼                                              │
│  ┌──────────┐  ┌──────────────────────────┐                    │
│  │ファイル  │  │ Mp4Recorder作成          │                    │
│  │作成      │  │ (ffmpegプロセス起動)     │                    │
│  └────┬─────┘  └────────────┬─────────────┘                    │
│       │                     │                                  │
│       ▼                     ▼                                  │
│  ┌──────────┐  ┌──────────────────────────┐                    │
│  │flush_to_ │  │ iter_frames()で          │                    │
│  │file()    │  │ 各フレームをwrite_frame()│                    │
│  │(一括書込)│  │ (個別書込)               │                    │
│  └──────────┘  └──────────────────────────┘                    │
│                                                                │
│  → プリバッファ完了、以降は継続録画                            │
│                                                                │
└────────────────────────────────────────────────────────────────┘
```

---

## Phase 6.1: MP4プリバッファ実装 (2026-01-17)

### 背景

Phase 6の初期実装では、MP4録画時のプリバッファが未実装だった：

```rust
// 旧実装 (gui_main.rs)
RecordingFormat::Mp4 => {
    // TODO: MJPEGファイルを読み込んで個別フレームとしてMP4に書き込む処理
    // 現在の実装では、プリバッファはスキップ（MP4の場合）
    warn!("MP4 motion recording: pre-buffer not yet implemented");
    (0, 0)  // プリバッファは未実装
}
```

### 問題

- MP4録画では動体検知時点からの録画になっていた
- 「10秒前から録画」の要件を満たしていなかった
- MJPEGとMP4で動作が異なっていた

### 解決策

`RingBuffer`に`iter_frames()`メソッドを追加し、個別フレームへのアクセスを可能にした。

#### 変更1: ring_buffer.rs

```rust
impl RingBuffer {
    /// バッファ内のフレームをイテレート（古い順）
    ///
    /// MP4録画のプリバッファ書き込みなど、個別フレームへのアクセスが必要な場合に使用。
    pub fn iter_frames(&self) -> impl Iterator<Item = &JpegFrame> {
        self.frames.iter()
    }
}
```

#### 変更2: gui_main.rs

```rust
RecordingFormat::Mp4 => {
    let mut recorder = Mp4Recorder::new(&filepath, 11)?;

    // Write pre-buffer frames to MP4
    let mut pre_frame_count = 0;
    let mut pre_byte_count = 0;

    // Ring bufferから各フレームを取得してMP4に書き込む
    for frame in self.ring_buffer.iter_frames() {
        if let Err(e) = recorder.write_frame(&frame.jpeg_data) {
            warn!("Failed to write pre-buffer frame to MP4: {}", e);
            break;
        }
        pre_frame_count += 1;
        pre_byte_count += frame.jpeg_data.len();
    }

    info!("Started motion MP4 recording to: {:?}", filepath);
    info!("  Pre-buffer: {} frames, {:.2} MB",
          pre_frame_count, pre_byte_count as f32 / 1_000_000.0);
    self.mp4_recorder = Some(recorder);
    (pre_frame_count, pre_byte_count)
}
```

### 効果

| 項目 | Before | After |
|------|--------|-------|
| MJPEG プリバッファ | 動作 | 動作 |
| MP4 プリバッファ | 未実装（0フレーム） | 動作（110フレーム） |
| 録画開始位置 | MP4は検知時点から | 両方とも10秒前から |

### ログ出力例

```
[INFO] Started motion MP4 recording to: "recordings/motion_20260117_123456.mp4"
[INFO]   Pre-buffer: 110 frames, 5.72 MB
```

---

## 設定項目

### MotionDetectionConfig

```rust
pub struct MotionDetectionConfig {
    /// 検知感度 (0.0 - 1.0)
    pub sensitivity: f32,
    /// プリバッファ秒数
    pub pre_record_seconds: u32,
    /// ポストバッファ秒数（動き終了後の継続録画）
    pub post_record_seconds: u32,
}
```

### デフォルト値

| 項目 | 値 |
|------|-----|
| sensitivity | 0.1 |
| pre_record_seconds | 10 |
| post_record_seconds | 5 |
| FPS (録画) | 11 |

## ファイル構成

```
src/
├── ring_buffer.rs      # リングバッファ実装
├── motion_detector.rs  # 動体検知ロジック
├── mp4_recorder.rs     # MP4録画（ffmpeg連携）
└── gui_main.rs         # GUI統合・録画制御
```

---

**Document Version**: 1.1
**Last Updated**: 2026-01-17
**Author**: Claude Opus 4.5
**Status**: IMPLEMENTED
