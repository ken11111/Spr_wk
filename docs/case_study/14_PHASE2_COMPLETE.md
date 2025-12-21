# Phase 2: PC側Rust実装 - 完了報告

**日付**: 2025-12-16
**プロジェクト**: セキュリティカメラ - PC側受信アプリケーション
**Phase**: Phase 2 - Rust実装
**結果**: ✅ ビルド成功・実行可能

---

## 📊 実装概要

### Phase 2 の目的
Spresense (Phase 1) から USB CDC-ACM 経由で送信される H.264 映像を受信・保存するPC側アプリケーションを実装する。

### 成果物
- **実行ファイル**: `/home/ken/Rust_ws/security_camera_viewer/target/debug/security_camera_viewer` (33MB)
- **ソースコード**: 3モジュール、約500行
- **プロジェクト**: `/home/ken/Rust_ws/security_camera_viewer/`

---

## 📁 実装ファイル一覧

### プロジェクト構造
```
/home/ken/Rust_ws/security_camera_viewer/
├── Cargo.toml               (依存関係定義)
├── Cargo.lock              (依存関係ロック)
└── src/
    ├── main.rs             (262行) メインアプリケーション
    ├── protocol.rs         (212行) プロトコル定義・パーサ
    └── serial.rs           (173行) シリアル通信
```

### Cargo.toml - 依存クレート
```toml
serialport = "4.5"          # シリアル通信
bytes = "1.5"               # バイトバッファ操作
byteorder = "1.5"           # エンディアン変換
crc = "3.0"                 # CRC計算
tokio = "1.35"              # 非同期ランタイム
log = "0.4"                 # ロギング
env_logger = "0.11"         # ログ出力
anyhow = "1.0"              # エラーハンドリング
thiserror = "1.0"           # カスタムエラー定義
clap = "4.4"                # CLI引数解析
```

---

## 🔧 解決した技術課題

### 1. libudev開発パッケージ不足
**問題**: `libudev-sys` のビルドエラー
```
error: failed to run custom build command for `libudev-sys v0.1.4`
The system library `libudev` required by crate `libudev-sys` was not found.
```

**原因**: serialport クレートが Linux で udev を使用するため、開発パッケージが必要

**解決**: システムパッケージをインストール
```bash
sudo apt-get install -y libudev-dev pkg-config
```

### 2. 曖昧な関連型エラー (Ambiguous Associated Item)
**問題**: protocol.rs でコンパイルエラー
```
error: ambiguous associated item
  --> src/protocol.rs:23:44
   |
23 |     fn try_from(value: u8) -> Result<Self, Self::Error> {
   |                                            ^^^^^^^^^^^
```

**原因**: `PacketType` 列挙型に `Error` バリアントがあり、`TryFrom` トレイトの `Error` 関連型と名前が衝突

**解決**: 完全修飾構文を使用
```rust
// 修正前
fn try_from(value: u8) -> Result<Self, Self::Error> {

// 修正後
fn try_from(value: u8) -> Result<Self, <PacketType as TryFrom<u8>>::Error> {
```

---

## 📋 実装詳細

### 1. Protocol Module (protocol.rs)

**役割**: カスタムバイナリプロトコルの定義とパース

**主要な型**:
```rust
pub const PACKET_MAGIC: u16 = 0x5350;      // 'SP'
pub const PACKET_VERSION: u8 = 0x01;

pub enum PacketType {
    Handshake = 0x01,
    VideoSPS = 0x10,
    VideoPPS = 0x11,
    VideoIDR = 0x12,
    VideoSlice = 0x13,
    Heartbeat = 0x20,
    Error = 0xFF,
}

pub struct PacketHeader {
    pub magic: u16,
    pub version: u8,
    pub packet_type: PacketType,
    pub sequence: u32,
    pub timestamp_us: u64,
    pub payload_size: u32,
    pub checksum: u16,
}

pub struct Packet {
    pub header: PacketHeader,
    pub payload: Vec<u8>,
}

pub struct HandshakePayload {
    pub video_width: u16,
    pub video_height: u16,
    pub fps: u8,
    pub codec: u8,
    pub bitrate: u32,
}
```

**主要な関数**:
- `PacketHeader::parse()`: 22バイトヘッダーをパース
- `Packet::parse()`: 完全なパケットをパース、CRC16検証
- `crc16_ibm_sdlc()`: CRC-16-IBM-SDLC 計算
- `HandshakePayload::parse()`: ハンドシェイクペイロードをパース

