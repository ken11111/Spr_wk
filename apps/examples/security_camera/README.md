# Security Camera Example

Spresense HDR Camera Board を使用した防犯カメラシステムのサンプルアプリケーション。

## 機能

- HD (1280x720) 映像キャプチャ @ 30fps
- H.264 ハードウェアエンコード (2 Mbps)
- USB CDC 経由でのストリーミング送信
- カスタムバイナリプロトコル (CRC16チェックサム付き)

## ビルド方法

### 1. 設定

```bash
cd /path/to/spresense/sdk
./tools/config.py examples/security_camera
```

または既存の設定に追加:

```bash
./tools/config.py default
make menuconfig
# -> Application Configuration -> Examples -> Security Camera を有効化
```

### 2. ビルド

```bash
make
```

### 3. フラッシュ

```bash
./tools/flash.sh -c /dev/ttyUSB0 nuttx.spk
```

## 実行方法

シリアルコンソールに接続:

```bash
screen /dev/ttyUSB0 115200
```

アプリケーション実行:

```
nsh> security_camera
```

## 設定オプション

Kconfig で以下の設定が可能:

- `CONFIG_EXAMPLES_SECURITY_CAMERA_CAMERA_WIDTH`: カメラ幅 (デフォルト: 1280)
- `CONFIG_EXAMPLES_SECURITY_CAMERA_CAMERA_HEIGHT`: カメラ高さ (デフォルト: 720)
- `CONFIG_EXAMPLES_SECURITY_CAMERA_FPS`: フレームレート (デフォルト: 30)
- `CONFIG_EXAMPLES_SECURITY_CAMERA_BITRATE`: ビットレート (デフォルト: 2000000)
- `CONFIG_EXAMPLES_SECURITY_CAMERA_HDR_ENABLE`: HDR有効化 (デフォルト: 無効)

## 必要な依存関係

Kconfig で自動的に有効化されます:

- `VIDEO`: ビデオサブシステム
- `VIDEO_ISX012`: ISX012 カメラドライバ
- `VIDEOUTILS_CODEC_H264`: H.264 コーデック
- `USBDEV`: USB デバイスサポート
- `CDCACM`: USB CDC-ACM サポート

## PC側受信

PC側でUSB CDC経由でデータを受信するには、別途Rustアプリケーションが必要です。
詳細は `/home/ken/Spr_ws/spresense/security_camera/README.md` を参照してください。

## ファイル構成

```
security_camera/
├── Kconfig                 - 設定定義
├── Makefile                - ビルド設定
├── README.md               - このファイル
├── config.h                - アプリケーション設定
├── camera_manager.h/c      - カメラ管理
├── encoder_manager.h/c     - エンコーダ管理
├── protocol_handler.h/c    - プロトコル処理
├── usb_transport.h/c       - USB転送
└── camera_app_main.c       - メインアプリケーション
```

## ライセンス

BSD 3-Clause License

## 参考

- [Spresense SDK Documentation](https://developer.sony.com/develop/spresense/)
- [仕様書: /home/ken/Spr_ws/spresense/security_camera/](../../../security_camera/)
