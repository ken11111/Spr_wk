# USB CDC ACM セットアップガイド

**日時**: 2025-12-16
**Phase**: 1B - USB CDC データ転送
**対象**: Spresense (CXD5602) USB CDC ACM 設定

---

## 概要

このドキュメントは、Spresense で USB CDC ACM (Abstract Control Model) を有効化し、MJPEG ストリームを PC に転送するための設定手順を記載します。

---

## 現在の状態

### 有効化済み設定 ✅

```
CONFIG_CDCACM=y                    # USB CDC ACM ドライバー
CONFIG_USBDEV=y                    # USB デバイスサポート
CONFIG_USBDEV_DMA=y                # DMA サポート
CONFIG_USBDEV_DUALSPEED=y          # デュアルスピード (Full/High)
CONFIG_BOARDCTL_USBDEVCTRL=y       # ボードコントロール USB
```

### 不足している設定 ❌

```
CONFIG_CXD56_USBDEV=y              # CXD56xx USB ハードウェア有効化 (CRITICAL!)
CONFIG_SYSTEM_CDCACM=y             # CDC ACM システムユーティリティ
```

---

## 必要な設定

### 1. CXD56_USBDEV (最重要)

**場所**: `/home/ken/Spr_ws/spresense/nuttx/arch/arm/src/cxd56xx/Kconfig:649`

```kconfig
config CXD56_USBDEV
	bool "USB"
	default n
	---help---
		Enables USB
```

**説明**: CXD5602 チップの USB ハードウェアを有効化します。この設定がないと、汎用 USB ドライバー (`CONFIG_USBDEV`) が有効でも USB デバイスは動作しません。

**必須度**: ⭐⭐⭐⭐⭐ (絶対必要)

### 2. SYSTEM_CDCACM

**説明**: CDC ACM デバイスを初期化・管理するシステムユーティリティです。`/dev/ttyACM0` デバイスノードを作成します。

**必須度**: ⭐⭐⭐⭐ (推奨)

---

## 参考: 動作確認済み設定 (USB MSC)

Spresense の USB Mass Storage 設定 (`boards/arm/cxd56xx/spresense/configs/usbmsc/defconfig`) から抜粋:

```
CONFIG_CXD56_USBDEV=y              # ✅ USB ハードウェア有効
CONFIG_USBDEV=y                    # ✅ USB デバイスサポート
CONFIG_USBDEV_DMA=y                # ✅ DMA 使用
CONFIG_USBDEV_DUALSPEED=y          # ✅ Full/High Speed
CONFIG_CDCACM=y                    # ✅ CDC ACM ドライバー
CONFIG_SYSTEM_CDCACM=y             # ✅ CDC ACM ユーティリティ
```

---

## 設定方法

### オプション A: NuttX menuconfig (推奨)

```bash
cd ~/Spr_ws/spresense/nuttx
make menuconfig
```

**設定パス**:

1. **CXD56_USBDEV**:
   ```
   Board Selection
     → CXD56xx Configuration
       → [*] USB
   ```

2. **SYSTEM_CDCACM**:
   ```
   Application Configuration
     → System NSH Add-Ons
       → [*] USB CDC/ACM Device Commands
   ```

3. **CDCACM** (既に有効の場合はスキップ):
   ```
   Device Drivers
     → USB Device Driver Support
       → [*] USB Modem (CDC/ACM) support
   ```

### オプション B: defconfig 編集

#### sdk/.config に追加:

```bash
cd ~/Spr_ws/spresense/sdk
./tools/config.py --kernel feature/usbcdcacm
```

または手動で `nuttx/.config` に追加:

```
CONFIG_CXD56_USBDEV=y
CONFIG_SYSTEM_CDCACM=y
```

---

## 設定適用後の確認

### 1. ビルド

```bash
cd ~/Spr_ws/spresense/sdk
./build.sh
```

### 2. フラッシュ

```bash
sudo tools/flash.sh -c /dev/ttyUSB0 nuttx.spk
```

### 3. デバイスノード確認

Spresense 起動後、NSH で確認:

```bash
nsh> ls /dev/tty*
```

**期待される出力**:
```
/dev/ttyACM0
/dev/ttyS0
/dev/ttyS1
/dev/ttyS2
```

`/dev/ttyACM0` が存在すれば USB CDC ACM が正常に動作しています。

---

## USB CDC デバイス情報

### デバイスクラス

- **Class**: 0x02 (Communications Device Class)
- **SubClass**: 0x02 (Abstract Control Model)
- **Protocol**: 0x01 (AT Commands)

### エンドポイント構成

| EP # | Direction | Type     | Max Packet Size | 用途                 |
|------|-----------|----------|-----------------|---------------------|
| 0    | Bi-dir    | Control  | 64 bytes        | 制御転送            |
| 1    | IN        | Bulk     | 512 bytes       | データ送信 (Tx)     |
| 2    | OUT       | Bulk     | 512 bytes       | データ受信 (Rx)     |
| 6    | IN        | Interrupt| 16 bytes        | ステータス通知      |

