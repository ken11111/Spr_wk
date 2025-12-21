# Phase 2: PC側 Rust 実装ガイド

**日付**: 2025-12-15
**プロジェクト**: セキュリティカメラ - PC側受信アプリケーション
**開発場所**: `/home/ken/Rust_ws/`
**言語**: Rust

---

## 📋 Phase 2 概要

### 目的
Spresense (Phase 1) から USB CDC-ACM 経由で送信される H.264 映像を受信・表示・保存する PC アプリケーションを実装する。

### 主な機能
1. USB CDC-ACM シリアル通信 (115200 bps)
2. カスタムプロトコルパケット受信・解析
3. CRC16 検証
4. H.264 NAL ユニット再構築
5. リアルタイム映像表示
6. MP4/MKV ファイル保存

---

## 🚀 開発環境セットアップ

### 1. Rust_ws ディレクトリ準備

```bash
# 作業ディレクトリ作成
mkdir -p /home/ken/Rust_ws
cd /home/ken/Rust_ws

# Rust プロジェクト作成
cargo new security_camera_viewer --bin
cd security_camera_viewer
```

### 2. 必要な Rust クレート

`Cargo.toml` に追加:

```toml
[package]
name = "security_camera_viewer"
version = "0.1.0"
edition = "2021"

[dependencies]
# シリアル通信
serialport = "4.5"

# バイトバッファ操作
bytes = "1.5"
byteorder = "1.5"

# CRC計算
crc = "3.0"

# H.264デコード (オプション: 表示機能用)
ffmpeg-next = { version = "7.0", optional = true }

# ファイルI/O
tokio = { version = "1.35", features = ["full"] }

# ログ
log = "0.4"
env_logger = "0.11"

# エラーハンドリング
anyhow = "1.0"
thiserror = "1.0"

# CLI引数解析
clap = { version = "4.4", features = ["derive"] }

[features]
default = []
gui = ["ffmpeg-next"]  # GUI表示機能 (オプション)
```

---

## 📦 プロジェクト構造

```
/home/ken/Rust_ws/security_camera_viewer/
├── Cargo.toml
├── Cargo.lock
├── README.md
├── .gitignore
└── src/
    ├── main.rs              # エントリーポイント
    ├── protocol.rs          # プロトコル定義・パーサ
    ├── serial.rs            # USB CDC-ACM通信
    ├── decoder.rs           # H.264 NALユニット処理
    ├── recorder.rs          # ファイル保存
    └── viewer.rs (optional) # リアルタイム表示
```

---

## 🔧 実装ステップ

### Step 1: プロトコル定義 (`src/protocol.rs`)

