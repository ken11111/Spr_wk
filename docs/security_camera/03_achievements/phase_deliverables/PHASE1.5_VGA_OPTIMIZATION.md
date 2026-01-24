# セキュリティカメラプロジェクト - 現在の状態

**最終更新**: 2025-12-20 19:30
**Phase**: Phase 1 完全達成 🎉 → Phase 2 準備中 ⏳

---

## Phase 1: 完全達成 🎉🎉🎉

**Phase 1A + Phase 1B 両方完了!**

---

## Phase 1A: カメラ・プロトコル実装 ✅

### 実装完了項目

✅ **カメラ JPEG キャプチャ**
✅ **MJPEG プロトコル設計**
✅ **プロトコル実装・テスト**
✅ **メモリ最適化**
✅ **ドキュメント作成**

### 最終実行結果

```
[CAM] Security Camera Application Starting (MJPEG)
[CAM] Camera config: 320x240 @ 30 fps, Format=JPEG, HDR=0
[CAM] Calculated sizeimage: 65536 bytes
[CAM] Allocating 2 buffers of 65536 bytes each
[CAM] Camera streaming started
[CAM] Packet buffer allocated: 65550 bytes
[CAM] Frame 1: JPEG=22208 bytes, Packet=22222 bytes, Seq=0
[CAM] Frame 30: JPEG=21920 bytes, Packet=21934 bytes, Seq=29
[CAM] Frame 60: JPEG=22016 bytes, Packet=22030 bytes, Seq=59
[CAM] Frame 90: JPEG=22016 bytes, Packet=22030 bytes, Seq=89
[CAM] Main loop ended, total frames: 90
```

**結果**: 90/90 フレーム成功 (100%)

---

## プロジェクト構成

### アーキテクチャ

```
┌─────────────────────┐      USB CDC       ┌──────────────┐
│    Spresense        │  ←────────────→    │  PC (Rust)   │
│  ISX012 Camera      │   MJPEG Stream     │   Viewer     │
│  JPEG Encoding      │                    │   Display    │
│  Protocol Packing   │                    │   Decode     │
└─────────────────────┘                    └──────────────┘
```

### MJPEG プロトコル

```
[SYNC_WORD:4] [SEQUENCE:4] [SIZE:4] [JPEG_DATA:N] [CRC16:2]
     0xCAFEBABE    Frame #     Bytes     JPEG        CRC-16-CCITT
```

**オーバーヘッド**: 14 bytes (0.064%)

---

## 技術仕様

### ハードウェア

- **MCU**: CXD5602 (ARM Cortex-M4F × 6)
- **カメラ**: ISX012 (5MP, JPEG encoder内蔵)
- **RAM**: 1.5 MB (使用中: 192 KB)
- **接続**: USB 2.0 Full Speed (12 Mbps)

### ソフトウェア

- **OS**: NuttX 12.3.0
- **Video API**: V4L2 (Video for Linux 2)
- **フォーマット**: JPEG (V4L2_PIX_FMT_JPEG / 0x4745504a)
- **解像度**: 320×240 (QVGA)
- **フレームレート**: 30 fps

### パフォーマンス

| 項目 | 値 |
|------|-----|
| JPEG サイズ (平均) | 22,016 bytes (~21.5 KB) |
| パケットサイズ (平均) | 22,030 bytes |
| 帯域幅 @ 30fps | 5.16 Mbps |
| USB 使用率 | 43% (余裕: 57%) |
| メモリ使用 | 192 KB |
| フレーム成功率 | 100% (90/90) |

---

## 解決済み問題 ✅

### 1. Hard Fault (CFSR: 00040000)
**原因**: V4L2_MEMORY_USERPTR 未設定
**解決**: memalign(32, bufsize) + buf.m.userptr 設定

### 2. Format フィールドオーバーフロー
**原因**: uint8_t で 32-bit fourcc 格納
**解決**: uint32_t に変更

### 3. ioctl Hard Fault (CFSR: 00020000)
**原因**: (unsigned long) キャスト
**解決**: (uintptr_t) に変更

### 4. バッファサイズ 0 bytes
**原因**: ドライバが sizeimage 未設定
**解決**: フォーマット別に手動計算

### 5. H.264 エンコーダー不存在
**原因**: Spresense は H.264 非対応
**解決**: カメラ内蔵 JPEG エンコーダー使用

### 6. STILL_CAPTURE タイムアウト
**原因**: 連続キャプチャ非対応
**解決**: VIDEO_CAPTURE + RING モード

### 7. メモリ割り当て失敗
**原因**: 1.5 MB 要求がヒープ超過
**解決**: 64 KB バッファに削減 (-87%)

---

## Phase 1B: USB CDC データ転送 ✅

### 完了項目 ✅

1. **USB CDC ハードウェア設定** ✅
   - `CONFIG_CXD56_USBDEV=y` - CXD5602 USB ハードウェア有効化
   - `CONFIG_SYSTEM_CDCACM=y` - CDC ACM システムユーティリティ
   - `/dev/ttyACM0` デバイス作成成功

2. **USB CDC ソフトウェア実装** ✅
   - `usb_transport_send_bytes()` - raw bytes 送信関数
   - エラーハンドリング・リトライ機能
   - 部分書き込み対応

3. **MJPEG ストリーム送信** ✅
   - 90/90 フレーム送信成功
   - 合計送信: 672,972 bytes
   - エラー数: 0 件

4. **PC 側データ受信** ✅
   - WSL2 + usbipd による USB パススルー
   - **重要**: TTY raw モード設定必須
   - MJPEG プロトコル完全受信確認

### 最終実行結果

