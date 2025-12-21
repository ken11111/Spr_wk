# クイックスタートガイド

**Phase 1 成功手順 (最小限の手順)**

---

## 前提条件

- Spresense (Main + Extension + Camera)
- USB ケーブル × 2
- Windows 10/11 + WSL2 + usbipd-win

---

## ビルド・フラッシュ

```bash
# 1. ビルド
cd ~/Spr_ws/spresense/sdk
./build.sh

# 2. USB 接続 (Windows PowerShell 管理者権限)
usbipd attach --wsl --busid 1-11  # コンソール用

# 3. フラッシュ (WSL2)
sudo modprobe cp210x
sudo tools/flash.sh -c /dev/ttyUSB0 nuttx.spk
```

---

## 実行

### Spresense 側

```bash
nsh> sercon
nsh> security_camera
```

### PC 側 (WSL2)

**セットアップ** (初回のみ):
```bash
# Windows PowerShell で
usbipd attach --wsl --busid 1-1  # CDC ACM

# WSL2 で
sudo modprobe cdc-acm
sudo chmod 666 /dev/ttyACM0
stty -F /dev/ttyACM0 raw -echo 115200  # ← 重要!
```

**データ受信**:
```bash
cat /dev/ttyACM0 > /tmp/mjpeg_stream.bin
```

(Spresense で `security_camera` 実行後、Ctrl+C で停止)

**確認**:
```bash
hexdump -C /tmp/mjpeg_stream.bin | head -10
hexdump -C /tmp/mjpeg_stream.bin | grep -c "be ba fe ca"  # 90 が期待値
```

---

## トラブルシューティング

### データが破損している

```bash
# Raw モード設定を再実行 (最重要!)
stty -F /dev/ttyACM0 raw -echo 115200
```

### カメラエラー (-17)

```bash
nsh> reboot
```

### USB デバイスが見つからない

```bash
# Windows PowerShell で
usbipd list
usbipd attach --wsl --busid <BUSID>

# WSL2 で
sudo modprobe cdc-acm
sudo modprobe cp210x
```

---

## 期待される結果

- Spresense: `total sent: 672972 bytes`
- PC: `/tmp/mjpeg_stream.bin` が ~657KB
- 同期ワード 90 個検出

---

詳細は `PHASE1_SUCCESS_GUIDE.md` を参照。
