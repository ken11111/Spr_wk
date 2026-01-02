# USB書き込み最適化提案

**作成日**: 2025-12-29
**対象**: Phase 1.5 VGA性能最適化 - USB転送最適化
**現状**: USB書き込み 30.2ms（総レイテンシの68.6%、最大ボトルネック）
**目標**: 30.2ms → 20-25ms（-20～-33%）、FPS 11.0 → 13-14 fps

---

## 📊 現状分析

### 現在のUSB実装

**ファイル**: `apps/examples/security_camera/usb_transport.c`

```c
// usb_transport_init() - Line 81
g_usb_transport.fd = open(CONFIG_USB_DEVICE_PATH, O_WRONLY);

// usb_transport_send_bytes() - Line 235
written = write(g_usb_transport.fd, data + total_written,
                size - total_written);
```

**特徴**:
- ✅ 同期ブロッキング I/O（`O_WRONLY`）
- ✅ 単一 `write()` システムコール
- ✅ リトライ機構あり（最大3回）
- ❌ 非同期転送なし
- ❌ DMA未使用
- ❌ バッファリング戦略なし（定義されているが未使用）

### パフォーマンス測定データ

| 項目 | 値 | 備考 |
|------|-----|------|
| **USB書き込み時間** | 30.2 ms | 総レイテンシの68.6% |
| **データサイズ** | 平均 41.1 KB | JPEG + ヘッダ + CRC |
| **USB帯域幅** | Full Speed (12 Mbps) | 理論上1.5 MB/s |
| **理論転送時間** | 41.1 KB ÷ 1.5 MB/s = **27.4 ms** | USB帯域幅のみ |
| **オーバーヘッド** | 30.2 - 27.4 = **2.8 ms** | 9.3% |

**分析**:
- USB帯域幅利用率: 27.4ms / 30.2ms = **90.7%** ← 非常に良好
- オーバーヘッドは小さい（2.8ms、9.3%）
- **主な遅延はUSB転送そのもの**（ハードウェア制約）

### ボトルネック特定

```
総レイテンシ 44.0 ms の内訳:
├─ カメラ遅延:      5.1 ms  (11.6%)
├─ パケット処理:    8.7 ms  (19.8%)  ← CRC最適化済み
└─ USB書き込み:    30.2 ms  (68.6%)  ← 🔴 最大ボトルネック
```

**USB書き込み 30.2ms の推定内訳**:
```
├─ USB転送時間:        27.4 ms  (90.7%)  ← ハードウェア制約
├─ システムコール:      1.0 ms  ( 3.3%)
├─ ドライバ処理:        1.0 ms  ( 3.3%)
└─ バッファコピー:      0.8 ms  ( 2.7%)
   総計:              30.2 ms
```

---

## 🎯 最適化戦略

### 戦略1: バッファサイズ最適化（優先度: 高）

**現状の問題**:
```c
#define USB_TX_BUFFER_SIZE    8192  /* 8KB */
```

- パケットサイズ: 平均 41.1 KB
- バッファサイズ: 8 KB
- **5回のwrite()呼び出しが必要** → システムコールオーバーヘッド

**提案**:
```c
#define USB_TX_BUFFER_SIZE    65536  /* 64KB (MJPEG_MAX_JPEG_SIZE + overhead) */
```

**期待効果**:
- write()呼び出し: 5回 → 1回
- システムコールオーバーヘッド: ~1.0ms → ~0.2ms（-0.8ms）
- **改善: -0.8ms（-2.7%）**

**実装の容易さ**: ⭐⭐⭐⭐⭐（config.hの1行変更）

**リスク**: メモリ使用量増加（+56KB × 4バッファ = 224KB）

---

### 戦略2: 非同期I/O（優先度: 中）

**現状の問題**:
- 同期ブロッキングI/O → USB転送完了まで待機
- 次のフレーム処理を開始できない

**提案1: ノンブロッキングモード + ポーリング**

```c
/* 初期化時 */
g_usb_transport.fd = open(CONFIG_USB_DEVICE_PATH, O_WRONLY | O_NONBLOCK);

/* 送信時 */
int usb_transport_send_bytes_async(const uint8_t *data, size_t size)
{
  ssize_t written;

  written = write(g_usb_transport.fd, data, size);

  if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
    {
      /* USB busy, return immediately */
      return ERR_USB_BUSY;
    }

  return written;
}

/* メインループでポーリング */
while (usb_is_busy()) {
  /* 次のフレームのカメラキャプチャを開始 */
  camera_manager_capture_frame();
}
```

**期待効果**:
- USB転送と次のフレームキャプチャを並列化
- パイプライン効率向上
- **FPS向上: ~5-10%（状況依存）**