**Spresense 側**:
```
[CAM] Security Camera Application Starting (MJPEG)
[CAM] USB transport initialized (/dev/ttyACM0)
[CAM] Frame 1: JPEG=8832 bytes, Packet=8846 bytes, USB sent=8846, Seq=0
[CAM] Frame 30: JPEG=7552 bytes, Packet=7566 bytes, USB sent=7566, Seq=29
[CAM] Frame 60: JPEG=7232 bytes, Packet=7246 bytes, USB sent=7246, Seq=59
[CAM] Frame 90: JPEG=7104 bytes, Packet=7118 bytes, USB sent=7118, Seq=89
[CAM] USB transport cleaned up (total sent: 672972 bytes)
```

**PC 側受信データ** (hexdump):
```
00000000  be ba fe ca 00 00 00 00  20 21 00 00 ff d8 ff db  |........ !......|
          ^^^^^^^^^^^ ^^^^^^^^^^^  ^^^^^^^^^^  ^^^^^^^^^^^
          SYNC_WORD   SEQUENCE=0   SIZE=8480   JPEG SOI
```

### USB CDC 接続情報

- **Spresense デバイス**: `/dev/ttyACM0`
- **PC デバイス (WSL2)**: `/dev/ttyACM0`
- **Windows デバイス**: `COM4`
- **VID:PID**: `054c:0bc2` (Sony CDC ACM)
- **転送速度**: Full Speed (12 Mbps)
- **ボーレート**: 115200 bps
- **WSL2 要件**: `usbipd` + `modprobe cdc-acm` + **raw mode**

### 重要な発見: TTY Raw モード

**問題**: デフォルトで `/dev/ttyACM0` が canonical (cooked) mode

**影響**:
- バイナリデータが破損
- 制御文字 (0x0A, 0x0D) が変換される
- MJPEG プロトコルヘッダーが消失

**解決策**:
```bash
stty -F /dev/ttyACM0 raw -echo 115200
```

この設定により、バイナリデータが完全に受信可能になりました

---

## Phase 2: PC 側アプリ (Rust) 📝

### 実装予定

1. **USB CDC 接続** (`serialport` crate)
2. **プロトコルパーサー** (同期・CRC検証)
3. **JPEG デコード** (`image` crate)
4. **GUI 表示** (`egui` / `sdl2`)
5. **録画機能** (MJPEG/AVI)

---

## ファイル構成

### 実装ファイル

```
/home/ken/Spr_ws/GH_wk_test/apps/examples/security_camera/
├── camera_manager.h         # カメラ制御ヘッダー
├── camera_manager.c         # カメラ制御実装 (JPEG キャプチャ)
├── mjpeg_protocol.h         # プロトコル定義
├── mjpeg_protocol.c         # CRC計算・パッキング
├── camera_app_main.c        # メインアプリケーション
├── config.h                 # 設定定義
├── Kconfig                  # ビルド設定
└── Makefile                 # ビルドスクリプト
```

### ドキュメント

```
/home/ken/Spr_ws/GH_wk_test/docs/security_camera/
├── MJPEG_PROTOCOL.md        # プロトコル仕様 (詳細設計)
├── MJPEG_SUCCESS.md         # Phase 1A 成功レポート
├── PROTOCOL_TEST_RESULTS.md # テスト結果 (90フレーム)
├── CURRENT_STATUS.md        # 本ドキュメント
├── ERROR_CODE_ANALYSIS.md   # エラーコード解析
├── TROUBLESHOOTING.md       # トラブルシューティング
└── IMPLEMENTATION_NOTES.md  # 実装ノート
```

---

## ビルド・実行

### ビルド

```bash
cd ~/Spr_ws/spresense/sdk
./build.sh
```

### フラッシュ

```bash
sudo tools/flash.sh -c /dev/ttyUSB0 nuttx.spk
```

### 実行

```bash
nsh> security_camera
```

---

## 設定

### config.h

```c
#define CONFIG_CAMERA_WIDTH          320              // QVGA
#define CONFIG_CAMERA_HEIGHT         240              // QVGA
#define CONFIG_CAMERA_FPS            30
#define CONFIG_CAMERA_FORMAT         V4L2_PIX_FMT_JPEG
```

### mjpeg_protocol.h

```c
#define MJPEG_SYNC_WORD          0xCAFEBABE
#define MJPEG_MAX_JPEG_SIZE      65536              // 64 KB
#define MJPEG_MAX_PACKET_SIZE    65550              // 64 KB + 14 bytes
```

### camera_manager.c

```c
// Buffer type: V4L2_BUF_TYPE_VIDEO_CAPTURE (continuous)
// Buffer mode: V4L2_BUF_MODE_RING
// Memory mode: V4L2_MEMORY_USERPTR
// Buffer size: 65,536 bytes × 2
// Alignment: 32 bytes
```

---

## 関連リンク

- [Spresense SDK Documentation](https://developer.sony.com/spresense/)
- [NuttX Video Driver](https://github.com/apache/nuttx)
- [V4L2 API Specification](https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/v4l2.html)

---

## まとめ

### Phase 1A 達成事項 🎉

```
✅ ISX012 カメラ初期化 (V4L2)
✅ JPEG フレーム連続キャプチャ (30 fps)
✅ MJPEG プロトコル設計・実装
✅ CRC-16-CCITT チェックサム
✅ プロトコルパッキング (90/90 成功)
✅ メモリ最適化 (1.5 MB → 192 KB)
✅ 包括的ドキュメント作成
```

### 次の目標: Phase 1B

```
⏳ NuttX USB CDC 設定
⏳ USB パケット送信実装
⏳ PC 接続・転送テスト
```

**Phase 1B 開始準備完了！** 🚀

---

**作成者**: Claude Code (Sonnet 4.5)
**最終更新**: 2025-12-16 14:00