**実装ポイント**:
- Little Endian バイトオーダー
- CRC16検証で不正パケットを検出
- マジックナンバー、バージョンチェック
- ペイロードサイズ上限チェック (8KB)

### 2. Serial Module (serial.rs)

**役割**: USB CDC-ACM シリアルポート通信

**主要な型**:
```rust
pub struct SerialConnection {
    port: Box<dyn SerialPort>,
}
```

**主要な関数**:
- `open()`: 指定ポートをオープン (115200 bps, 1秒タイムアウト)
- `auto_detect()`: Spresense自動検出 (VID=0x054C, PID=0x0BC2)
- `list_ports()`: 利用可能なシリアルポート一覧表示
- `read_packet()`: 完全なパケットを読み込み
  1. ヘッダー22バイト読み込み
  2. ペイロードサイズ取得
  3. ペイロード読み込み
  4. パケット検証
- `flush()`: 受信バッファクリア

**実装ポイント**:
- USB VID/PID による自動デバイス検出
- タイムアウト処理 (1秒)
- エラーリトライ機構
- デバッグログ出力

### 3. Main Module (main.rs)

**役割**: メインアプリケーションロジックとCLI

**CLIオプション**:
```
-p, --port <PORT>          シリアルポートパス (例: /dev/ttyACM0)
-o, --output <OUTPUT>      出力ファイルパス [default: output.h264]
-v, --verbose              詳細デバッグログ有効化
-l, --list                 利用可能なシリアルポート一覧表示
    --max-errors <N>       最大連続エラー数 [default: 10]
-h, --help                 ヘルプ表示
-V, --version              バージョン表示
```

**処理フロー**:
```
1. CLI引数解析
2. ログ初期化 (INFO or DEBUG)
3. シリアルポート接続 (手動指定 or 自動検出)
4. 出力ファイル作成
5. 受信バッファフラッシュ
6. ハンドシェイク待機 (最大100回試行)
7. 映像設定情報表示
8. パケット受信ループ:
   - SPS/PPS: パラメータセット受信
   - IDR: キーフレーム受信 (フレームカウント++)
   - Slice: P/Bフレーム受信
   - NALユニットにスタートコード追加 (0x00000001)
   - ファイル書き込み
   - 30フレーム毎に進捗表示
9. 終了時統計表示
10. ファイル完全性チェック
```

**エラーハンドリング**:
- タイムアウト: デバッグログのみ、リトライ
- その他エラー: エラーログ、カウント
- 連続エラー10回: プログラム終了

**統計情報**:
- 総フレーム数
- 総パケット数
- 総受信データ量 (MB)
- SPS/PPS受信状態
- ファイル再生可能性判定

---

## 📊 ビルド結果

### ビルドコマンド
```bash
cd /home/ken/Rust_ws/security_camera_viewer
cargo build
```

### ビルドログ抜粋
```
   Compiling security_camera_viewer v0.1.0
warning: unused import: `warn`
 --> src/serial.rs:4:24

warning: fields `magic`, `version`, and `timestamp_us` are never read
  --> src/protocol.rs:40:9

warning: methods `read_bytes` and `set_timeout` are never used
  --> src/serial.rs:100:12

    Finished `dev` profile [unoptimized + debuginfo] target(s) in 1.95s
```

**警告について**:
- すべて未使用コードの警告 (機能には影響なし)
- `magic`, `version`, `timestamp_us`: デバッグ時に使用可能
- `read_bytes`, `set_timeout`: 将来の拡張用

### 成果物
```
/home/ken/Rust_ws/security_camera_viewer/target/debug/security_camera_viewer
サイズ: 33 MB (デバッグビルド)
パーミッション: -rwxr-xr-x (実行可能)
```

---

## 🧪 動作確認

### 1. ヘルプ表示
```bash
$ ./target/debug/security_camera_viewer --help
Usage: security_camera_viewer [OPTIONS]

Options:
  -p, --port <PORT>              Serial port path (e.g., /dev/ttyACM0)
  -o, --output <OUTPUT>          Output file path for H.264 video [default: output.h264]
  -v, --verbose                  Enable verbose debug logging
  -l, --list                     List available serial ports and exit
      --max-errors <MAX_ERRORS>  Maximum number of consecutive errors before exit [default: 10]
  -h, --help                     Print help
  -V, --version                  Print version
```

