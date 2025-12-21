# 防犯カメラシステム実装プロンプト案

## 📋 このドキュメントについて

Claude Codeで実装を依頼する際に使用する効率的なプロンプト案です。
以下のプロンプトをClaude Codeに送信することで、仕様書に基づいた実装を段階的に進めることができます。

---

## 🎯 Phase 1: Spresense側実装（最小機能）

### プロンプト 1-1: プロジェクト構造作成とKconfig設定

```
/home/ken/Spr_ws/spresense/security_camera/ ディレクトリ以下に、
FUNCTIONAL_SPEC.md、SOFTWARE_SPEC_SPRESENSE.md、PROTOCOL_SPEC.md の仕様書があります。

これらの仕様書に基づいて、Spresense側の防犯カメラアプリケーションを実装してください。

# Phase 1-1: プロジェクト構造とKconfig設定

以下の手順で実装してください:

1. **プロジェクト構造作成**
   - /home/ken/Spr_ws/spresense/examples/security_camera/ 配下に必要なファイルを作成
   - Makefile, Kconfig, Make.defs, README.md を作成
   - SOFTWARE_SPEC_SPRESENSE.md のディレクトリ構成に従う

2. **Kconfig設定 - 重要な注意事項**
   NuttXは「二重コンフィグ構造」を採用しています:

   a. Kconfigファイル作成:
      - examples/security_camera/Kconfig を作成
      - 必要な依存関係を明記:
        * VIDEO=y (カメラドライバ)
        * VIDEO_ISX012=y (カメラセンサー)
        * USBDEV=y (USBデバイス)
        * CDCACM=y (USB CDC-ACM)

   b. defconfig設定:
      - sdk/configs/default/defconfig に設定を追加する必要があります
      - または新しいdefconfigを作成: sdk/configs/security_camera/defconfig

   c. ビルド前の設定反映:
      ```bash
      cd /home/ken/Spr_ws/spresense/sdk
      ./tools/config.py default  # または security_camera
      make
      ```

3. **config.h作成**
   - SOFTWARE_SPEC_SPRESENSE.md の「9.1 config.h」を参照
   - カメラ解像度: 1280x720
   - FPS: 30
   - ビットレート: 2000000 (2Mbps)

4. **共通ヘッダファイル作成**
   - camera_manager.h
   - encoder_manager.h
   - protocol_handler.h
   - usb_transport.h

注意事項:
- NuttXのコンフィグ変更後は必ず `./tools/config.py` で反映してください
- ビルドエラーが出た場合、.config ファイルで実際の設定値を確認してください
- USB CDC-ACMは /dev/ttyACM0 として見えるようにCONFIG_CDCACM=y が必須です
```

### プロンプト 1-2: カメラマネージャ実装

```
次に、camera_manager.c/h を実装してください。

# Phase 1-2: Camera Manager実装

SOFTWARE_SPEC_SPRESENSE.md の「7.1 Camera Manager API」に従って実装してください。

実装する関数:
1. camera_manager_init(const camera_config_t *config)
   - /dev/video0 をオープン
   - ioctl(VIDIOC_S_FMT) で解像度設定 (1280x720, YUV422)
   - ioctl(VIDIOC_S_PARM) でFPS設定 (30fps)
   - ioctl(VIDIOC_REQBUFS) でバッファ確保

2. camera_get_frame(camera_frame_t *frame)
   - poll() でフレーム待機
   - ioctl(VIDIOC_DQBUF) でバッファ取得
   - フレームデータコピー
   - タイムスタンプ設定 (clock_gettime使用)
   - ioctl(VIDIOC_QBUF) でバッファ返却

参考:
- NuttXのカメラドライバ例: spresense/examples/camera/
- V4L2ヘッダ: nuttx/include/nuttx/video/video.h

注意:
- バッファは2個使用（ダブルバッファリング）
- エラーハンドリングを必ず実装
- ログ出力は syslog() を使用
```

### プロンプト 1-3: エンコーダマネージャ実装

