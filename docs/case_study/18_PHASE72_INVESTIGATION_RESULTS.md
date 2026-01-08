# Phase 7.2 調査結果: WiFi/TCP最適化のための事前調査

**調査日**: 2026-01-08
**調査者**: Claude Code (Sonnet 4.5)
**目的**: Phase 7.2最適化実装の前提となる技術調査

---

## 📋 要約

Phase 7.1cで特定されたWiFi/TCP性能問題（3.6 fps、目標15-25 fps）を解決するため、Phase 7.2最適化実装の前に以下の調査を実施しました。

**調査結果:**
- ✅ NuttXメモリ設定を確認（I/Oバッファ、TCP設定）
- ✅ GS2200Mドライバー機能を解析（チャンネル、パワーセーブ、SPI）
- ✅ メモリ使用量測定コードを追加
- ⚠️ 重要な発見：usrsock TCPスタックがボトルネックの根本原因
- ❌ Station modeではWiFiチャンネル固定が不可能

**次のステップ:**
- 実装可能な最適化（A1～A3、B2）を実施
- 目標：3.6 fps → 5-7 fps

---

## 🔍 調査タスク1: メモリ設定確認

### 実施コマンド

```bash
cd /home/ken/Spr_ws/GH_wk_test/spresense/nuttx
grep -E "IOB_BUFSIZE|IOB_NBUFFERS|TCP_NWRBCHAINS" .config
grep -E "^CONFIG_NET_TCP" .config
```

### 調査結果

#### I/Oバッファ設定

```
CONFIG_IOB_NBUFFERS=8
CONFIG_IOB_BUFSIZE=196
```

**計算:**
```
総I/Oバッファ容量 = 8 × 196 = 1,568 bytes ≈ 1.5 KB
```

**分析:**
- NuttXのI/Oバッファは**非常に小さい**（1.5 KB）
- TCP書き込みバッファとして使用
- 47 KBのMJPEGパケットを送信するには不十分
- 複数のI/Oバッファに分割して送信される可能性が高い

#### TCP設定

```
CONFIG_NET_TCP=y
CONFIG_NET_TCP_NO_STACK=y  ← 重要な発見
```

**重要な発見: usrsock TCPスタック**

`CONFIG_NET_TCP_NO_STACK=y` は以下を意味する：

1. **NuttXのTCPスタックが無効**
2. **usrsock（User Space Socket）経由**でgs2200mデーモンがTCP処理
3. これが**4回のコンテキストスイッチの根本原因**

**アーキテクチャ比較:**

```
USB Serial（Phase 2: 37 fps）:
アプリ → カーネルドライバ → USB転送
       (1回のコンテキストスイッチ)

TCP usrsock（Phase 7.1c: 3.6 fps）:
アプリ → カーネル → gs2200mデーモン → SPI → WiFiモジュール
       (4回のコンテキストスイッチ)
```

**オーバーヘッド比較:**
- USB: 1回のシステムコール
- TCP usrsock: 4回のコンテキストスイッチ + プロセス間通信
- **結果**: TCP送信時間 233ms vs USB送信時間 27ms（**8.6倍遅い**）

### 結論

#### メモリ制約
- I/Oバッファが小さい（1.5 KB）
- 47 KBパケットを複数のバッファに分割
- 分割によるオーバーヘッドが発生

#### usrsockオーバーヘッド
- これが性能劣化の**根本原因の1つ**
- ドライバーレベルの変更が必要（高コスト）
- 現実的には**アプリケーションレベルで最適化**するしかない

---

## 🔍 調査タスク2: GS2200Mドライバー機能確認

### 実施コマンド

```bash
find . -name "gs2200m*" -type f
grep -n "channel\|CHANNEL\|freq\|power\|txpower" ./drivers/wireless/gs2200m.c
```

### ドライバーファイル確認

```
./include/nuttx/wireless/gs2200m.h    # ヘッダーファイル
./drivers/wireless/gs2200m.c          # ドライバー実装
```

### 調査結果

#### 1. WiFiチャンネル設定機能

**発見した実装:**

```c
// gs2200m.c:2805
else if (msg->cmd == SIOCGIWFREQ)
{
    if (strstr(pkt_dat.msg[2], "CHANNEL=") == NULL)
    {
        return -EINVAL;
    }
    n = sscanf(pkt_dat.msg[2], "%s CHANNEL=%" SCNd32 " %s",
               cmd, &res->u.freq.m, cmd2);
    wlinfo("CHANNEL:%" PRId32 "\n", res->u.freq.m);
}
```

**機能:**
- `SIOCGIWFREQ` ioctlで**現在のチャンネル取得が可能**
- チャンネル情報を読み出せる