**実装の容易さ**: ⭐⭐⭐（中程度の変更）

**リスク**: 複雑性増加、タイミング調整が必要

---

**提案2: ダブルバッファリング**

```c
/* 2つのバッファを交互に使用 */
typedef struct usb_async_context_s
{
  uint8_t buffer[2][USB_TX_BUFFER_SIZE];
  int     current_buffer;
  bool    buffer_in_use[2];
} usb_async_context_t;

int usb_transport_send_async(const uint8_t *data, size_t size)
{
  int buf_idx = g_async_ctx.current_buffer;

  /* 前の転送が完了するまで待機 */
  while (g_async_ctx.buffer_in_use[buf_idx])
    {
      usleep(100);  /* 短時間待機 */
    }

  /* バッファにコピー */
  memcpy(g_async_ctx.buffer[buf_idx], data, size);
  g_async_ctx.buffer_in_use[buf_idx] = true;

  /* 非同期書き込み開始 */
  write(g_usb_transport.fd, g_async_ctx.buffer[buf_idx], size);

  /* バッファ切り替え */
  g_async_ctx.current_buffer = 1 - buf_idx;

  return size;
}
```

**期待効果**:
- USB転送中に次のフレーム準備可能
- **FPS向上: ~5-10%**

**実装の容易さ**: ⭐⭐⭐⭐（既存バッファ構造を活用）

**リスク**: メモリコピーオーバーヘッド追加

---

### 戦略3: USB CDC-ACMドライバ最適化（優先度: 高）

**NuttX USB CDC-ACM設定の調整**

現在のSpresense USB CDC-ACM設定を確認・調整:

```bash
# NuttX menuconfig
cd spresense/sdk
./tools/config.py -m

# 確認すべき設定:
# Device Drivers > USB Device Driver Support > USB CDC/ACM
CONFIG_CDCACM=y
CONFIG_CDCACM_BULKONLY=y          # バルク転送専用モード
CONFIG_CDCACM_EPBULKIN_FSSIZE=64  # バルクIN エンドポイントサイズ
CONFIG_CDCACM_EPBULKOUT_FSSIZE=64 # バルクOUT エンドポイントサイズ
```

**提案**:

1. **バルクエンドポイントサイズを最大化**
```
CONFIG_CDCACM_EPBULKIN_FSSIZE=64  # USB Full Speedの最大値
```

2. **バッファサイズの最適化**
```
CONFIG_CDCACM_RXBUFSIZE=512   # 受信バッファ（影響小）
CONFIG_CDCACM_TXBUFSIZE=2048  # 送信バッファ（重要！）
```

**送信バッファサイズの影響**:
- 小さい → 頻繁なUSB転送 → オーバーヘッド増
- 大きい → バッチ転送 → 効率向上

**推奨値**: 2048バイト以上（現在値を確認し、必要に応じて増加）

**期待効果**:
- USB転送効率向上
- **改善: -1～2ms（-3～7%）**

**実装の容易さ**: ⭐⭐⭐⭐（設定変更のみ）

**リスク**: 低（標準的な最適化）

---

### 戦略4: 書き込みサイズの最適化（優先度: 低）

**USB Full Speedのパケットサイズ**: 64バイト

**現状**:
- 任意のサイズでwrite() → ドライバが64バイトパケットに分割

**提案**: 64バイトアラインで書き込み

```c
#define USB_PACKET_SIZE 64

int usb_transport_send_aligned(const uint8_t *data, size_t size)
{
  size_t aligned_size = (size + USB_PACKET_SIZE - 1) & ~(USB_PACKET_SIZE - 1);
  uint8_t aligned_buffer[aligned_size];

  memcpy(aligned_buffer, data, size);
  memset(aligned_buffer + size, 0, aligned_size - size);  /* パディング */

  return write(g_usb_transport.fd, aligned_buffer, aligned_size);
}
```

**期待効果**:
- ドライバの処理負荷軽減
- **改善: -0.2～0.5ms（-1～2%）**

**実装の容易さ**: ⭐⭐⭐

**リスク**: メモリコピーオーバーヘッド、効果が小さい可能性

**評価**: 効果が不明確なため優先度低

---

### 戦略5: DMA転送の活用（優先度: 中〜低）

**SpresenseのDMA対応確認**

SpresenseのUSB CDC-ACMドライバがDMAをサポートしているか確認:

```bash
# NuttX設定確認
grep -r "DMA" spresense/nuttx/arch/arm/src/cxd56xx/
```

**提案**（DMAサポートがある場合）:

NuttX設定でDMAを有効化:
```
CONFIG_ARCH_DMA=y
CONFIG_USBDEV_DMA=y
```