### 2. シリアルポート一覧
```bash
$ ./target/debug/security_camera_viewer --list
[2025-12-15T21:14:57Z INFO  security_camera_viewer] Security Camera Viewer v0.1.0
[2025-12-15T21:14:57Z INFO  security_camera_viewer] =========================================
[2025-12-15T21:14:57Z INFO  security_camera_viewer::serial] No serial ports found
```
(Spresense未接続のため "No serial ports found" は正常)

---

## 🎯 Phase 2 完了チェックリスト

- [x] Rustプロジェクト作成
- [x] Cargo.toml 依存関係設定
- [x] protocol.rs 実装
  - [x] パケット型定義
  - [x] パーサ実装
  - [x] CRC16実装
  - [x] ユニットテスト
- [x] serial.rs 実装
  - [x] シリアルポートオープン
  - [x] Spresense自動検出
  - [x] パケット読み込み
  - [x] エラーハンドリング
- [x] main.rs 実装
  - [x] CLI引数解析
  - [x] ハンドシェイク処理
  - [x] パケット受信ループ
  - [x] ファイル書き込み
  - [x] 統計情報表示
- [x] ビルド成功
- [x] 実行可能ファイル生成
- [x] 基本動作確認 (--help, --list)

---

## 📈 次のステップ: Phase 3 統合テスト

### 必要な作業

1. **Spresense準備**
   - ファームウェア書き込み
   - USB接続
   - デバイス認識確認

2. **PC側準備**
   - シリアルポートアクセス権限設定
   ```bash
   sudo usermod -a -G dialout $USER
   # ログアウト・ログイン
   ```

3. **統合テスト実行**
   ```bash
   # 自動検出モード
   ./target/debug/security_camera_viewer -v -o test.h264

   # 手動指定モード
   ./target/debug/security_camera_viewer -p /dev/ttyACM0 -v -o test.h264
   ```

4. **映像再生確認**
   ```bash
   # VLC
   vlc test.h264

   # ffplay
   ffplay test.h264

   # MP4変換
   ffmpeg -i test.h264 -c copy test.mp4
   ```

5. **テストシナリオ**
   - [ ] ハンドシェイク受信
   - [ ] SPS/PPS受信
   - [ ] IDRフレーム受信
   - [ ] 30秒以上の連続受信
   - [ ] ファイル保存
   - [ ] 映像再生確認
   - [ ] エラー処理動作確認

---

## 📚 使用方法

### 基本的な使い方

1. **Spresenseを接続**
   ```bash
   # デバイス確認
   ls -l /dev/ttyACM*
   # または
   lsusb | grep 054c
   ```

2. **アプリケーション実行**
   ```bash
   cd /home/ken/Rust_ws/security_camera_viewer

   # 自動検出モード (推奨)
   ./target/debug/security_camera_viewer -v

   # 手動指定モード
   ./target/debug/security_camera_viewer -p /dev/ttyACM0 -v

   # 出力ファイル指定
   ./target/debug/security_camera_viewer -o my_video.h264 -v
   ```

3. **受信開始**
   - ハンドシェイク待機
   - 映像設定情報表示
   - パケット受信開始
   - 30フレーム毎に進捗表示

4. **受信停止**
   - Ctrl+C で停止
   - 統計情報表示
   - ファイル保存完了

5. **映像再生**
   ```bash
   vlc output.h264
   # または
   ffplay output.h264
   ```

### トラブルシューティング

**シリアルポートが見つからない**:
```bash
# デバイス確認
ls -l /dev/ttyACM* /dev/ttyUSB*
lsusb | grep -i sony

# パーミッション確認
ls -l /dev/ttyACM0

# グループ追加
sudo usermod -a -G dialout $USER
# ログアウト・ログイン必要
```

**ハンドシェイクがタイムアウト**:
- Spresenseが起動しているか確認
- シリアルポートが正しいか確認
- Spresenseアプリが実行中か確認

**CRCエラー**:
- USB接続を確認
- ケーブルを交換
- ノイズの多い環境を避ける

---

## ✅ 結論

Phase 2 (PC側Rust実装) は完全に成功しました。

**主な成果**:
1. プロトコルパーサ実装 (CRC16検証付き)
2. USB CDC-ACMシリアル通信実装
3. Spresense自動検出機能
4. H.264ファイル保存機能
5. 詳細なログ・統計情報
6. エラーハンドリング

**次の段階**:
Phase 3 として Spresense を接続し、エンドツーエンドの統合テストを実行します。

---

**作成日**: 2025-12-16
**作成者**: Claude Code (Sonnet 4.5)
**プロジェクト**: Security Camera - Phase 2 完了報告
