# Phase 1 成功手順ガイド

**作成日**: 2025-12-20
**Phase**: Phase 1A + Phase 1B 完全成功手順

このドキュメントは、Spresense セキュリティカメラプロジェクトの Phase 1 を再現するための完全な手順を記載しています。

---

## 目次

1. [ハードウェア要件](#ハードウェア要件)
2. [ソフトウェア要件](#ソフトウェア要件)
3. [Phase 1A: カメラ・プロトコル実装](#phase-1a-カメラプロトコル実装)
4. [Phase 1B: USB CDC データ転送](#phase-1b-usb-cdc-データ転送)
5. [トラブルシューティング](#トラブルシューティング)

---

## ハードウェア要件

### 必須

- **Spresense メインボード** (CXD5602)
- **Spresense 拡張ボード** (USB CDC 用)
- **Spresense カメラボード** (ISX012)
- **USB ケーブル × 2**:
  - 1本: コンソール/フラッシュ用 (Main Board)
  - 1本: データ転送用 (Extension Board)
- **PC**: Windows 10/11 + WSL2 (Ubuntu 20.04 推奨)

### 接続構成

```
PC
├── USB1 → Spresense Main Board (CP2102 - コンソール/フラッシュ)
└── USB2 → Spresense Extension Board (CXD5602 USB Device - データ転送)
```

---

## ソフトウェア要件

### Spresense 側

- **NuttX**: 12.3.0
- **Spresense SDK**: 最新版
- **ツールチェーン**: `arm-none-eabi-gcc` (spresenseenv)

### PC 側 (Windows + WSL2)

- **WSL2**: Ubuntu 20.04 以降
- **usbipd-win**: USB デバイス WSL2 パススルー用
- **Python 3**: フラッシュツール用

**インストール** (Windows PowerShell 管理者権限):
```powershell
winget install --interactive --exact dorssel.usbipd-win
```

**WSL2 で**:
```bash
sudo apt update
sudo apt install build-essential python3 python3-serial
```

---

## Phase 1A: カメラ・プロトコル実装

### 1. プロジェクトセットアップ

```bash
cd ~/Spr_ws/spresense/sdk
```

### 2. NuttX 設定

#### カメラ設定

`sdk/apps/examples/security_camera/config.h`:
```c
#define CONFIG_CAMERA_FORMAT         V4L2_PIX_FMT_JPEG
#define CONFIG_CAMERA_WIDTH          320
#define CONFIG_CAMERA_HEIGHT         240
#define CONFIG_CAMERA_FPS            30
```

#### V4L2 設定

`sdk/apps/examples/security_camera/camera_manager.c`:
- **Buffer Type**: `V4L2_BUF_TYPE_VIDEO_CAPTURE` (連続キャプチャ用)
- **Buffer Mode**: `V4L2_BUF_MODE_RING` (連続ストリーム用)
- **Memory Mode**: `V4L2_MEMORY_USERPTR`

### 3. MJPEG プロトコル実装

#### プロトコル構造

```
[SYNC_WORD:4] [SEQUENCE:4] [SIZE:4] [JPEG_DATA:N] [CRC16:2]
```

- **Sync Word**: `0xCAFEBABE` (リトルエンディアン: `BE BA FE CA`)
- **Sequence**: フレーム番号 (uint32_t)
- **Size**: JPEG データサイズ (uint32_t)
- **JPEG Data**: カメラからの JPEG データ
- **CRC16**: CRC-16-CCITT (polynomial 0x1021)

#### 実装ファイル

- `mjpeg_protocol.h`: プロトコル定義
- `mjpeg_protocol.c`: CRC計算、パケット作成
- `camera_manager.c`: カメラキャプチャ
- `camera_app_main.c`: メインループ

### 4. メモリ最適化

**重要**: JPEG バッファサイズは **64 KB** に設定:

```c
// mjpeg_protocol.h
#define MJPEG_MAX_JPEG_SIZE      65536    /* 64 KB */

// camera_manager.c
if (fmt.fmt.pix.sizeimage == 0)
{
    fmt.fmt.pix.sizeimage = 65536;  /* 64 * 1024 */
}
```

**メモリ使用量**:
- Camera buffers: 2 × 65536 = 131,072 bytes
- Packet buffer: 65,550 bytes
- **Total**: 196,622 bytes (~192 KB)

### 5. ビルドスクリプト

`sdk/build.sh`:
```bash
#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NUTTX_DIR="${SCRIPT_DIR}/../nuttx"

export PATH="${HOME}/spresenseenv/usr/bin:/usr/bin:/bin:${PATH}"

cd "${NUTTX_DIR}"
/usr/bin/make

if [ -f "${NUTTX_DIR}/nuttx.spk" ]; then
    cp "${NUTTX_DIR}/nuttx.spk" "${SCRIPT_DIR}/nuttx.spk"
    echo "Build successful!"
    echo "Flash with: sudo tools/flash.sh -c /dev/ttyUSB0 nuttx.spk"
fi
```

### 6. ビルド・フラッシュ

```bash
# ビルド
cd ~/Spr_ws/spresense/sdk
chmod +x build.sh
./build.sh

# フラッシュ (WSL2)
sudo modprobe cp210x
sudo tools/flash.sh -c /dev/ttyUSB0 nuttx.spk
```

### 7. Phase 1A テスト

```bash
nsh> security_camera
[CAM] Security Camera Application Starting (MJPEG)
[CAM] Frame 1: JPEG=22208 bytes, Packet=22222 bytes, Seq=0
[CAM] Frame 90: JPEG=22016 bytes, Packet=22030 bytes, Seq=89
[CAM] Main loop ended, total frames: 90
```

**成功条件**: 90/90 フレーム完了、エラーなし

---

## Phase 1B: USB CDC データ転送

### 1. NuttX USB CDC 設定

#### Kconfig 変更

`nuttx/.config` に以下を追加:

```
CONFIG_CXD56_USBDEV=y
CONFIG_SYSTEM_CDCACM=y
```

**手動編集** または **menuconfig** で設定:

```bash
cd ~/Spr_ws/spresense/nuttx
make menuconfig
```

設定パス:
- `Board Selection → CXD56xx Configuration → [*] USB`
- `Application Configuration → System NSH Add-Ons → [*] USB CDC/ACM Device Commands`

### 2. USB Transport 実装

#### usb_transport.h

新関数追加:
```c
int usb_transport_send_bytes(const uint8_t *data, size_t size);
```

#### usb_transport.c

実装内容:
- `/dev/ttyACM0` オープン
- `write()` でデータ送信
- エラーハンドリング・リトライ
- 部分書き込み対応

#### camera_app_main.c

USB 送信統合:
```c
/* USB transport 初期化 */
ret = usb_transport_init();

/* MJPEG パケット送信 */
packet_size = mjpeg_pack_frame(frame.buf, frame.size, &sequence,
                                packet_buffer, MJPEG_MAX_PACKET_SIZE);

ret = usb_transport_send_bytes(packet_buffer, packet_size);
```

### 3. ビルド・フラッシュ

```bash
cd ~/Spr_ws/spresense/sdk
./build.sh
sudo tools/flash.sh -c /dev/ttyUSB0 nuttx.spk
```

### 4. PC 側 USB CDC セットアップ

#### Windows 側: usbipd 設定

**PowerShell (管理者権限)**:

```powershell
# USB デバイス一覧
usbipd list

# 期待される出力:
# BUSID  VID:PID    DEVICE
# 1-1    054c:0bc2  USB シリアル デバイス (COM4)      ← CDC ACM
# 1-11   10c4:ea60  CP210x USB to UART Bridge (COM3)  ← コンソール

# WSL2 にアタッチ
usbipd attach --wsl --busid 1-1   # CDC ACM
usbipd attach --wsl --busid 1-11  # コンソール (オプション)
```

#### WSL2 側: ドライバー・デバイス設定

```bash
# USB CDC ドライバーロード
sudo modprobe cdc-acm

# デバイス確認
ls -l /dev/ttyACM0
# 期待: crw-rw---- 1 root dialout 166, 0 ...

# 権限設定
sudo chmod 666 /dev/ttyACM0

# 🔴 最重要: Raw モード設定
stty -F /dev/ttyACM0 raw -echo 115200
```

**Raw モード設定の重要性**:
- デフォルトは canonical (cooked) mode
- バイナリデータが破損する
- **必ず raw モードに設定すること!**

### 5. データ受信テスト

#### ターミナル 1: PC 側受信

```bash
# 古いファイル削除
rm -f /tmp/mjpeg_stream.bin

# データ受信開始
cat /dev/ttyACM0 > /tmp/mjpeg_stream.bin
```

(このコマンドはブロックします - 正常です)

#### ターミナル 2: Spresense 実行

```bash
nsh> sercon
sercon: Successfully registered the CDC/ACM serial driver

nsh> security_camera
[CAM] Security Camera Application Starting (MJPEG)
[CAM] USB transport initialized (/dev/ttyACM0)
[CAM] Frame 1: JPEG=8832 bytes, Packet=8846 bytes, USB sent=8846, Seq=0
[CAM] Frame 90: JPEG=7104 bytes, Packet=7118 bytes, USB sent=7118, Seq=89
[CAM] USB transport cleaned up (total sent: 672972 bytes)
```

#### ターミナル 1: 受信確認

Spresense が完了したら **Ctrl+C** で `cat` を停止。

```bash
# ファイルサイズ確認
ls -lh /tmp/mjpeg_stream.bin
# 期待: ~657K (672,972 bytes)

# プロトコル確認
hexdump -C /tmp/mjpeg_stream.bin | head -30

# 期待される出力:
# 00000000  be ba fe ca 00 00 00 00  20 21 00 00 ff d8 ff db  |........ !......|
#           ^^^^^^^^^^^ ^^^^^^^^^^^  ^^^^^^^^^^  ^^^^^^^^^^^
#           SYNC_WORD   SEQUENCE=0   SIZE=8480   JPEG SOI

# 同期ワード数確認
hexdump -C /tmp/mjpeg_stream.bin | grep -c "be ba fe ca"
# 期待: 90 (90フレーム分)
```

### 6. 成功確認チェックリスト

- [ ] ファイルサイズ: ~657K (672,972 bytes)
- [ ] 同期ワード (`be ba fe ca`) が 90 個見つかる
- [ ] JPEG SOI マーカー (`ff d8`) が 90 個見つかる
- [ ] 最初のパケットに正しいシーケンス番号 (0x00000000)
- [ ] データに破損がない

---

## トラブルシューティング

### 問題 1: `/dev/ttyACM0` が見つからない

**原因**: CDC ACM ドライバー未ロードまたは USB 未接続

**解決策**:
```bash
# ドライバーロード
sudo modprobe cdc-acm

# USB デバイス確認
lsusb | grep Sony
# 期待: Bus 001 Device XXX: ID 054c:0bc2 Sony Corp. CDC/ACM Serial

# usbipd で再アタッチ (Windows PowerShell)
usbipd detach --busid 1-1
usbipd attach --wsl --busid 1-1
```

### 問題 2: 同期ワードが見つからない (データ破損)

**原因**: TTY が canonical mode (最も一般的な問題!)

**解決策**:
```bash
# Raw モード設定 (必須!)
stty -F /dev/ttyACM0 raw -echo 115200

# 設定確認
stty -F /dev/ttyACM0 -a | grep -E "raw|echo"
# 期待: -isig -icanon ... -echo ...
```

### 問題 3: カメラ初期化失敗 (Error -17)

**原因**: カメラデバイスが busy (前回の実行が残っている)

**解決策**:
```bash
nsh> reboot
```

### 問題 4: メモリ割り当て失敗

**原因**: バッファサイズが大きすぎる

**解決策**: `MJPEG_MAX_JPEG_SIZE` を 65536 (64KB) に設定

### 問題 5: `/dev/ttyUSB0` デバイスロック

**原因**: 別プロセスがデバイスを使用中

**解決策**:
```bash
# ロックファイル削除
sudo rm -f /var/lock/LCK..ttyUSB0 /var/run/lock/LCK..ttyUSB0

# または使用中プロセス確認
sudo lsof /dev/ttyUSB0
```

---

## パフォーマンス

### Phase 1B 最終結果

- **フレーム数**: 90/90 (100%)
- **総送信データ**: 672,972 bytes (~657 KB)
- **平均フレームサイズ**: 7,477 bytes
- **平均パケットサイズ**: 7,491 bytes
- **プロトコルオーバーヘッド**: 14 bytes/frame (0.19%)
- **転送速度**: ~224 KB/s (30fps × 7.5KB)
- **USB 使用率**: 15% (Full Speed 12Mbps)

---

## 次のステップ: Phase 2

Phase 1 完了後、次は PC 側 Rust アプリケーションを実装:

1. **USB CDC 接続** (`serialport` crate)
2. **MJPEG プロトコルパーサー**
3. **CRC-16 検証**
4. **JPEG デコード** (`image` crate)
5. **リアルタイム表示** (`egui` / `sdl2`)

---

## まとめ

### Phase 1A: カメラ・プロトコル

✅ JPEG キャプチャ (ISX012 内蔵エンコーダー)
✅ MJPEG プロトコル設計・実装
✅ メモリ最適化 (1.5MB → 192KB)

### Phase 1B: USB CDC 転送

✅ USB CDC ACM 設定 (NuttX Kconfig)
✅ USB transport 実装
✅ データ送受信成功 (672,972 bytes)
✅ **TTY raw モード設定** ← 最重要!

---

**作成者**: Claude Code (Sonnet 4.5)
**最終更新**: 2025-12-20 19:45