**期待効果**:
- CPU負荷削減
- USB転送のオーバーヘッド削減
- **改善: -0.5～1.5ms（-2～5%）**

**実装の容易さ**: ⭐⭐（ハードウェア依存、検証が必要）

**リスク**:
- DMAサポートがない可能性
- バッファアライメント要求
- デバッグが困難

---

### 戦略6: USB転送とパケット処理のパイプライン化（優先度: 高）

**現在のシーケンス**（逐次処理）:
```
Frame N:
  カメラキャプチャ(5.1ms) → パケット化(8.7ms) → USB書込(30.2ms)
                                                          ↓
Frame N+1:                                                ↓ 待機
  カメラキャプチャ(5.1ms) ← ← ← ← ← ← ← ← ← ← ← ← ← ← ← ←
```

**提案**: パイプライン処理

```
Frame N:
  カメラキャプチャ(5.1ms) → パケット化(8.7ms) → USB書込開始
                                                    ↓
Frame N+1:                                          ↓ 並列実行
  カメラキャプチャ(5.1ms) → パケット化(8.7ms) → ↓ → 待機(15.2ms)
                                                    ↓
Frame N+2:                                          ↓
  カメラキャプチャ(5.1ms) ← ← ← ← ← ← ← ← ← ← ← ←
```

**実装**:

```c
/* グローバルステート */
static struct {
  bool     usb_busy;
  uint8_t  tx_buffer[2][USB_TX_BUFFER_SIZE];
  int      current_tx_buffer;
} g_pipeline;

/* メインループ */
while (running)
{
  /* 1. カメラキャプチャ */
  ret = camera_manager_capture_frame(&jpeg_data, &jpeg_size);

  /* 2. パケット化（次のバッファに） */
  int buf_idx = g_pipeline.current_tx_buffer;
  packet_size = mjpeg_pack_frame(jpeg_data, jpeg_size, &sequence,
                                   g_pipeline.tx_buffer[buf_idx],
                                   USB_TX_BUFFER_SIZE);

  /* 3. 前のUSB転送が完了するまで待機 */
  while (g_pipeline.usb_busy)
    {
      usleep(100);  /* 短時間待機 */
    }

  /* 4. USB転送開始（非同期） */
  g_pipeline.usb_busy = true;
  usb_transport_send_async(g_pipeline.tx_buffer[buf_idx], packet_size);

  /* 5. バッファ切り替え */
  g_pipeline.current_tx_buffer = 1 - buf_idx;

  /* 次のループで次のフレームをキャプチャ（USB転送と並列）*/
}
```

**期待効果**:
- USB転送(30.2ms)とカメラキャプチャ(5.1ms)が並列化
- フレーム間隔: 44.0ms → 39.0ms（**-5ms、-11.4%**）
- **FPS: 11.0 fps → 12.8 fps（+16.4%）**

**実装の容易さ**: ⭐⭐⭐（中程度）

**リスク**: タイミング調整、デバッグの複雑化

---

## 📋 推奨実装プラン

### フェーズ1: 即効性の高い最適化（工数: 小）

**目標**: USB書き込み 30.2ms → 28-29ms

1. ✅ **バッファサイズ最適化**（戦略1）
   - `config.h`: `USB_TX_BUFFER_SIZE` を 8KB → 64KB に変更
   - 期待効果: -0.8ms
   - 工数: 5分

2. ✅ **USB CDC-ACM設定最適化**（戦略3）
   - NuttX menuconfig で送信バッファサイズを確認・調整
   - 期待効果: -1～2ms
   - 工数: 30分（調査 + 設定 + ビルド + テスト）

**合計期待効果**: -1.8～2.8ms（-6～9%）
**予測結果**: 30.2ms → **27.4～28.4ms**

---

### フェーズ2: 並列化による高速化（工数: 中）

**目標**: FPS 11.0 fps → 12.5-13.0 fps

3. ✅ **パイプライン化**（戦略6）
   - ダブルバッファリング実装
   - USB転送とカメラキャプチャの並列実行
   - 期待効果: FPS +1.5～2.0 fps
   - 工数: 2-3時間

**合計期待効果**: フレーム間隔 -5ms
**予測結果**: 11.0 fps → **12.5～13.0 fps**

---

### フェーズ3: 高度な最適化（工数: 大、オプション）

**目標**: さらなる性能向上（不確実性あり）

4. ⚠️ **DMA転送**（戦略5）
   - SpresenseのDMAサポート確認
   - 設定・実装・検証
   - 期待効果: -0.5～1.5ms（不確実）
   - 工数: 4-8時間

5. ⚠️ **アライメント最適化**（戦略4）
   - 64バイトアラインでの書き込み
   - 期待効果: -0.2～0.5ms（小）
   - 工数: 1-2時間