**設定値** (現在の .config から):
```
CONFIG_CDCACM_EP0MAXPACKET=64      # EP0 パケットサイズ
CONFIG_CDCACM_EPINTIN=6            # ステータス EP番号
CONFIG_CDCACM_EPINTIN_FSSIZE=16    # Full Speed サイズ
CONFIG_CDCACM_EPINTIN_HSSIZE=16    # High Speed サイズ
CONFIG_CDCACM_EPBULKOUT=2          # Bulk OUT EP番号
CONFIG_CDCACM_EPBULKOUT_FSSIZE=64  # Full Speed サイズ
CONFIG_CDCACM_EPBULKOUT_HSSIZE=512 # High Speed サイズ
CONFIG_CDCACM_EPBULKIN=1           # Bulk IN EP番号
CONFIG_CDCACM_EPBULKIN_FSSIZE=64   # Full Speed サイズ
CONFIG_CDCACM_EPBULKIN_HSSIZE=512  # High Speed サイズ
```

### USB Vendor/Product ID

```
CONFIG_CDCACM_VENDORID=0x0525      # NetChip Technology
CONFIG_CDCACM_PRODUCTID=0xa4a7     # CDC ACM Serial
CONFIG_CDCACM_VENDORSTR="NuttX"
CONFIG_CDCACM_PRODUCTSTR="CDC/ACM Serial"
```

---

## PC 側での認識

### Linux

USB CDC ACM デバイスは `/dev/ttyACM0` として認識されます。

```bash
# デバイス確認
ls -l /dev/ttyACM*

# デバイス情報
udevadm info /dev/ttyACM0

# USB 情報
lsusb -d 0525:a4a7 -v
```

**期待される出力**:
```
Bus 001 Device 010: ID 0525:a4a7 NetChip Technology, Inc.
Device Descriptor:
  bDeviceClass            2 Communications
  bDeviceSubClass         0
  bDeviceProtocol         0
  ...
```

### Windows

デバイスマネージャーで「ポート (COM と LPT)」に「USB Serial Device (COMx)」として表示されます。

---

## トラブルシューティング

### 問題 1: `/dev/ttyACM0` が作成されない

**原因**: `CONFIG_CXD56_USBDEV` が無効

**解決策**:
```bash
cd ~/Spr_ws/spresense/nuttx
make menuconfig
# Board Selection → CXD56xx Configuration → [*] USB
make
```

### 問題 2: PC が USB デバイスを認識しない

**原因 A**: USB ケーブルがデータ転送非対応 (充電専用)

**解決策**: データ転送対応 USB ケーブルを使用

**原因 B**: USB ドライバー未インストール (Windows)

**解決策**: CDC ACM ドライバーをインストール

### 問題 3: `dmesg` に "device descriptor read error"

**原因**: USB 列挙 (enumeration) 失敗

**解決策**:
1. USB ケーブル交換
2. 別の USB ポートに接続
3. ボードリセット後に再試行

---

## 次のステップ

### Phase 1B タスク

1. ✅ **USB CDC Kconfig 調査** (本ドキュメント)
2. ⏳ **USB CDC 設定適用** (次)
3. ⏳ **USB CDC 動作確認**
4. ⏳ **データ送信実装**

### 実装予定

#### usb_transport.c の実装

```c
int usb_transport_init(void)
{
  /* Open /dev/ttyACM0 for writing */
  g_usb_fd = open("/dev/ttyACM0", O_WRONLY);
  if (g_usb_fd < 0)
    {
      LOG_ERROR("Failed to open /dev/ttyACM0: %d", errno);
      return -errno;
    }

  LOG_INFO("USB CDC ACM device opened: /dev/ttyACM0");
  return 0;
}

int usb_transport_send(const uint8_t *data, size_t len)
{
  ssize_t written;

  written = write(g_usb_fd, data, len);
  if (written < 0)
    {
      LOG_ERROR("Failed to write to USB: %d", errno);
      return -errno;
    }
  else if (written != len)
    {
      LOG_WARN("Partial write: %d/%d bytes", written, len);
      return -EIO;
    }

  return 0;
}
```

#### camera_app_main.c の変更

```c
/* USB transport 初期化 (現在 #if 0 でコメントアウト) */
ret = usb_transport_init();
if (ret < 0)
  {
    LOG_ERROR("Failed to initialize USB transport: %d", ret);
    goto cleanup;
  }

/* メインループ内 - パケット送信 */
packet_size = mjpeg_pack_frame(frame.buf, frame.size, &sequence,
                                packet_buffer, MJPEG_MAX_PACKET_SIZE);

/* USB 経由で送信 */
ret = usb_transport_send(packet_buffer, packet_size);
if (ret < 0)
  {
    LOG_ERROR("Failed to send packet: %d", ret);
    error_count++;
  }
```

---

## まとめ

### 必要な変更点

```
CONFIG_CXD56_USBDEV=y              # ⭐ 最重要
CONFIG_SYSTEM_CDCACM=y             # ⭐ 推奨
```

### 期待される結果

1. Spresense 起動時に `/dev/ttyACM0` デバイス作成
2. PC 接続時に USB CDC ACM デバイスとして認識
3. `/dev/ttyACM0` への書き込みで PC にデータ転送
4. MJPEG ストリームの USB 経由転送

---

**作成者**: Claude Code (Sonnet 4.5)
**最終更新**: 2025-12-16