```rust
use byteorder::{LittleEndian, ReadBytesExt, WriteBytesExt};
use std::io::{self, Read};

// パケットマジックナンバー
pub const PACKET_MAGIC: u16 = 0x5350; // 'SP'
pub const PACKET_VERSION: u8 = 0x01;

// パケットタイプ
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PacketType {
    Handshake = 0x01,
    VideoSPS = 0x10,
    VideoPPS = 0x11,
    VideoIDR = 0x12,
    VideoSlice = 0x13,
    Heartbeat = 0x20,
    Error = 0xFF,
}

impl TryFrom<u8> for PacketType {
    type Error = ();
    fn try_from(value: u8) -> Result<Self, Self::Error> {
        match value {
            0x01 => Ok(PacketType::Handshake),
            0x10 => Ok(PacketType::VideoSPS),
            0x11 => Ok(PacketType::VideoPPS),
            0x12 => Ok(PacketType::VideoIDR),
            0x13 => Ok(PacketType::VideoSlice),
            0x20 => Ok(PacketType::Heartbeat),
            0xFF => Ok(PacketType::Error),
            _ => Err(()),
        }
    }
}

// パケットヘッダー (22 bytes)
#[derive(Debug, Clone)]
pub struct PacketHeader {
    pub magic: u16,
    pub version: u8,
    pub packet_type: PacketType,
    pub sequence: u32,
    pub timestamp_us: u64,
    pub payload_size: u32,
    pub checksum: u16,
}

impl PacketHeader {
    pub const SIZE: usize = 22;

    pub fn parse(buf: &[u8]) -> io::Result<Self> {
        if buf.len() < Self::SIZE {
            return Err(io::Error::new(
                io::ErrorKind::UnexpectedEof,
                "Buffer too small for header",
            ));
        }

        let mut cursor = io::Cursor::new(buf);

        let magic = cursor.read_u16::<LittleEndian>()?;
        if magic != PACKET_MAGIC {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                format!("Invalid magic: 0x{:04X}", magic),
            ));
        }

        let version = cursor.read_u8()?;
        if version != PACKET_VERSION {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                format!("Invalid version: 0x{:02X}", version),
            ));
        }

        let packet_type = PacketType::try_from(cursor.read_u8()?)
            .map_err(|_| io::Error::new(io::ErrorKind::InvalidData, "Invalid packet type"))?;

        Ok(PacketHeader {
            magic,
            version,
            packet_type,
            sequence: cursor.read_u32::<LittleEndian>()?,
            timestamp_us: cursor.read_u64::<LittleEndian>()?,
            payload_size: cursor.read_u32::<LittleEndian>()?,
            checksum: cursor.read_u16::<LittleEndian>()?,
        })
    }
}

// パケット全体
#[derive(Debug, Clone)]
pub struct Packet {
    pub header: PacketHeader,
    pub payload: Vec<u8>,
}

impl Packet {
    pub fn parse(buf: &[u8]) -> io::Result<Self> {
        let header = PacketHeader::parse(buf)?;

        let total_size = PacketHeader::SIZE + header.payload_size as usize;
        if buf.len() < total_size {
            return Err(io::Error::new(
                io::ErrorKind::UnexpectedEof,
                "Buffer too small for payload",
            ));
        }

        let payload = buf[PacketHeader::SIZE..total_size].to_vec();

        // CRC16検証
        let calculated_crc = crc16_ibm_sdlc(&payload);
        if calculated_crc != header.checksum {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                format!("CRC mismatch: expected 0x{:04X}, got 0x{:04X}",
                        header.checksum, calculated_crc),
            ));
        }

        Ok(Packet { header, payload })
    }
}

// CRC16-IBM-SDLC計算
pub fn crc16_ibm_sdlc(data: &[u8]) -> u16 {
    let mut crc: u16 = 0xFFFF;

    for &byte in data {
        crc ^= byte as u16;
        for _ in 0..8 {
            if crc & 0x0001 != 0 {
                crc = (crc >> 1) ^ 0x8408;
            } else {
                crc >>= 1;
            }
        }
    }

    crc ^ 0xFFFF
}

// Handshakeペイロード
#[derive(Debug, Clone)]
pub struct HandshakePayload {
    pub video_width: u16,
    pub video_height: u16,
    pub fps: u8,
    pub codec: u8,
    pub bitrate: u32,
}

impl HandshakePayload {
    pub fn parse(data: &[u8]) -> io::Result<Self> {
        let mut cursor = io::Cursor::new(data);
        Ok(HandshakePayload {
            video_width: cursor.read_u16::<LittleEndian>()?,
            video_height: cursor.read_u16::<LittleEndian>()?,
            fps: cursor.read_u8()?,
            codec: cursor.read_u8()?,
            bitrate: cursor.read_u32::<LittleEndian>()?,
        })
    }
}
```

### Step 2: シリアル通信 (`src/serial.rs`)

```rust
use serialport::{SerialPort, SerialPortType};
use std::io::{self, Read};
use std::time::Duration;
use log::{info, warn, error};

pub struct SerialConnection {
    port: Box<dyn SerialPort>,
    buffer: Vec<u8>,
}

impl SerialConnection {
    pub fn open(port_name: &str, baud_rate: u32) -> io::Result<Self> {
        info!("Opening serial port: {} @ {} bps", port_name, baud_rate);

        let port = serialport::new(port_name, baud_rate)
            .timeout(Duration::from_millis(1000))
            .open()?;

        info!("Serial port opened successfully");

        Ok(SerialConnection {
            port,
            buffer: Vec::with_capacity(8192),
        })
    }

    pub fn auto_detect() -> io::Result<Self> {
        let ports = serialport::available_ports()?;

        for port in ports {
            match &port.port_type {
                SerialPortType::UsbPort(info) => {
                    // Spresense VID/PID: 0x054C/0x0BC2
                    if info.vid == 0x054C && info.pid == 0x0BC2 {
                        info!("Found Spresense device: {}", port.port_name);
                        return Self::open(&port.port_name, 115200);
                    }
                }
                _ => {}
            }
        }

        Err(io::Error::new(
            io::ErrorKind::NotFound,
            "Spresense device not found",
        ))
    }

    pub fn read_bytes(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        self.port.read(buf)
    }

    pub fn read_packet(&mut self) -> io::Result<crate::protocol::Packet> {
        use crate::protocol::{Packet, PacketHeader};

        // ヘッダー読み込み
        let mut header_buf = [0u8; PacketHeader::SIZE];
        self.port.read_exact(&mut header_buf)?;

        let header = PacketHeader::parse(&header_buf)?;

        // ペイロード読み込み
        let mut payload = vec![0u8; header.payload_size as usize];
        self.port.read_exact(&mut payload)?;

        // パケット構築・検証
        let mut full_packet = Vec::with_capacity(PacketHeader::SIZE + payload.len());
        full_packet.extend_from_slice(&header_buf);
        full_packet.extend_from_slice(&payload);

        Packet::parse(&full_packet)
    }
}
```