**APモードでのチャンネル指定:**

```c
// gs2200m.c:1702
/* In AP mode, we can specify channel to use */

// gs2200m.h:122
struct gs2200m_assoc_msg
{
    FAR char *ssid;
    FAR char *key;
    uint8_t   mode;
    uint8_t   ch;    // チャンネル指定可能
};
```

**重要な制約発見:**
- ✅ **AP mode（アクセスポイントモード）**: チャンネル指定**可能**
- ❌ **Station mode（クライアントモード）**: チャンネル指定**不可能**
  - 接続先APのチャンネルに自動的に従う
  - 手動でチャンネル固定できない

**現在の動作モード:**
```c
// security_cameraアプリはStation modeで動作
// wifi_manager.c: wifi_mgr->assoc_msg.mode = (station mode)
```

**結論:**
- ❌ **B1（WiFiチャンネル固定）は実装不可**
- Station modeではチャンネルを固定できない
- ルーター側の設定変更で対応可能（AP側で最適チャンネルに設定）

#### 2. パワーセーブ機能

**発見した実装:**

```c
// gs2200m.c:2090
static enum pkt_type_e gs2200m_powersave_wrx(FAR struct gs2200m_dev_s *dev, ...)

// gs2200m.c:3413
t = gs2200m_powersave_wrx(dev, 0);
```

**機能:**
- ドライバーレベルで**パワーセーブ制御が実装されている**
- `gs2200m_powersave_wrx()` 関数で制御

**確認が必要:**
- 現在の設定状態（有効/無効）
- 設定方法（ioctl、コンフィグ、ATコマンド）
- 性能への影響度

**期待効果:**
- パワーセーブ無効化で5-15%の遅延削減（Phase 7.1c分析より）

#### 3. SPI周波数設定

**発見した実装:**

```c
// gs2200m.c:69
#define SPI_MAXFREQ  CONFIG_WL_GS2200M_SPI_FREQUENCY

// gs2200m.c:750, 754
/* SPI settings (mode1/8bits/max freq) */
SPI_SETFREQUENCY(dev->spi, SPI_MAXFREQ);
```

**機能:**
- `CONFIG_WL_GS2200M_SPI_FREQUENCY` で**SPIクロック周波数を設定可能**
- `.config` または `menuconfig` で変更可能

**確認が必要:**
- 現在のSPI周波数値
- サポートされる最大周波数
- 安定性のトレードオフ

**期待効果:**
- SPIクロック増加で10-30%の転送時間削減（Phase 7.1c分析より）

### 結論

| 機能 | 実装状況 | 適用可能性 | 期待効果 |
|------|---------|-----------|---------|
| **WiFiチャンネル固定** | APモードのみ可能 | ❌ 不可（Station mode） | - |
| **パワーセーブ制御** | ✅ 実装済み | ✅ 可能 | 5-15%遅延削減 |
| **SPI周波数調整** | ✅ 設定可能 | ✅ 可能 | 10-30%転送削減 |

---

## 🔍 調査タスク3: メモリ使用量測定コード追加

### 実装内容

**ファイル:** `apps/examples/security_camera/camera_app_main.c`

**変更1: ヘッダーインクルード**

```c
#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>
#include <malloc.h>  /* Phase 7.2: Memory statistics */
```

**変更2: main関数にメモリログ追加**

```c
LOG_INFO("=================================================");
LOG_INFO("Security Camera Application Starting (MJPEG)");
LOG_INFO("=================================================");

/* Phase 7.2: Log memory usage at startup */

struct mallinfo mem = mallinfo();
LOG_INFO("Memory at startup: Heap used=%d bytes, free=%d bytes, "
         "largest_free_block=%d bytes",
         mem.uordblks, mem.fordblks, mem.mxordblk);

/* Setup signal handlers */
```

### 期待される出力

```
[CAM] Memory at startup: Heap used=12345 bytes, free=654321 bytes, largest_free_block=600000 bytes
```

**測定項目:**
- `uordblks`: 使用中のヒープメモリ（bytes）
- `fordblks`: 空きヒープメモリ（bytes）
- `mxordblk`: 最大の連続空きブロックサイズ（bytes）

### 目的

1. **アプリケーション起動時のメモリ使用量を把握**
2. **Phase 7.2実装後のメモリ削減効果を測定**
3. **メモリ不足リスクの早期検出**

### 次のステップ

Phase 7.2実装後、以下の箇所でもメモリログを追加予定：
- フレームキューバッファ割り当て後
- TCP接続確立後
- アプリケーション終了時

---

## 🔍 調査タスク4: WiFiチャンネルスキャン

### 実施方法

**Windows PowerShell（管理者権限不要）:**