```
次に、encoder_manager.c/h を実装してください。

# Phase 1-3: Encoder Manager実装

SOFTWARE_SPEC_SPRESENSE.md の「7.2 Encoder Manager API」に従って実装してください。

実装する関数:
1. encoder_manager_init(const encoder_config_t *config)
   - /dev/video1 をオープン（ビデオエンコーダデバイス）
   - H.264エンコーダパラメータ設定
   - ビットレート: 2 Mbps
   - GOP: 30フレーム
   - プロファイル: Baseline

2. encoder_encode_frame(const camera_frame_t *yuv_frame, h264_nal_unit_t *nal_units, int max_nal_count)
   - YUVデータをエンコーダに書き込み
   - エンコード完了待機
   - NAL Unitを読み出し（SPS, PPS, IDR, or SLICE）
   - NAL Unitタイプ判定

参考:
- Spresense Video Encoder SDK: sdk/modules/video/
- ビデオエンコーダ例: examples/video_player/

重要な注意:
- 最初のフレームではSPS, PPS, IDRの3つのNAL Unitが返される
- 通常のフレームはSLICE（P-frame）1つのみ
- NAL Unitタイプは先頭バイトの下位5ビットで判定
```

### プロンプト 1-4: プロトコルハンドラとUSB転送実装

```
次に、protocol_handler.c/h と usb_transport.c/h を実装してください。

# Phase 1-4: Protocol Handler & USB Transport実装

PROTOCOL_SPEC.md の仕様に従って実装してください。

## protocol_handler.c/h

実装する関数:
1. protocol_pack_nal_unit(const h264_nal_unit_t *nal, packet_t *packets, int max_packets)
   - NAL Unitをパケット化
   - ヘッダ作成（magic=0x5350, version=0x01, type, sequence, timestamp）
   - CRC16計算（ペイロードに対して）
   - NAL Unitが4KB超の場合はフラグメント化

2. protocol_send_handshake(void)
   - HANDSHAKEパケット作成
   - ペイロード: width(1280), height(720), fps(30), codec(0x01), bitrate(2000000)

CRC16実装:
- CRC-16-IBM-SDLCアルゴリズム使用
- 初期値: 0xFFFF
- 多項式: 0x1021

## usb_transport.c/h

実装する関数:
1. usb_transport_init(void)
   - /dev/ttyACM0 をオープン
   - 送信バッファ初期化（4個、8KB each）

2. usb_transport_send(const packet_t *packet)
   - パケット送信
   - write() でUSB経由送信
   - 送信バイト数をログ出力

注意:
- USB CDC-ACMは /dev/ttyACM0 として見える
- PC側が接続していない場合、write()は-EAGAINを返す可能性がある
- エラーハンドリングを実装（再試行3回）
```

### プロンプト 1-5: メインアプリケーション実装

```
最後に、camera_app_main.c を実装してください。

# Phase 1-5: Main Application実装

SOFTWARE_SPEC_SPRESENSE.md の「5.1 初期化シーケンス」と「5.2 メインループシーケンス」に従って実装してください。

実装内容:

1. main()関数
   - カメラマネージャ初期化
   - エンコーダマネージャ初期化
   - USB転送初期化
   - USB接続待機
   - HANDSHAKEパケット送信
   - メインループ起動

2. メインループ（30fps = 33msごと）
   ```c
   while (1) {
       // フレーム取得
       camera_get_frame(&frame);

       // H.264エンコード
       encoder_encode_frame(&frame, nal_units, &nal_count);

       // 各NAL Unitをパケット化して送信
       for (int i = 0; i < nal_count; i++) {
           protocol_pack_nal_unit(&nal_units[i], packets, &pkt_count);
           for (int j = 0; j < pkt_count; j++) {
               usb_transport_send(&packets[j]);
           }
       }

       // 33ms待機（30fps維持）
       usleep(33000);
   }
   ```

3. エラーハンドリング
   - USB切断検出 → 再接続試行（最大3回）
   - カメラエラー → ログ出力、継続試行
   - エンコーダエラー → ログ出力、フレームスキップ

4. ログ出力
   - 起動時: "Security Camera App started"
   - 接続時: "USB connected, streaming started"
   - エラー時: "Error: ..." with error code

参考:
- エントリポイント: camera_app_main(int argc, char *argv[])
- NuttXアプリケーション登録: Makefile の MAINSRC
```

---

## 🎯 Phase 2: PC側実装（Rust）

### プロンプト 2-1: Cargoプロジェクト作成