### Step 3: メイン処理 (`src/main.rs`)

```rust
mod protocol;
mod serial;

use clap::Parser;
use log::{info, warn, error};
use std::fs::File;
use std::io::Write;
use anyhow::{Result, Context};

#[derive(Parser, Debug)]
#[command(author, version, about, long_about = None)]
struct Args {
    /// Serial port path (e.g., /dev/ttyACM0)
    #[arg(short, long)]
    port: Option<String>,

    /// Output file path
    #[arg(short, long, default_value = "output.h264")]
    output: String,

    /// Enable verbose logging
    #[arg(short, long)]
    verbose: bool,
}

fn main() -> Result<()> {
    let args = Args::parse();

    // ログ初期化
    if args.verbose {
        env_logger::Builder::from_default_env()
            .filter_level(log::LevelFilter::Debug)
            .init();
    } else {
        env_logger::Builder::from_default_env()
            .filter_level(log::LevelFilter::Info)
            .init();
    }

    info!("Security Camera Viewer starting...");

    // シリアルポート接続
    let mut serial = if let Some(port) = args.port {
        serial::SerialConnection::open(&port, 115200)?
    } else {
        info!("Auto-detecting Spresense device...");
        serial::SerialConnection::auto_detect()?
    };

    // 出力ファイル
    let mut output_file = File::create(&args.output)
        .context("Failed to create output file")?;
    info!("Output file: {}", args.output);

    // ハンドシェイク受信
    info!("Waiting for handshake...");
    let handshake = loop {
        match serial.read_packet() {
            Ok(packet) => {
                if packet.header.packet_type == protocol::PacketType::Handshake {
                    let hs = protocol::HandshakePayload::parse(&packet.payload)?;
                    info!("Handshake received: {}x{} @ {}fps, codec={}, bitrate={}",
                          hs.video_width, hs.video_height, hs.fps, hs.codec, hs.bitrate);
                    break hs;
                }
            }
            Err(e) => {
                warn!("Handshake read error: {}", e);
                continue;
            }
        }
    };

    // パケット受信ループ
    info!("Receiving video packets...");
    let mut frame_count = 0u64;
    let mut error_count = 0u32;

    loop {
        match serial.read_packet() {
            Ok(packet) => {
                error_count = 0; // エラーカウントリセット

                match packet.header.packet_type {
                    protocol::PacketType::VideoSPS |
                    protocol::PacketType::VideoPPS |
                    protocol::PacketType::VideoIDR |
                    protocol::PacketType::VideoSlice => {
                        // H.264 NALユニットをファイルに書き込み
                        // Start code (0x00 0x00 0x00 0x01) を追加
                        output_file.write_all(&[0x00, 0x00, 0x00, 0x01])?;
                        output_file.write_all(&packet.payload)?;

                        if packet.header.packet_type == protocol::PacketType::VideoIDR {
                            frame_count += 1;
                            if frame_count % 30 == 0 {
                                info!("Received {} frames (seq={})",
                                      frame_count, packet.header.sequence);
                            }
                        }
                    }
                    _ => {
                        warn!("Unexpected packet type: {:?}", packet.header.packet_type);
                    }
                }
            }
            Err(e) => {
                error_count += 1;
                error!("Packet read error ({}): {}", error_count, e);

                if error_count >= 10 {
                    error!("Too many errors, exiting");
                    break;
                }
            }
        }
    }

    info!("Total frames received: {}", frame_count);
    info!("Output saved to: {}", args.output);

    Ok(())
}
```

---

## 🎯 実装の進め方

### Phase 2-1: 基本プロトコル実装 (1-2時間)
```bash
cd /home/ken/Rust_ws/security_camera_viewer

# プロトコル定義
vim src/protocol.rs

# ビルド確認
cargo build

# テスト (ユニットテスト追加推奨)
cargo test
```

### Phase 2-2: シリアル通信実装 (1-2時間)
```bash
# シリアル通信実装
vim src/serial.rs

# ビルド
cargo build

# デバイス検出テスト
cargo run -- --verbose
```

### Phase 2-3: 統合テスト (1時間)
```bash
# Spresense接続
# (Spresenseにファームウェア書き込み済み)

# アプリケーション起動
cargo run -- --port /dev/ttyACM0 --output test.h264 --verbose

# 映像確認
ffplay test.h264
# または
vlc test.h264
```