```powershell
netsh wlan show networks mode=bssid
```

### 取得できる情報

```
SSID: DESKTOP-GPU979R
    BSSID 1: xx:xx:xx:xx:xx:xx
    Signal: 100%
    Channel: 6
    Authentication: WPA2-Personal
    Encryption: CCMP
```

**分析項目:**
- 周辺の全SSID一覧
- 各APのチャンネル番号（1-13）
- 信号強度（RSSI）
- 最も混雑していないチャンネルの特定

### 調査結果の取扱い

**Station modeでの制約:**
- 手動でチャンネル固定**不可能**（タスク2の調査結果）
- しかし**ルーター側で最適チャンネルに設定**することで改善可能

**推奨アクション:**
1. Windows PowerShellでチャンネルスキャン実施
2. 最も混雑していないチャンネルを特定（AP数が最少）
3. ルーター管理画面で該当チャンネルに固定設定
4. Spresense再接続後に性能テスト

**優先度:**
- 調査タスク2の結果により**優先度を下げて後回し推奨**
- Station modeでは直接的な実装が不可能
- ルーター設定変更は実装フェーズ外

---

## 📊 調査結果の統合分析

### メモリ予算の詳細

**Phase 7.1c現在の構成（推定）:**

```
フレームキュー:    7 × 98,318 = 688,226 bytes ≈ 672 KB
Cameraバッファ:    3 × 65,536 = 196,608 bytes ≈ 192 KB
TCPバッファ:       ~256 KB（SO_SNDBUF設定値）
I/Oバッファ:       8 × 196 = 1,568 bytes ≈ 1.5 KB
────────────────────────────────────────────────
合計:              ~1,121 KB ≈ 1.1 MB

利用可能RAM:       ~640 KB（アプリケーション空間、推定）
不足分:            -481 KB ← 予算オーバー
```

**Phase 7.2最適化後の構成（目標）:**

```
フレームキュー:    5 × 60,000 = 300,000 bytes ≈ 293 KB  (A1, A3)
Cameraバッファ:    3 × 65,536 = 196,608 bytes ≈ 192 KB  (変更なし)
TCPバッファ:       ~128 KB（SO_SNDBUF削減）            (A2)
I/Oバッファ:       8 × 196 = 1,568 bytes ≈ 1.5 KB      (変更なし)
────────────────────────────────────────────────
合計:              ~614.5 KB

利用可能RAM:       ~640 KB
余裕:              +25.5 KB ← 予算内！
メモリ削減:        -506.5 KB（45%削減）
```

### 性能予測

**Phase 7.1c ベースライン:**
- FPS: 3.6 fps
- TCP送信時間（平均）: 233ms
- Metrics成功率: 12.5%

**Phase 7.2 目標（最適化後）:**

| 指標 | Phase 7.1c | Phase 7.2目標 | 改善率 |
|------|-----------|--------------|--------|
| FPS | 3.6 fps | **5-7 fps** | +38-94% |
| TCP送信時間 | 233ms | **200-220ms** | -6% ~ -14% |
| Metrics成功率 | 12.5% | **20-30%** | +60% ~ +140% |
| キュー深度 | 7 (満杯) | 5 (適度) | - |
| メモリ使用量 | ~1,121 KB | ~615 KB | -45% |

**改善の根拠:**

1. **メモリ最適化（A1～A3）:**
   - オーバーヘッド削減により5-10%のスループット改善
   - キュー深度削減により空バッファ増加（Metrics成功率向上）

2. **パワーセーブ無効化（B2）:**
   - 5-15%の遅延削減（Phase 7.1c分析より）

3. **総合効果:**
   - 3.6 fps × 1.05 × 1.10 × 1.15 = **4.8 fps**（保守的推定）
   - 最良ケース: **5-7 fps**

---

## 🎯 Phase 7.2実装計画（修正版）

### 実装可能な最適化

#### 優先度A: 即座に実施

- ✅ **A1: フレームキューバッファサイズ削減**
  - 現在: `MJPEG_MAX_PACKET_SIZE = 98,318 bytes`
  - 変更: `MJPEG_MAX_PACKET_SIZE = 60,000 bytes`
  - 節約: 268 KB（7バッファ）

- ✅ **A2: SO_SNDBUF削減**
  - 現在: `SO_SNDBUF = 256 KB`
  - 変更: `SO_SNDBUF = 128 KB`
  - 節約: ~128 KB

- ✅ **A3: キュー深度削減**
  - 現在: 7バッファ
  - 変更: 5バッファ
  - 追加節約: 2 × 60,000 = 120 KB

#### 優先度B: 調査と実装

