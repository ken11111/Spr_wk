# Security Camera Example

Spresense HDR Camera Board を使用した防犯カメラシステムのサンプルアプリケーション。

## 機能

- HD (1280x720) 映像キャプチャ @ 30fps
- H.264 ハードウェアエンコード (2 Mbps)
- USB CDC 経由でのストリーミング送信
- カスタムバイナリプロトコル (CRC16チェックサム付き)

## ビルド方法

### 0. WiFi 設定 (初回のみ)

WiFi 認証情報は `wifi_config.h` に書き込みますが、**このファイルは `.gitignore` 済**です (X-7: 認証情報のリポジトリ漏洩防止)。

初回セットアップ:

```bash
cd apps/examples/security_camera
cp wifi_config.h.example wifi_config.h
# wifi_config.h を編集して WIFI_SSID と WIFI_PASSWORD を実際の値に変更
```

⚠ **絶対に `wifi_config.h` をコミットしないこと**。`.gitignore` で track 外しており、`git add` 時に自動除外される。詳細は [`docs/security_camera/02_specifications/quality/THREAT_MODEL.md`](../../../docs/security_camera/02_specifications/quality/THREAT_MODEL.md) TI-2 (DREAD 45) 参照。

### 1. 設定

#### 前提パッケージ (Linux/WSL)

```bash
# kconfig フロントエンド (Spresense SDK ビルドで必須)
sudo apt-get install -y kconfig-frontends

# ARM クロスコンパイラ
sudo apt-get install -y gcc-arm-none-eabi
```

#### 推奨手順 (defconfig 経由, X-9 で整備, 2026-05-09):

```bash
cd /path/to/spresense/sdk
./tools/config.py examples/security_camera
```

`spresense/sdk/configs/examples/security_camera/defconfig` を読み込み、必要設定 (CDCACM / VIDEO_ISX012 / WIFI_GS2200M / NET_TCP_NO_STACK 等) を反映する。

#### 代替手順 (menuconfig での手動有効化):

```bash
./tools/config.py default
make menuconfig
# -> Application Configuration -> Examples -> Security Camera を有効化
```

⚠ defconfig 経由を強く推奨。menuconfig 経由は設定漏れが起きやすい。

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