```
/home/ken/Spr_ws/spresense/security_camera/pc/ ディレクトリ配下に、
PC側のRustプロジェクトを作成してください。

# Phase 2-1: Rust Project Setup

SOFTWARE_SPEC_PC_RUST.md の仕様に従って実装してください。

1. **Cargoプロジェクト作成**
   ```bash
   cd /home/ken/Spr_ws/spresense/security_camera/
   cargo new pc --name security_camera
   cd pc
   ```

2. **Cargo.toml設定**
   SOFTWARE_SPEC_PC_RUST.md の「2.2 Cargo.toml」をコピー
   主要なクレート:
   - tokio: 非同期ランタイム
   - tokio-serial: USB CDC通信
   - ffmpeg-next: H.264デコード
   - egui, eframe: GUI
   - rusqlite: データベース
   - nom: パケットパース

3. **ディレクトリ構造作成**
   SOFTWARE_SPEC_PC_RUST.md の「2.1 ディレクトリ構造」に従う

4. **config.toml作成**
   FUNCTIONAL_SPEC.md の「10.1 設定ファイル例」を参照

注意:
- ffmpeg-nextはシステムにFFmpegライブラリが必要
  Ubuntu: sudo apt install libavcodec-dev libavformat-dev libavutil-dev
```

### プロンプト 2-2: 共通型とエラー型実装

```
次に、types.rs と error.rs を実装してください。

# Phase 2-2: Common Types & Error Types

SOFTWARE_SPEC_PC_RUST.md の「3. データ構造」に従って実装してください。

## src/types.rs
以下の型を定義:
- VideoFrame: RGB24映像フレーム
- NalUnit: H.264 NAL Unit
- Packet: プロトコルパケット
- PacketType: パケットタイプ（enum）
- HandshakeInfo: ハンドシェイク情報
- RecordingFile: 録画ファイル情報
- SystemState: システム状態（enum）

## src/error.rs
SOFTWARE_SPEC_PC_RUST.md の「3.2 エラー型定義」を実装
- CameraError: thiserrorを使用
- Result<T>: type alias

注意:
- derive(Debug, Clone)を適切に付与
- Bytesクレートを使用（ゼロコピー）
```

### プロンプト 2-3: USB Transport & Protocol実装

```
次に、receiver/usb_transport.rs と receiver/protocol.rs を実装してください。

# Phase 2-3: USB Transport & Protocol Handler

SOFTWARE_SPEC_PC_RUST.md の「4.1 USB Transport」と「4.2 Protocol Handler」に従って実装してください。

## receiver/usb_transport.rs

実装内容:
1. UsbTransport構造体
   - シリアルポート接続
   - パケット受信ループ（非同期）
   - バッファ管理

2. connect() メソッド
   - tokio_serial::new() でポート接続
   - ボーレート: 115200 (実際は無視されるがデフォルト値)

3. receive_loop() メソッド
   - AsyncReadExt::read() で受信
   - バッファに蓄積
   - try_parse_packet() でパケット抽出
   - mpsc::Sender でパケット送信

## receiver/protocol.rs

実装内容:
1. parse_packet() 関数
   - nomクレートを使用
   - PROTOCOL_SPEC.md の「2. パケット構造」に従う
   - CRC16検証

2. calculate_crc16() 関数
   - crcクレートのCRC_16_IBM_SDLCを使用

3. parse_handshake() 関数
   - HANDSHAKEペイロードをパース
   - HandshakeInfo構造体に変換

参考:
- nom: コンビネータパーサライブラリ
- le_u16, le_u32, le_u64: Little Endianパース
```

### プロンプト 2-4: H.264デコーダ実装

```
次に、receiver/decoder.rs を実装してください。

# Phase 2-4: H.264 Decoder

SOFTWARE_SPEC_PC_RUST.md の「4.3 H.264 Decoder」に従って実装してください。

実装内容:

1. H264Decoder構造体
   - ffmpeg-next を使用
   - Arc<Mutex<>> でスレッド安全に

2. new() メソッド
   - ffmpeg::init()
   - H.264デコーダ検索
   - デコーダ初期化

3. decode() メソッド（非同期）
   - NAL Unitをデコード
   - YUV → RGB変換（SwScale使用）
   - VideoFrame生成

YUV → RGB変換:
- ffmpeg::software::scaling::Context使用
- 入力: YUV420P（デコード結果）
- 出力: RGB24
- スケーリングフラグ: BILINEAR

注意:
- ffmpeg::Error::Eof と EAGAIN(-11) は正常（フレームがまだ出力されていない）
- SPS/PPSパケットではフレームは出力されない
```

### プロンプト 2-5: MP4ライター実装