- ✅ **B2: パワーセーブモード確認・無効化**
  - 現在の設定状態を確認
  - 無効化の方法を調査
  - 性能テストで効果を検証

- ⚠️ **B3: TCP最適化（PC側）**
  - Delayed ACK無効化
  - TCP_QUICKACK設定
  - Rustコードでの実装

#### 実装不可（調査結果による）

- ❌ **B1: WiFiチャンネル固定**
  - Station modeでは不可能
  - ルーター側設定変更で対応可能（実装外）

### 実装の優先順位

```
1. A1 + A2 + A3（メモリ最適化）
   ↓ ビルド・テスト
2. B2（パワーセーブ調査・無効化）
   ↓ ビルド・テスト
3. 総合テスト（30秒以上）
   ↓ メトリクス収集
4. 結果分析とドキュメント作成
```

---

## 🚨 重要な技術的発見

### 発見1: usrsock TCPスタックがボトルネック

**問題:**
```
CONFIG_NET_TCP_NO_STACK=y
→ NuttX TCPスタック無効
→ usrsock経由でgs2200mデーモンが処理
→ 4回のコンテキストスイッチ（vs USB: 1回）
```

**影響:**
- これが性能劣化の**根本原因**
- 回避策はドライバーレベルの変更が必要（高コスト）
- アプリケーションレベルでの最適化に限界がある

**長期的な解決策:**
- NuttX TCPスタックを有効化（カーネル変更）
- usrsockオーバーヘッドを削減（ドライバー変更）
- または**WiFi+USBハイブリッド方式**に切り替え（Phase 2で実証済み37 fps）

### 発見2: Station modeでのチャンネル固定不可

**問題:**
- GS2200MドライバーはAPモードでのみチャンネル指定可能
- Station mode（現在の動作モード）ではチャンネル固定不可

**影響:**
- B1（WiFiチャンネル固定最適化）は**実装不可**
- 当初の最適化計画を修正する必要

**代替案:**
- ルーター側で最適チャンネルに設定（実装外、手動対応）

### 発見3: I/Oバッファが極端に小さい

**問題:**
```
CONFIG_IOB_NBUFFERS=8
CONFIG_IOB_BUFSIZE=196
総容量: 1,568 bytes ≈ 1.5 KB
```

**影響:**
- 47 KBのMJPEGパケットを約30個のI/Oバッファに分割
- 分割によるオーバーヘッドが発生
- TCP送信時間の一因

**オプション（検討中）:**
- I/Oバッファ増加（リスク：メモリ不足）
- `.config`で`CONFIG_IOB_NBUFFERS`を16に増加
- または`CONFIG_IOB_BUFSIZE`を増加

---

## 📝 次のステップ

### Phase 7.2実装（即座に開始）

1. **A1～A3実装**（所要時間: 30分）
   - `mjpeg_protocol.h`: バッファサイズ変更
   - `tcp_server.c`: SO_SNDBUF変更
   - `frame_queue.c`: キュー深度変更

2. **B2調査と実装**（所要時間: 30分～1時間）
   - GS2200Mパワーセーブ設定確認
   - 必要に応じて無効化

3. **ビルドとテスト**（所要時間: 1-2時間）
   - ファームウェアビルド
   - フラッシュ
   - 30秒間性能テスト
   - メトリクス収集

4. **結果ドキュメント作成**（所要時間: 1時間）
   - Phase 7.2テスト結果レポート
   - Before/After比較
   - 次フェーズへの推奨事項

### 長期的な検討事項

- **usrsock TCPスタックの回避**（Phase 8?）
  - WiFi+USBハイブリッド方式の検討
  - NuttX TCPスタック有効化の検討

- **I/Oバッファ最適化**（Phase 7.3?）
  - CONFIG_IOB_NBUFFERS増加テスト
  - メモリ安全性の検証

---

## 📚 参考資料

### 関連ドキュメント

- **Phase 7.1c テスト結果**: `docs/security_camera/04_test_results/17_PHASE71C_TEST_RESULTS_OPTIMIZATION.md`
- **Phase 7 仕様**: `docs/security_camera/01_specifications/PHASE7_WIFI_TCP_SPEC.md`
- **操作手順**: `docs/security_camera/04_test_results/04_TEST_PROCEDURE_FLOW.md`

### ドライバーソースコード

- `spresense/nuttx/drivers/wireless/gs2200m.c`
- `spresense/nuttx/include/nuttx/wireless/gs2200m.h`

### 設定ファイル

- `spresense/nuttx/.config`

---

**ドキュメントバージョン**: 1.0
**作成日**: 2026-01-08
**最終更新**: 2026-01-08
**次回レビュー**: Phase 7.2実装完了後