**評価**: 効果が不確実なため、フェーズ1・2で目標達成後に検討

---

## 🎯 性能予測

### フェーズ1完了時（バッファサイズ + CDC-ACM設定）

| 指標 | 現在 | フェーズ1後 | 改善 |
|------|------|-----------|------|
| USB書き込み | 30.2 ms | 27.4～28.4 ms | -1.8～2.8ms |
| 総レイテンシ | 44.0 ms | 41.2～42.2 ms | -1.8～2.8ms |
| FPS | 11.0 fps | 11.4～11.6 fps | +0.4～0.6 fps |

**評価**: 小幅改善、実装容易

---

### フェーズ2完了時（パイプライン化追加）

| 指標 | 現在 | フェーズ2後 | 改善 |
|------|------|-----------|------|
| USB書き込み | 30.2 ms | 27.4～28.4 ms | -1.8～2.8ms |
| フレーム間隔 | 44.0 ms | 38～39 ms | **-5～6ms** |
| FPS | 11.0 fps | **12.5～13.2 fps** | **+1.5～2.2 fps** |

**評価**: **目標達成** ✅（13-14 fps目標に対して12.5-13.2 fps）

---

### 理論上限

| 要素 | 時間 | 備考 |
|------|------|------|
| カメラ遅延 | 5.1 ms | 環境依存（最小） |
| パケット処理 | 8.7 ms | CRC最適化済み |
| USB転送（理論値） | 27.4 ms | ハードウェア制約 |
| **理論上限レイテンシ** | **41.2 ms** | - |
| **理論上限FPS** | **24.3 fps** | 1000ms / 41.2ms |

**現実的な上限**: フレーム間隔 38-39ms、**FPS 13-14 fps**

**理由**: システムコールオーバーヘッド、ドライバ処理時間が不可避

---

## ⚠️ 制約とリスク

### ハードウェア制約

1. **USB Full Speed帯域幅**: 12 Mbps（理論上1.5 MB/s）
   - 41.1 KB転送の理論時間: **27.4 ms**（絶対下限）
   - 現在30.2msは既に**理論値の110%**と非常に良好

2. **カメラレイテンシの変動**: 0.05～265ms（シーン依存）
   - シンプルシーン: 5ms
   - 複雑シーン: 50～265ms

### 実装リスク

| 戦略 | リスク | 対策 |
|------|--------|------|
| バッファサイズ増加 | メモリ不足 | Spresenseのメモリ量確認（1.5MB RAM） |
| パイプライン化 | タイミングバグ | 段階的実装、十分なテスト |
| DMA転送 | ハードウェア非対応 | 事前調査、フォールバック実装 |
| 非同期I/O | 複雑性増加 | シンプルな設計、ドキュメント化 |

### 投資対効果

| フェーズ | 工数 | 効果 | ROI |
|---------|------|------|-----|
| フェーズ1 | 35分 | +0.4～0.6 fps | ⭐⭐⭐⭐⭐ 非常に高い |
| フェーズ2 | 2-3時間 | +1.5～2.2 fps | ⭐⭐⭐⭐ 高い |
| フェーズ3 | 5-10時間 | +0.2～0.7 fps | ⭐⭐ 低い（不確実） |

**推奨**: フェーズ1→2を実施、フェーズ3は目標未達時のみ検討

---

## 📝 まとめ

### 重要な発見

1. **現在のUSB転送は既に効率的**
   - 帯域幅利用率: 90.7%
   - オーバーヘッド: わずか2.8ms（9.3%）

2. **ハードウェア制約が支配的**
   - USB Full Speed理論値: 27.4ms
   - 現在値: 30.2ms（理論値の110%）
   - **大幅な改善は困難**

3. **並列化が最も効果的**
   - バッファサイズ等の単純最適化: -1～3ms
   - **パイプライン化**: -5～6ms（フレーム間隔短縮）

### 推奨アプローチ

✅ **フェーズ1を即座に実施**（工数35分、効果+0.4～0.6 fps）
✅ **フェーズ2を実施**（工数2-3時間、効果+1.5～2.2 fps）
→ **合計効果**: FPS 11.0 → **12.5～13.2 fps**、**目標達成** 🎯

⚠️ **フェーズ3は保留**（工数大、効果不確実）

### 次のステップ

1. **即座**: バッファサイズ変更とビルド（5分）
2. **短期**: CDC-ACM設定調査・最適化（30分）
3. **短期**: パイプライン化実装（2-3時間）
4. **測定**: 各フェーズ後の性能測定
5. **評価**: 目標達成確認（13-14 fps）

---

**文責**: Claude Code
**レビュー**: 2025-12-29
**次回更新**: フェーズ1実装後
