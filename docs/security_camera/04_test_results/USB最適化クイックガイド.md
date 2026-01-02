# USB最適化クイックガイド

**対象**: 即座に実装可能なUSB最適化
**所要時間**: 約35分
**期待効果**: FPS 11.0 → 11.4-11.6 fps

---

## ⚡ フェーズ1: 即効性の高い最適化（推奨）

### 1. バッファサイズ最適化（5分）

**ファイル**: `/home/ken/Spr_ws/GH_wk_test/apps/examples/security_camera/config.h`

**変更内容**:
```c
/* USB Configuration */
// 変更前
#define CONFIG_USB_TX_BUFFER_SIZE    8192   /* 8KB */

// 変更後
#define CONFIG_USB_TX_BUFFER_SIZE    65536  /* 64KB */
```

**理由**:
- 現在: パケット41KBを8KBバッファで送信 → 5回のwrite()
- 変更後: 1回のwrite()で完結
- システムコールオーバーヘッド削減

**期待効果**: -0.8ms

---

### 2. USB CDC-ACM設定最適化（30分）

#### ステップ1: 現在の設定確認

```bash
cd /home/ken/Spr_ws/GH_wk_test/spresense/sdk

# NuttX設定確認
grep -E "CONFIG_CDCACM|CONFIG_USBDEV" .config | grep -v "^#"
```

**確認すべき項目**:
- `CONFIG_CDCACM_EPBULKIN_FSSIZE` - バルクINエンドポイントサイズ
- `CONFIG_CDCACM_TXBUFSIZE` - 送信バッファサイズ（重要）
- `CONFIG_CDCACM_BULKONLY` - バルク転送専用モード

#### ステップ2: 設定変更（必要に応じて）

```bash
# menuconfigを起動
./tools/config.py -m

# 設定画面で以下を確認・変更:
# Device Drivers > USB Device Driver Support > USB CDC/ACM
#   CONFIG_CDCACM_EPBULKIN_FSSIZE = 64  (USB Full Speedの最大値)
#   CONFIG_CDCACM_TXBUFSIZE = 2048 以上 (推奨: 4096)
#   CONFIG_CDCACM_BULKONLY = y

# 保存して終了: Save > Exit
```

#### ステップ3: リビルドとフラッシュ

```bash
# クリーンビルド
PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin make clean
PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin make

# フラッシュ
cd tools
sudo -E PATH=$HOME/spresenseenv/usr/bin:$PATH ./flash.sh -c /dev/ttyUSB0 ../../nuttx/nuttx.spk
```

**期待効果**: -1～2ms

---

## 📊 期待される結果

### フェーズ1完了後

| 指標 | 現在 | フェーズ1後 | 改善 |
|------|------|-----------|------|
| USB書き込み | 30.2 ms | 27.4～28.4 ms | -1.8～2.8ms |
| 総レイテンシ | 44.0 ms | 41.2～42.2 ms | -1.8～2.8ms |
| FPS | 11.0 fps | **11.4～11.6 fps** | +0.4～0.6 fps |

---

## 🧪 テスト方法

### テスト実行

```bash
# minicomで接続
minicom -D /dev/ttyUSB0 -b 115200

# アプリ実行
nsh> security_camera

# 90フレーム完了まで待機
# 結果を記録
```

### 確認すべき指標

1. **USB書き込み時間**: Window平均が27-28msになっているか
2. **FPS**: 11.4-11.6 fps程度になっているか
3. **総レイテンシ**: 41-42ms程度になっているか
4. **エラー**: エラー、ドロップフレーム、リトライがゼロか

---

## 🎯 次のステップ

フェーズ1完了後、さらなる最適化が必要な場合:

### フェーズ2: パイプライン化（工数: 2-3時間）

**目標**: FPS 12.5-13.2 fps

**実装内容**:
- ダブルバッファリング
- USB転送とカメラキャプチャの並列実行
- 非同期I/O

**詳細**: [12_USB書き込み最適化提案.md](./12_USB書き込み最適化提案.md) 参照

---

## ⚠️ トラブルシューティング

### ビルドエラー

**エラー**: `USB_TX_BUFFER_SIZE redefined`

**対策**: `usb_transport.h`でも定義されている場合、`config.h`の定義を優先

```c
// usb_transport.h
#ifndef USB_TX_BUFFER_SIZE
#define USB_TX_BUFFER_SIZE    8192
#endif
```

### メモリ不足

**エラー**: ビルド時にメモリ不足エラー

**対策**: バッファ数を削減

```c
// config.h
#define CONFIG_USB_TX_BUFFER_COUNT   2  // 4 → 2 に削減
```

### 性能改善が見られない

**確認事項**:
1. カメラの向きは固定されているか（シンプルなシーン）
2. クリーンビルドを実施したか
3. デバイスを再起動したか
4. CDC-ACM設定が正しく反映されているか（`.config`確認）

---

## 📋 チェックリスト

- [ ] `config.h`でバッファサイズを64KBに変更
- [ ] NuttX設定でCDC-ACM送信バッファサイズ確認
- [ ] 必要に応じてCDC-ACM設定を調整
- [ ] クリーンビルド実施
- [ ] フラッシュ成功
- [ ] テスト実行（90フレーム）
- [ ] 結果記録（USB書き込み時間、FPS）
- [ ] 性能改善を確認（+0.4～0.6 fps）

---

**文責**: Claude Code
**作成日**: 2025-12-29
**所要時間**: 約35分
**期待効果**: +0.4～0.6 fps