### Phase 2-4: 機能拡張 (オプション)
- リアルタイム表示 (ffmpeg-next)
- MP4コンテナ保存 (mp4-rust)
- GUI (egui, iced)
- 統計情報表示 (fps, bitrate, packet loss)

---

## 🔍 デバッグ方法

### シリアルポート確認
```bash
# デバイス一覧
ls -l /dev/ttyACM*
ls -l /dev/ttyUSB*

# Spresense VID/PID確認
lsusb | grep 054c:0bc2

# パーミッション設定
sudo usermod -a -G dialout $USER
# ログアウト・ログイン必要
```

### パケットキャプチャ
```bash
# 生データ保存
cat /dev/ttyACM0 > raw_capture.bin

# hexdump確認
hexdump -C raw_capture.bin | head -50

# マジックナンバー検索
grep -abo $'\x50\x53' raw_capture.bin
```

### Rust デバッグログ
```bash
# 詳細ログ
RUST_LOG=debug cargo run -- --port /dev/ttyACM0 --verbose

# トレースレベル
RUST_LOG=trace cargo run -- --port /dev/ttyACM0 --verbose
```

---

## 📊 期待される動作

### 正常時のログ出力例
```
[INFO] Security Camera Viewer starting...
[INFO] Opening serial port: /dev/ttyACM0 @ 115200 bps
[INFO] Serial port opened successfully
[INFO] Waiting for handshake...
[INFO] Handshake received: 1280x720 @ 30fps, codec=1, bitrate=2000000
[INFO] Receiving video packets...
[INFO] Received 30 frames (seq=120)
[INFO] Received 60 frames (seq=240)
[INFO] Received 90 frames (seq=360)
^C
[INFO] Total frames received: 95
[INFO] Output saved to: output.h264
```

### 映像ファイル再生
```bash
# VLC
vlc output.h264

# ffplay
ffplay output.h264

# MP4変換
ffmpeg -i output.h264 -c copy output.mp4
```

---

## ⚠️ 注意事項

### 1. シリアルポートアクセス権限
```bash
# dialoutグループに追加
sudo usermod -a -G dialout $USER

# または一時的に
sudo chmod 666 /dev/ttyACM0
```

### 2. ボーレート設定
- Spresense CDC-ACM: 115200 bps (デフォルト)
- 高速転送時は baudrate に依存しない (USB full speed)

### 3. バッファサイズ
- USB CDC-ACM: 64バイトパケット
- カーネルバッファ: 通常4KB
- アプリケーションバッファ: 8KB推奨

### 4. タイムアウト
- 読み込みタイムアウト: 1秒
- フレーム間隔: 33ms (30fps)
- 再接続タイムアウト: 10秒

---

## 🧪 テストシナリオ

### Test 1: ハンドシェイク
1. アプリケーション起動
2. Spresense接続
3. ハンドシェイク受信確認
4. 設定情報表示

### Test 2: 映像受信
1. パケット受信開始
2. SPS/PPS/IDR/SLICE 受信確認
3. CRC16検証成功
4. ファイル保存

### Test 3: エラー処理
1. USB切断 → 再接続
2. CRCエラー → スキップ
3. シーケンス番号ギャップ → 警告

### Test 4: 長時間動作
1. 1分間連続受信 (1800フレーム)
2. メモリリークなし
3. パケットロス率 < 1%

---

## 📚 参考リソース

### Rustクレート
- [serialport](https://docs.rs/serialport/) - シリアル通信
- [bytes](https://docs.rs/bytes/) - バイトバッファ
- [crc](https://docs.rs/crc/) - CRC計算
- [ffmpeg-next](https://docs.rs/ffmpeg-next/) - H.264デコード

### H.264仕様
- ITU-T H.264 / ISO/IEC 14496-10
- NAL Unit構造
- Start Code (0x00000001)

### USB CDC-ACM
- USB Communication Device Class
- Abstract Control Model
- Linux: /dev/ttyACMX

---

## ✅ Phase 2 完了基準

- [ ] Rustプロジェクト作成
- [ ] プロトコルパーサ実装
- [ ] CRC16検証実装
- [ ] シリアル通信実装
- [ ] ハンドシェイク処理
- [ ] H.264ファイル保存
- [ ] エラーハンドリング
- [ ] 統合テスト成功 (30秒以上の連続受信)
- [ ] 映像再生確認 (VLC/ffplay)

---

**作成日**: 2025-12-15
**作成者**: Claude Code (Sonnet 4.5)
**次のステップ**: `/home/ken/Rust_ws/` でRustプロジェクトを開始