```
次に、recorder/mp4_writer.rs を実装してください。

# Phase 2-5: MP4 Writer

SOFTWARE_SPEC_PC_RUST.md の「4.4 MP4 Writer」に従って実装してください。

実装内容:

1. Mp4Writer構造体
   - 出力ディレクトリ管理
   - 現在のファイル管理
   - 1時間ごとの分割

2. start_recording() メソッド
   - タイムスタンプ付きファイル名生成
   - "video_YYYYMMDD_HHMMSS.mp4"
   - ファイルオープン

3. write_nal_unit() メソッド
   - H.264 NAL Unitを書き込み
   - MP4コンテナ形式（mp4クレート使用）
   - 1時間経過チェック → ファイル分割

4. stop_recording() メソッド
   - ファイルクローズ
   - RecordingFile情報返却

注意:
- mp4クレートの使用方法を確認
- H.264をMP4にmuxする際、SPS/PPSはファイルヘッダに記録
```

### プロンプト 2-6: ストレージマネージャ実装

```
次に、storage/storage_manager.rs を実装してください。

# Phase 2-6: Storage Manager

SOFTWARE_SPEC_PC_RUST.md の「4.5 Storage Manager」に従って実装してください。

実装内容:

1. StorageManager構造体
   - SQLiteデータベース管理
   - 録画ファイルメタデータ管理

2. new() メソッド
   - SQLiteデータベースオープン
   - recordingsテーブル作成

3. register_file() メソッド
   - 録画ファイル情報をDBに登録
   - UNIX timestampで保存

4. cleanup_old_files() メソッド
   - 7日以上前のファイルを削除
   - DBからも削除
   - 削除数を返却

5. list_all_recordings() メソッド
   - 全録画ファイルリスト取得
   - 新しい順にソート

SQLiteスキーマ:
```sql
CREATE TABLE recordings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    filename TEXT NOT NULL,
    start_time INTEGER NOT NULL,
    end_time INTEGER,
    file_size INTEGER NOT NULL,
    frame_count INTEGER NOT NULL
);
```
```

### プロンプト 2-7: GUI実装

```
次に、gui/app.rs を実装してください。

# Phase 2-7: GUI Application

SOFTWARE_SPEC_PC_RUST.md の「6. GUI設計」に従って実装してください。

実装内容:

1. CameraApp構造体
   - eframe::Appトレイト実装
   - 現在のフレーム保持
   - FPSカウンター
   - 録画情報

2. update() メソッド
   - eframeの描画ループ
   - egui::CentralPanel使用
   - ステータス表示
   - 映像表示（ColorImage）
   - 録画情報表示

3. FpsCounter実装
   - 直近1秒間のフレーム数カウント

レイアウト:
- ヘッダ: タイトル
- ステータスバー: 状態、FPS
- メイン: 映像表示（1280x720）
- フッター: 録画情報

注意:
- ctx.request_repaint() で定期的に再描画
- TextureHandle でテクスチャ管理
```

### プロンプト 2-8: メインアプリケーション実装

```
最後に、main.rs を実装してください。

# Phase 2-8: Main Application

SOFTWARE_SPEC_PC_RUST.md の「5. メインアプリケーション」に従って実装してください。

実装内容:

1. main()関数（非同期）
   - トレーシング初期化
   - 設定ファイル読み込み
   - チャネル作成（mpsc）

2. タスク起動
   - USB Transport タスク（非同期）
   - Receiver + Decoder タスク（非同期）
   - Recorder タスク（非同期）
   - GUI（メインスレッド）

3. タスク間通信
   - USB → Receiver: Packet
   - Receiver → Recorder: VideoFrame
   - Receiver → GUI: VideoFrame（共有）

4. エラーハンドリング
   - USB切断 → 自動再接続
   - デコードエラー → ログ出力、継続

全体フロー:
```
USB Transport → Packet → Receiver → NAL Unit → Decoder → VideoFrame → [Recorder, GUI]
```

実行:
```bash
cargo run --release
```
```

---

## 🎯 Phase 3: 統合テスト

### プロンプト 3-1: Spresenseビルドとフラッシュ

```
Spresense側のコードをビルドしてフラッシュしてください。

# Phase 3-1: Spresense Build & Flash

1. **ビルド準備**
   ```bash
   cd /home/ken/Spr_ws/spresense/sdk

   # コンフィグ適用
   ./tools/config.py default  # または security_camera

   # Kconfigメニュー確認（オプション）
   make menuconfig
   # -> Application Configuration -> Examples -> Security Camera
   ```

2. **ビルド**
   ```bash
   make
   ```

3. **フラッシュ**
   ```bash
   ./tools/flash.sh -c /dev/ttyUSB0 nuttx.spk
   ```

4. **動作確認**
   ```bash
   # シリアルコンソール接続
   screen /dev/ttyUSB0 115200

   # アプリ起動
   nsh> security_camera
   ```

ビルドエラーが出た場合:
- .config ファイルで設定を確認
- VIDEO, USBDEV, CDCACM が有効か確認
- make clean してから再ビルド
```

### プロンプト 3-2: PC側ビルドと実行

```
PC側のRustコードをビルドして実行してください。

# Phase 3-2: PC Build & Run

1. **依存ライブラリインストール（Ubuntu）**
   ```bash
   sudo apt update
   sudo apt install libavcodec-dev libavformat-dev libavutil-dev \
                    libswscale-dev pkg-config
   ```

2. **ビルド**
   ```bash
   cd /home/ken/Spr_ws/spresense/security_camera/pc
   cargo build --release
   ```

3. **設定ファイル確認**
   config.toml を確認:
   - connection.port = "/dev/ttyACM0"  # SpresenseのUSB CDC
   - recorder.output_dir = "./videos"

4. **実行**
   ```bash
   ./target/release/security_camera
   ```

5. **動作確認**
   - GUIウィンドウが開く
   - "Connecting..." → "Connected" → "Streaming"
   - 映像が表示される
   - FPSが30付近で安定
   - ./videos/ に video_YYYYMMDD_HHMMSS.mp4 が作成される

トラブルシューティング:
- USB接続エラー → `ls /dev/ttyACM*` で確認
- 映像が表示されない → Spresense側のログ確認
- FPSが低い → CPU使用率確認、デコード処理の最適化
```

---

## ⚠️ 重要な注意事項

### NuttX固有の注意

1. **二重コンフィグ構造**
   - Kconfigで依存関係定義
   - defconfigで実際の値設定
   - `./tools/config.py` で反映が必須

2. **ビルドの依存関係**
   - コンフィグ変更 → config.py → make clean → make
   - ヘッダ変更 → make clean推奨

3. **デバイスファイル**
   - カメラ: /dev/video0
   - エンコーダ: /dev/video1
   - USB CDC: /dev/ttyACM0

### Rust固有の注意

1. **FFmpegバインディング**
   - システムライブラリが必要
   - pkg-config で検出

2. **非同期処理**
   - tokio使用
   - async/await
   - mpscチャネルでタスク間通信

3. **所有権**
   - Bytes使用でゼロコピー
   - Arc<Mutex<>>でスレッド間共有

---

## 📝 実装チェックリスト

### Spresense側
- [ ] Kconfig設定完了
- [ ] defconfig設定完了
- [ ] camera_manager.c/h 実装
- [ ] encoder_manager.c/h 実装
- [ ] protocol_handler.c/h 実装
- [ ] usb_transport.c/h 実装
- [ ] camera_app_main.c 実装
- [ ] ビルド成功
- [ ] フラッシュ成功
- [ ] 起動確認

### PC側
- [ ] Cargo.toml設定
- [ ] types.rs, error.rs 実装
- [ ] usb_transport.rs 実装
- [ ] protocol.rs 実装
- [ ] decoder.rs 実装
- [ ] mp4_writer.rs 実装
- [ ] storage_manager.rs 実装
- [ ] gui/app.rs 実装
- [ ] main.rs 実装
- [ ] ビルド成功
- [ ] 実行成功
- [ ] 映像受信確認

### 統合テスト
- [ ] USB接続確立
- [ ] HANDSHAKEパケット受信
- [ ] 映像ストリーミング開始
- [ ] リアルタイム表示動作
- [ ] MP4ファイル作成
- [ ] ファイル分割動作（1時間後）
- [ ] 古いファイル削除動作（7日後）

---

## 🚀 次のステップ（Phase 2機能）

Phase 1完成後、以下の機能を追加:

1. **動き検出**
   - PC側で画像差分検出
   - 閾値超えで録画開始

2. **複数カメラ対応**
   - カメラIDをプロトコルに追加
   - GUI分割表示

3. **Web UI**
   - axum + WebSocketでブラウザ表示

4. **通知機能**
   - 動き検出時にメール送信
   - デスクトップ通知

---

**ドキュメントバージョン**: 1.0
**最終更新**: 2025-12-15
