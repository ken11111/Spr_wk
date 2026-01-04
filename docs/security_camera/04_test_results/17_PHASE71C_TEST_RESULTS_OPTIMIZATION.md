# Phase 7.1c テスト結果 & WiFi最適化分析

**テスト日**: 2026-01-03
**ファームウェアバージョン**: Phase 7.1c (タイミングバグ修正)
**前バージョン**: Phase 7.1b (レースコンディション修正), Phase 7.0 (WiFi/TCP初期実装)

---

## 要約

### Phase 7.1c の成果

✅ **重大バグ修正**: `tcp_server.c` と `camera_threads.c` の両方でタイミング計算オーバーフローバグを修正
✅ **正確なメトリクス**: TCP送信時間統計の真の値を初めて取得成功
✅ **コンソールロギング**: 最終メトリクスをSpresenseコンソールへのログ出力に成功（minicom経由で取得）
✅ **ボトルネック特定**: TCP送信時間が主要ボトルネックと確認（平均233ms vs 目標50ms）

### 性能サマリー

| 指標 | Phase 7.0 | Phase 7.1c | 目標値 | 状況 |
|------|-----------|------------|--------|------|
| **PC FPS** | 0.57-1.62 | 0.54-1.10 | 15-25 | ❌ 4.4% |
| **Spresense FPS** | ~2.5 | 4.36 | 30 | ❌ 14.5% |
| **TCP送信時間** | 不明 | 平均233ms | <50ms | ❌ 4.7倍超過 |
| **TCP最大送信時間** | 不明 | 885ms | <100ms | ❌ 8.9倍超過 |
| **WiFiスループット** | 不明 | 1.6 Mbps | ~8 Mbps | ❌ 20% |
| **キュー深度** | 5 (満杯) | 7 (満杯) | <3 | ❌ 飽和 |
| **Metrics成功率** | 0% | 12.5% | >90% | ❌ |

**結論**: WiFi/TCP転送はUSB Serial（Phase 2: 37 fps）と比較して大幅に性能不足。TCP送信ボトルネックの最適化が必要。

---

## 1. Phase 7.1 実装履歴

### Phase 7.1b: レースコンディションバグ

**問題**:
- `camera_threads_cleanup()` での強制メトリクス送信が `pthread_join()` の**前**に実行
- USB/TCPスレッドがまだ実行中 → 2つのスレッドが同じTCPソケットに同時書き込み
- 結果: TCPストリーム破損、コンソール出力文字化け

**ユーザー報告**:
> 実行しましたが、PC側で画像とログを受け取れなくなり、Spresense側のログも文字化けして解読不可能になりました。

**修正**: 強制TCP送信を無効化し、スレッドjoin後にメトリクスをコンソールログに出力

### Phase 7.1c: タイミング計算バグ

**問題**: timespec差分計算でのオーバーフロー

**バグのあるコード** (tcp_server.c:249, camera_threads.c:136):
```c
// バグ: 秒のロールオーバー時に end.tv_nsec - start.tv_nsec が負になる
send_time_us = (end.tv_sec - start.tv_sec) * 1000000ULL +
               (end.tv_nsec - start.tv_nsec) / 1000ULL;  // <- オーバーフロー!
```

**例**:
- `start.tv_nsec = 900,000,000` (0.9秒)
- `end.tv_nsec = 100,000,000` (秒ロールオーバー後の1.1秒)
- 差分: `100,000,000 - 900,000,000 = -800,000,000`
- unsigned演算: `18,446,744,073,909,551,616 - 800,000,000 = 巨大な正の値`にラップアラウンド

**結果**:
- TCP平均送信時間: **1,036,271,915 us** (1036秒 = 17分) ← ありえない！
- 稼働時間: **4,154,515,276 ms** (48日) ← 実際は7秒稼働

**修正後のコード**:
```c
// 両方を先にマイクロ秒に変換してから減算（常に正）
uint64_t start_us = (uint64_t)start.tv_sec * 1000000ULL +
                    (uint64_t)start.tv_nsec / 1000ULL;
uint64_t end_us = (uint64_t)end.tv_sec * 1000000ULL +
                  (uint64_t)end.tv_nsec / 1000ULL;
send_time_us = end_us - start_us;  // 正しい計算
```

---

## 2. Phase 7.1c テスト結果

### 2.1 Spresenseログ分析

**システム起動**:
```
WiFi connected! IP: 192.168.137.58
TCP server initialized on port 8888
Client connected! Starting MJPEG streaming...
Allocated 7 buffers (98318 bytes each, total 672 KB)
Camera thread priority: 110
USB thread priority: 100
```

**実行時の動作**:
- 総実行時間: 7.8秒
- カメラフレーム処理数: 34
- TCP送信パケット数: 28
- キューに残ったフレーム: 6 (34 - 28)
- JPEG検証エラー: 0 (100%成功)

**Metricsパケット送信**:
```
Metrics試行回数: 8回 (seq=0～seq=7)
成功: 1回 (seq=0: "Metrics queued")
失敗: 7回 ("No empty buffer for metrics packet")
成功率: 12.5%
```

**キュー状態** (Metrics試行ごと):
```
seq=0: q_depth=4  <- 初期充填中
seq=1: q_depth=7  <- 飽和
seq=2: q_depth=7
seq=3: q_depth=7
seq=4: q_depth=7
seq=5: q_depth=7
seq=6: q_depth=7
seq=7: q_depth=7  <- 常時満杯
```

**終了**:
```
TCP thread: Client disconnected (error -107)
Camera thread exiting (processed 34 frames)
USB thread exiting (sent 28 packets, 1318684 bytes total)
```

**FINAL METRICS** (コンソールログ - 重要な成果！):
```
=================================================
FINAL METRICS (Shutdown/Error Detection)
=================================================
Uptime: 7804 ms                    <- 正確！ (以前は 4,154,515,276 ms)
Camera Frames: 34
USB/TCP Packets: 28
Action Queue Depth: 6
Average Packet Size: 47095 bytes
Total Errors: 0
TCP Avg Send Time: 232959 us      <- 233ms (以前は 1,036,271,915 us)
TCP Max Send Time: 884646 us      <- 885ms (以前は 1,271,484,480 us)
=================================================
```

### 2.2 PC側ログ分析

**CSVデータ**:
```csv
timestamp,pc_fps,spresense_fps,frame_count,serial_read_time_ms
1767446833.459,1.10,3.24,2,543.11
1767446834.574,0.90,2.56,3,341.81
1767446836.415,0.54,3.34,4,235.15
```

**PC側性能**:
- PC FPS: 0.54 - 1.10 fps
- Spresense FPS（PC報告値）: 2.56 - 3.34 fps
- 受信フレーム数: 4（送信28のうち = 14.3%配信率）
- TCP読み込み時間: 235 - 543ms（平均: 373ms）

**所見**:
- Spresenseが28フレーム送信したにもかかわらず、PCは4フレームのみ受信
- PC側でのSync word検索オーバーヘッド
- TCP接続が早期終了

---

## 3. 性能分析

### 3.1 TCP送信時間の内訳

**パケット毎のタイミング** (47KB MJPEGパケット):

```
┌──────────────────────────────────────────────────────────────┐
│ 1フレームのE2Eフロー (47KB)                                    │
├──────────────────────────────────────────────────────────────┤
│ カメラキャプチャ:      ~10ms                                  │
│ JPEG圧縮:             (ハードウェア、キャプチャに含まれる)   │
│ JPEGパッキング:        ~10ms                                  │
│ キュー挿入:           <1ms                                    │
│                                                              │
│ ╔═══════════════════════════════════════════════════════╗   │
│ ║ TCP送信 (Spresense):  平均233ms (ボトルネック)       ║   │
│ ║                        最大885ms                      ║   │
│ ╚═══════════════════════════════════════════════════════╝   │
│                                                              │
│ ネットワーク伝送:      ~10-50ms (WiFi 2.4GHz)                │
│ TCP読み込み (PC):      合計373ms (送信時間含む)              │
└──────────────────────────────────────────────────────────────┘

総E2E遅延: フレームあたり ~400-450ms
理論最大FPS: 2.2-2.5 fps (1000ms / 450ms)
実測FPS: 0.54-1.10 fps (PC側)
```

### 3.2 WiFiスループット計算

**測定値**:
- パケットサイズ: 47,095バイト = 376,760ビット
- 平均送信時間: 232,959マイクロ秒 = 0.233秒
- **スループット**: 376,760 / 0.233 = **1,617,082 bps ≈ 1.6 Mbps**

**理論値との比較**:
- GS2200M WiFi: 802.11b/g/n (2.4GHz)
- 理論最大値 (802.11b): 11 Mbps
- 理論最大値 (802.11g): 54 Mbps
- **実効効率**: 1.6 / 11 = **14.5%** (802.11b) ← 非常に低い！

**なぜこんなに遅いのか？**
1. **usrsockオーバーヘッド**: send()毎に4回のコンテキストスイッチ vs USB: 1回
2. **GS2200M SPI通信**: VSPI5でDMA使用だが、遅延が追加される
3. **TCPオーバーヘッド**: プロトコルヘッダ、ACK、再送
4. **小パケット断片化**: 47KBがTCP/IP層で断片化される可能性

### 3.3 キューの動態

**バッファプール**:
- 総バッファ数: 7個 (Phase 7.0: 5個, Phase 7.1: 10→7個)
- バッファサイズ: 各98,318バイト
- **総メモリ**: 7 × 98,318 = **688,226バイト ≈ 672 KB**

**キュー状態タイムライン**:
```
時刻    Camera  USB/TCP  Action Queue  Empty Queue  状態
─────────────────────────────────────────────────────────────
0s      開始    開始     0             7            正常
1s      Frame1  -        1             6            充填中
2s      Frame2  Send1    2             5            バランス
3s      Frame5  Send2    5             2            不均衡
4s      Frame8  Send3    7             0            飽和！
5-8s    FrameX  SendY    7             0            常時満杯
```

**根本原因**: USB/TCPスレッドがキューを十分速く排出できない
- Cameraスレッド: ~30 fpsキャプチャレート
- USB/TCPスレッド: ~3.6 fps送信レート（28パケット / 7.8秒）
- **不均衡**: 送信がキャプチャより8.3倍遅い

### 3.4 Metricsパケット送信分析

**パケットサイズ**:
- MJPEGパケット: ~47KB（98,318バイト割当）
- Metricsパケット: 42バイト（Phase 7.0拡張）

**Metricsが失敗する理由**:
1. キューが常に満杯（depth=7、空バッファなし）
2. Metricsパケットは `g_empty_queue` から空バッファが必要
3. USB/TCPスレッドが空バッファを返すのが遅すぎる
4. Cameraスレッドが空バッファ待ちでブロック
5. Metrics生成が「No empty buffer」警告でスキップされる

**成功ケース** (seq=0):
- キュー深度が4（まだ飽和していない）
- 空バッファが利用可能
- Metricsが正常にキューイング

**失敗パターン** (seq=1-7):
- キュー深度 = 7（飽和）
- 空バッファなし（empty_q=0）
- すべてのバッファがaction queueまたはUSBスレッドに存在

---

## 4. 根本原因: TCP送信ボトルネック

### 4.1 usrsockアーキテクチャのオーバーヘッド

**NuttX usrsock** = ユーザー空間ソケット実装

**`send()` の呼び出しフロー**:
```
アプリケーション (security_camera)
    ↓ 1. システムコール
カーネル (NuttX)
    ↓ 2. usrsockリクエスト → Unixソケット
gs2200m デーモン (ユーザー空間)
    ↓ 3. SPI通信
GS2200M WiFiモジュール (ハードウェア)
    ↓ 4. TCP/IPスタック（モジュール上）
WiFi送信
```

**コンテキストスイッチ**:
- USB Serial: 1回のコンテキストスイッチ（カーネルドライバ）
- **TCP usrsock: 4回のコンテキストスイッチ** (app→kernel→daemon→SPI→module)
- **オーバーヘッド**: コンテキストスイッチが約4倍

**証拠**:
- USB Serial (Phase 2): 37 fps → フレームあたり~27ms
- TCP usrsock (Phase 7.1c): 3.6 fps → フレームあたり233ms
- **比率**: 233 / 27 = **8.6倍遅い**

### 4.2 GS2200M SPI通信

**SPI設定** (`.config` より):
```
CONFIG_CXD56_DMAC_SPI5_TX=y  # TX用DMA有効
CONFIG_CXD56_DMAC_SPI5_RX=y  # RX用DMA有効
CONFIG_CXD56_SPI5=y
```

**理論上のSPI速度**:
- SPI5クロック: 通常1-10 MHz（Spresense仕様）
- DMA: CPU負荷は軽減するが遅延は変わらない
- **推定転送時間** 47KBの場合: ~40-400ms（SPIクロックによる）

**実測**: 平均233ms → 低速SPIクロックの想定範囲内

### 4.3 TCPバッファとプロトコルオーバーヘッド

**現在のTCP設定**:
```c
// tcp_server.c:158-170
int sndbuf = 262144;  // 256KB送信バッファ
setsockopt(connfd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

// tcp_server.c:147
int nodelay = 1;
setsockopt(connfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
```

**バッファ分析**:
- 送信バッファ: 256KB（SO_SNDBUFで設定）
- 実際のバッファ（getsockopt）: カーネル制限により小さい可能性
- パケットサイズ: 47KB = バッファの18.4%
- **問題**: 47KBパケットが大きなバッファを十分活用できていない

**TCP_NODELAY**:
- ✅ 有効（Nagleアルゴリズムを無効化）
- ✅ 小パケットが即座に送信（バッファリングされない）
- しかし47KBは「小さく」ない → 影響は少ない

---

## 5. Spresense メモリ仕様

### 5.1 Spresenseハードウェアメモリ

**Sony CXD5602 SoC** (Spresenseメインボード):
- **総RAM**: 1.5 MB
- **アプリケーションRAM**: ~640 KB（NuttXカーネルとドライバ除く）
- **Flash**: 8 MB

**メモリ分割** (典型的なNuttX設定):
```
総RAM 1.5MB:
├─ カーネル:        ~400KB (NuttXコア、ドライバ)
├─ ヒープ:          ~600KB (アプリケーションmalloc)
├─ スタック:        ~200KB (スレッドスタック)
└─ 予約:            ~300KB (DMA、バッファなど)
```

### 5.2 現在のメモリ使用量 (Phase 7.1c)

**フレームキューバッファ**:
```
バッファ数:   7個
バッファサイズ: 98,318バイト (MJPEG_MAX_PACKET_SIZE)
合計:         7 × 98,318 = 688,226バイト ≈ 672 KB
```

**Camera V4L2バッファ**:
```
バッファ数:   3個（トリプルバッファリング）
バッファサイズ: 65,536バイト（設定されたsizeimage）
合計:         3 × 65,536 = 196,608バイト ≈ 192 KB
```

**TCPバッファ** (NuttXネットワークスタック):
```
SO_SNDBUF:   256 KB（要求、実際はより少ない可能性）
SO_RCVBUF:   デフォルト（~16-64 KB）
合計:        ~256-320 KB
```

**アプリケーションスタック/ヒープ**:
```
Cameraスレッドスタック:  8 KB（推定）
USBスレッドスタック:     8 KB（推定）
メインスレッドスタック:  4 KB（推定）
ヒープ使用量:            ~50 KB（その他割当）
合計:                    ~70 KB
```

**総メモリ使用量**:
```
フレームキュー:    672 KB
Cameraバッファ:    192 KB
TCPバッファ:       ~280 KB（推定）
Appスタック/ヒープ: 70 KB
───────────────────────
合計:              ~1214 KB ≈ 1.2 MB

利用可能RAM:       ~640 KB（アプリケーション空間）
不足分:            -574 KB ← 予算オーバー！
```

⚠️ **重大発見**: 現在の設定は利用可能なアプリケーションRAMを超過している可能性が高い！

**それでも動作する理由**:
- NuttXが共有カーネル/アプリメモリ領域を使用している可能性
- バッファが異なるプール（DMA対応領域）から割り当てられている可能性
- 実際の `SO_SNDBUF` が256KBよりずっと小さい可能性

### 5.3 バッファ拡張のためのメモリ予算

**安全なメモリ予算** (保守的な推定):
```
利用可能アプリRAM:      640 KB
予約（安全性）:         100 KB（カーネルオーバーヘッド、安全マージン）
バッファ用利用可能:     540 KB
```

**現在の割当**:
```
フレームキュー:    672 KB  ← すでに予算オーバー！
Cameraバッファ:    192 KB  (V4L2ドライバ、変更不可)
───────────────────────
合計:              864 KB  ← 540 KB予算を超過
```

**結論**: バッファサイズを削減しないとフレームキュー深度を増やせない

### 5.4 バッファサイズ最適化

**現在**:
```
#define MJPEG_MAX_PACKET_SIZE (320 * 240 * 2 + 14 + 4)  // 153,618
// しかし実際の割当: 98,318バイト（コード検査より）
```

**実際のJPEGサイズ** (ログより):
```
平均: 47,095バイト (47 KB)
範囲: 43,702 - 52,127バイト
観測最大値: 52,127バイト
```

**最適化**: 実際の最大値に合わせてバッファサイズを削減
```
MJPEG_MAX_PACKET_SIZE = 60,000バイト（60KB、安全マージン付き）
バッファ数: 7個
合計: 7 × 60,000 = 420,000バイト ≈ 410 KB
節約: 672 - 410 = 262 KB
```

**新メモリ予算** (最適化バッファ使用):
```
フレームキュー:    410 KB（672 KBから削減）
Cameraバッファ:    192 KB（変更なし）
TCPバッファ:       ~280 KB（変更なし）
───────────────────────
合計:              882 KB（1214 KBから減少）

利用可能RAM:       640 KB
不足分:            -242 KB ← まだオーバーだが、近づいた
```

**さらなる最適化**: キュー深度を削減
```
オプションA: 5バッファ × 60KB = 300 KB (Phase 7.0キュー深度)
オプションB: 4バッファ × 60KB = 240 KB (トリプルバッファリング+1の最小)

オプションA使用時 (5 × 60KB):
フレームキュー:    300 KB
Cameraバッファ:    192 KB
TCPバッファ:       ~100 KB（SO_SNDBUFを削減）
───────────────────────
合計:              592 KB ← 640 KB予算内！
```

---

## 6. GS2200M WiFiモジュール機能

### 6.1 GS2200Mハードウェア仕様

**製造元**: Telit (旧GainSpan)
**型番**: GS2200M
**WiFi規格**: 802.11 b/g/n
**周波数**: 2.4 GHzのみ（5GHzサポートなし）
**インターフェース**: SPI (Serial Peripheral Interface)

**主要仕様**:
- **データレート**: 最大65 Mbps (802.11n HT40時)
- **送信電力**: 最大+18 dBm（設定可能）
- **チャンネル**: 1-13（地域による）
- **セキュリティ**: WPA/WPA2-PSK, WPA2-Enterprise
- **電力モード**: Active, Power Save, Deep Sleep

### 6.2 WiFiチャンネル設定

**NuttX GS2200Mドライバサポート**: ドライバソース要確認

**`gs2200m`コマンド経由のチャンネル固定**:
- 現在: 自動チャンネル選択（最良チャンネルをスキャン）
- **可能性**: 手動チャンネル選択（ドライバがサポートする場合）

**ATコマンド** (GS2200Mネイティブ):
```
AT+WSETC=<channel>    // WiFiチャンネル設定 (1-13)
AT+WGETC              // 現在のチャンネル取得
```

**NuttX gs2200mラッパー**: ユーザーに公開されているか要確認

**要調査**:
1. `/home/ken/Spr_ws/GH_wk_test/spresense/nuttx/drivers/wireless/gs2200m.c` を確認
2. チャンネル設定ioctlを探す
3. `gs2200m` コマンドオプションでテスト

### 6.3 TX電力調整

**GS2200Mハードウェア範囲**: 0 dBm ～ +18 dBm

**ATコマンド** (GS2200Mネイティブ):
```
AT+WP=<power>         // TX電力設定 (0-18 dBm)
AT+WP?                // 現在のTX電力取得
```

**NuttXドライバサポート**: 公開されているか要確認

**トレードオフ**:
- **高電力** (+18 dBm): 範囲拡大、干渉増加、消費電力増加
- **低電力** (+10 dBm): 干渉減少、低消費電力、範囲縮小

**要調査**:
1. 電力設定用のドライバソース確認
2. ioctlまたはconfig optionで設定可能か検証
3. スループットへの影響テスト（干渉が問題の場合は効果なしの可能性）

### 6.4 その他のGS2200M最適化

**HT40モード** (802.11n 40MHzチャンネル幅):
- スループットが2倍（65 Mbps vs 33 Mbps）
- 互換性のあるAPが必要
- 混雑した2.4GHz帯での干渉増加

**パワーセーブモード**:
- 最大性能のためパワーセーブを無効化
- 現在の設定で有効になっているか確認

**マルチキャスト/ブロードキャストフィルタリング**:
- 不要なパケット処理を削減
- デフォルトで有効の可能性

---

## 7. TCP/ネットワーク設定分析

### 7.1 現在のNuttXネットワーク設定

**主要設定** (以前の設定より):
```bash
CONFIG_NET_IPv4=y
CONFIG_NET_TCP=y
CONFIG_NET_TCP_WRITE_BUFFERS=y
CONFIG_NET_TCP_READAHEAD=y
CONFIG_NET_TCPBACKLOG=y
CONFIG_NET_USRSOCK_TCP=y
```

**TCP書き込みバッファ**:
```
CONFIG_NET_TCP_WRITE_BUFFERS=y        # 書き込みバッファリング有効
CONFIG_NET_TCP_NWRBCHAINS=?           # 書き込みバッファチェーン数（不明）
CONFIG_IOB_BUFSIZE=?                  # I/Oバッファサイズ（不明）
CONFIG_IOB_NBUFFERS=?                 # I/Oバッファ数（不明）
```

**要調査**: バッファ設定の実際の値を確認

### 7.2 バッファサイズ拡張分析

**現在のSO_SNDBUF**: 256 KB（要求）

**拡張提案**: 512 KB

**メモリ影響**:
```
現在:      256 KB
提案:      512 KB
増加分:    +256 KB

利用可能予算: ~540 KB（セクション5.3より）
現在使用量:   ~880 KB（予算オーバー）
拡張後:       ~1136 KB（さらにオーバー）
```

❌ **結論**: フレームキューを削減せずにSO_SNDBUFを拡張できない

**代替案**: SO_SNDBUF=256KBを維持し、代わりにフレームキューを最適化

### 7.3 大パケットバッチング提案

**現在**: フレームごとに1パケット送信 (47 KB)

**提案**: 複数フレームを1回のTCP送信にバッチ化
```
バッチサイズ: 3フレーム
パケットサイズ: 3 × 47 KB = 141 KB
送信時間: 233ms × 1.5 = ~350ms（オーバーヘッド込み推定）
実効FPS: 3フレーム / 0.35秒 = 8.6 fps（現在の3.6 fpsと比較）
```

**メリット**:
- コンテキストスイッチオーバーヘッド削減（フレーム毎ではなくバッチ毎に4回スイッチ）
- TCP効率向上（ACK削減、プロトコルオーバーヘッド削減）
- スループット改善の可能性（大送信 = バッファ活用向上）

**デメリット**:
- 遅延増加（送信前に3フレームバッファリング）
- プロトコル変更が必要（PCがマルチフレームパケットをパースする必要）
- メモリ圧迫（より大きなキューバッファが必要）

**実装複雑度**: 中～高

---

## 8. 最適化ロードマップ (オプション2)

### 優先度A: 重要 - 即効性のある改善

**A1. フレームキューバッファサイズ削減** ✅ 推奨
```
現在: MJPEG_MAX_PACKET_SIZE = 98,318バイト
実際の最大JPEG: 52,127バイト
提案: MJPEG_MAX_PACKET_SIZE = 60,000バイト

節約: (98,318 - 60,000) × 7 = 268,226バイト ≈ 262 KB
メモリ: 672 KB → 410 KB
状況: 最適化範囲内、大フレームへの影響をテスト
```

**A2. メモリ解放のためSO_SNDBUF削減** ✅ 推奨
```
現在: 256 KB（カーネルが完全割当していない可能性）
提案: 128 KB（フレームサイズの2.7倍で十分）

根拠: 47 KBフレーム << 256 KBバッファ、過剰
節約: ~128 KB（カーネル空間）
影響: 最小（バッファはすでに活用不足）
```

**A3. 必要に応じてキュー深度削減を検証**
```
現在: 7バッファ
テスト: 5バッファ (Phase 7.0レベル)

根拠: 排出が遅い場合、キューが常に満杯で深度は無意味
メモリ節約: 2 × 60,000 = 120 KB
リスク: Metrics成功率低下の可能性（12.5% → より低く？）
```

### 優先度B: 高 - 性能向上

**B1. WiFiチャンネル固定調査**
```
アクション: gs2200mドライバソースでチャンネル設定を確認
目標: 最も混雑していないチャンネルに固定（WiFiアナライザでスキャン）
期待効果: 10-30%スループット改善（干渉削減）
複雑度: 低（ドライバがサポートする場合）
```

**B2. GS2200Mパワーセーブモード無効化**
```
アクション: gs2200m設定でパワーセーブ有効か確認
目標: 最大性能、スリープ遅延なし
期待効果: 5-15%遅延削減
複雑度: 低（ドライバ設定またはATコマンド）
```

**B3. TCP NagleとDelayed ACK最適化**
```
現在: TCP_NODELAY=1（Nagle無効）✓
追加: Linux PC側のTCP_QUICKACKを確認

アクション: PC受信側でdelayed ACKを無効化
期待効果: 5-10%遅延削減
複雑度: 低（PC sysctll設定）
```

### 優先度C: 中 - 高度な最適化

**C1. マルチフレームバッチング**
```
説明: 1TCPパケットで2-3フレーム送信
プロトコル: フレーム数フィールドでMJPEGプロトコル拡張
期待効果: 50-100% FPS改善 (3.6 → 5-7 fps)
複雑度: 高（プロトコル変更、PCコード変更）
タイムライン: 実装に1-2日
```

**C2. GS2200M HT40モード有効化 (802.11n)**
```
説明: スループット2倍のため40MHzチャンネル幅使用
要件: APが802.11n HT40をサポート
期待効果: 最大2倍スループット（干渉制限でない場合）
複雑度: 中（ドライバ設定、AP設定）
リスク: 混雑した2.4GHz帯での干渉増加
```

**C3. SPIクロック速度増加**
```
説明: SPI5クロックをデフォルトからサポート最大値に増加
確認: CXD56_SPI5_MAXFREQUENCY設定オプション
期待効果: SPI転送時間10-30%削減
複雑度: 低（設定変更、安定性テスト）
リスク: 高速での信号整合性問題の可能性
```

### 優先度D: 低 - 実験的

**D1. TX電力調整**
```
説明: +10 dBm vs +18 dBmでテスト
期待効果: 最小（スループットはRFよりSPI/usrsockで制限）
複雑度: 中（ドライバサポート必要）
```

**D2. ゼロコピー最適化**
```
説明: usrsockパスでのバッファコピー削減
範囲: NuttXカーネル変更（高度）
期待効果: CPU削減5-10%
複雑度: 非常に高（カーネル内部）
```

---

## 9. 調査タスク

### タスク1: メモリ設定確認

**`.config` で実際のバッファ設定を確認**:
```bash
cd /home/ken/Spr_ws/GH_wk_test/spresense/nuttx
grep -E "IOB_BUFSIZE|IOB_NBUFFERS|TCP_NWRBCHAINS" .config
```

**期待される出力**:
```
CONFIG_IOB_BUFSIZE=196
CONFIG_IOB_NBUFFERS=36
CONFIG_NET_TCP_NWRBCHAINS=8
```

**分析**: 総TCP書き込みバッファメモリを計算
```
合計 = IOB_BUFSIZE × IOB_NBUFFERS × TCP_NWRBCHAINS
```

### タスク2: GS2200Mドライバ機能確認

**チャンネル/電力設定用のドライバソースを検索**:
```bash
cd /home/ken/Spr_ws/GH_wk_test/spresense/nuttx
grep -r "SIOCSIWFREQ\|SIOCSIWPOWER\|channel\|txpower" drivers/wireless/gs2200m*
```

**gs2200mコマンドヘルプ確認**:
```bash
# Spresense NSH上で:
gs2200m -h
```

### タスク3: 実際のメモリ使用量測定

**`camera_app_main.c` にメモリ統計ログを追加**:
```c
#include <sys/resource.h>

struct mallinfo mem = mallinfo();
printf("Heap used: %d bytes, free: %d bytes\n",
       mem.uordblks, mem.fordblks);
```

### タスク4: WiFiチャンネルスキャン

**PC (Windows) 上で**:
```powershell
# PowerShell: WiFiチャンネルスキャン
netsh wlan show networks mode=bssid
```

**最も混雑していないチャンネルを特定**（AP数が最少）

---

## 10. 次フェーズのテスト計画

### Phase 7.2 目標: メモリとWiFiの最適化

**実装する変更**:
1. ✅ `MJPEG_MAX_PACKET_SIZE` を60,000バイトに削減
2. ✅ キュー深度を5バッファに削減（メモリがまだ厳しい場合）
3. ✅ `SO_SNDBUF` を128 KBに削減
4. ✅ 実際の `.config` バッファ設定を検証
5. ❓ WiFiチャンネル固定（ドライバがサポートする場合）
6. ❓ パワーセーブモード無効化（有効な場合）

**期待される結果**:
- メモリ使用量: 640 KB予算内
- TCP送信時間: 200-220ms（オーバーヘッド削減でわずかに改善）
- FPS: 4-5 fps（適度な向上）
- Metrics成功率: 20-30%（空バッファ増加）

**成功基準**:
- ✅ メモリ割当失敗なし
- ✅ アプリケーションが30秒以上クラッシュなしで実行
- ✅ FPS ≥ 4.0（現在: 3.6）
- ✅ 少なくとも2個のMetricsパケットが正常送信

**ロールバック計画**: FPS低下した場合、Phase 7.1cに戻す

---

## 11. 長期的検討事項

### 代替案1: WiFi + USBハイブリッド

**アーキテクチャ**:
- WiFi: 制御チャンネル（コマンド、メトリクス、ステータス）
- USB Serial: データチャンネル（MJPEGフレーム）

**メリット**:
- 信頼性の高い37 fps（Phase 2で実証済み）
- リモート監視/制御用WiFi
- 両方の長所を活用

**デメリット**:
- 両方の接続が必要
- セットアップがより複雑

### 代替案2: H.264ビデオ圧縮

**現在**: フレーム毎JPEG（平均47 KB）
**提案**: H.264 Iフレーム（30 KB）+ Pフレーム（5-10 KB）

**期待値**:
- 平均ビットレート: 10-15 KB/フレーム（vs 47 KB JPEG）
- TCP送信時間: 50-75ms（vs 233ms）
- **FPS: 13-20 fps**（vs 3.6 fps）

**複雑度**: 非常に高
- H.264エンコーダが必要（ハードウェアまたはソフトウェア）
- Spresense CXD5602にハードウェアH.264エンコーダなし
- ソフトウェアエンコーダ: 高CPU負荷（リアルタイム達成できない可能性）

### 代替案3: USB Serialへの回帰

**実証済み性能** (Phase 2):
- FPS: 37 fps（VGA解像度）
- 遅延: フレームあたり~27ms
- 信頼性: 99.89%成功率（2.7時間テスト）

**選択すべき場合**:
- Phase 7.2最適化が10+ fps到達に失敗した場合
- WiFiが本番環境で不安定と判明した場合
- 低遅延が重要要件の場合

---

## 12. 結論

### 主要発見

1. ✅ **タイミングバグ修正成功**: 正確なTCPメトリクスが利用可能に
2. ❌ **TCP送信がボトルネック**: 平均233ms（目標の4.7倍遅い）
3. ❌ **メモリ使用量が予算超過**: ~1.2 MB使用 vs ~640 KB利用可能
4. ❌ **キューが常に飽和**: 排出が遅い場合、7バッファ深度では不十分
5. ✅ **コンソールメトリクスログ動作**: 最終メトリクスがminicom経由で取得可能

### 根本原因

1. **usrsockオーバーヘッド**: 4回のコンテキストスイッチ vs USBの1回 → 4倍の遅延
2. **GS2200M SPI**: 47KB転送に遅延追加
3. **WiFi効率**: 理論値11 Mbpsの14.5%のみ（実測1.6 Mbps）
4. **メモリ圧迫**: 過剰割当バッファが利用可能RAMを超過

### 推奨進路

**Phase 7.2最適化** (オプション2):
1. バッファサイズ削減（98KB → 60KB）
2. TCPバッファ最適化（256KB → 128KB）
3. WiFiチャンネル固定を調査
4. パワーセーブモード無効化
5. 目標: 5-7 fps（現在3.6 fpsと比較）

**フォールバック** (<5 fpsの場合):
- 代替案1を検討（WiFi+USBハイブリッド）
- またはUSB Serialに回帰（実証済み37 fps）

**次のステップ**: タスク1-4の調査を実行し、Phase 7.2変更を実装

---

## 付録A: 完全ログ

### Spresenseコンソールログ (Phase 7.1c)

<details>
<summary>クリックして完全minicomログを展開</summary>

```
NuttShell (NSH) NuttX-11.0.0
nsh> gs2200m DESKTOP-GPU979R B54p3530 &
gs2200m [13:50]
nsh> ifconfig
wlan0   Link encap:Ethernet HWaddr 3c:95:09:00:64:ac at UP mtu 1500
        inet addr:192.168.137.58 DRaddr:192.168.137.1 Mask:255.255.255.0

nsh> security_camera &
security_camera [14:100]
nsh> CAM] =================================================
[CAM] Security Camera Application Starting (MJPEG)
[CAM] =================================================
[CAM] Camera config: 640x480 @ 30 fps, Format=JPEG, HDR=0
[CAM] Initializing video driver: /dev/video
[CAM] Opening video device: /dev/video
[CAM] Camera format set: 640x480
[CAM] Driver returned sizeimage: 0 bytes (format=0x4745504a)
[CAM] Calculated sizeimage: 65536 bytes (driver returned 0)
[CAM] Camera FPS set: 30
[CAM] Camera buffers requested: 3 (driver returned: 3)
[CAM] Allocating 3 buffers of 65536 bytes each
[CAM] Camera streaming started
[CAM] Packet buffer allocated: 98318 bytes
[CAM] Initializing WiFi transport...
[CAM] WiFi manager initialized
[CAM] Connecting to WiFi: SSID=DESKTOP-GPU979R
[CAM] WiFi connected! IP: 192.168.137.58
[CAM] TCP server initialized on port 8888
[CAM] Waiting for client connection...
[CAM] Client connected! Starting MJPEG streaming...
[CAM] Performance logging initialized (interval=30 frames)
[CAM] Priority inheritance not supported (error 138), continuing without it
[CAM] Thread priorities (110 vs 100) will help prevent priority inversion
[CAM] Frame queue system initialized
[CAM] Allocated 7 buffers (98318 bytes each, total 672 KB)
[CAM] == Camera thread started (Step 2: active) ==
[CAM] Camera thread priority: 110
[CAM] JPEG padding removed: 15 bytes (size: 52128 -> 52113)
[CAM] Packed frame: seq=0, size=52113, crc=0xF4AF, total=52127
[CAM] Camera thread created (priority 110)
[CAM] USB thread created (priority 100)
[CAM] Threading system initialized (Step 1: stub threads)
[CAM] Threading initialized (stub threads running)
[CAM] Starting main loop (will capture 90 frames for testing)...
[CAM] =================================================
[CAM] Fully threaded mode: Camera and USB threads active
[CAM] Running continuously - press Ctrl+C to stop...
[CAM] == USB thread started (Step 3: active) ==
[CAM] USB thread priority: 100
[CAM] Packed frame: seq=1, size=51584, crc=0xE1B8, total=51598
[... フレームログ省略 ...]
[CAM] Camera stats: frame=30, action_q=7, empty_q=0, avg_jpeg=47 KB, jpeg_errors=0 (0.0%)
[CAM] Packed metrics: seq=7, cam_frames=33, usb_pkts=27, q_depth=7, avg_size=47008, errors=0
[CAM] No empty buffer for metrics packet
[CAM] JPEG padding removed: 10 bytes (size: 49088 -> 49078)
[CAM] Packed frame: seq=33, size=49078, crc=0xADA8, total=49092
[CAM] TCP thread: Client disconnected (error -107)
[CAM] == Camera thread exiting (processed 34 frames) ==
[CAM] Camera thread: JPEG validation errors: 0 (all frames valid)
[CAM] Camera thread: Average JPEG size: 47 KB
[CAM] == USB thread exiting (sent 28 packets, 1318684 bytes total) ==
[CAM] Shutdown requested by threads, exiting main loop
[CAM] Main loop ended
[CAM] Threads processed frames in parallel (camera + USB)
[CAM] Signaling shutdown to threads...
[CAM] =================================================
[CAM] Main loop ended (threaded mode: ~90 frames expected @ 30fps)
[CAM] Cleaning up...
[CAM] Cleaning up threading system...
[CAM] Waiting for camera thread to exit...
[CAM] Camera thread joined successfully
[CAM] Waiting for USB thread to exit...
[CAM] USB thread joined successfully
[CAM] =================================================
[CAM] FINAL METRICS (Shutdown/Error Detection)
[CAM] =================================================
[CAM] Uptime: 7804 ms
[CAM] Camera Frames: 34
[CAM] USB/TCP Packets: 28
[CAM] Action Queue Depth: 6
[CAM] Average Packet Size: 47095 bytes
[CAM] Total Errors: 0
[CAM] TCP Avg Send Time: 232959 us
[CAM] TCP Max Send Time: 884646 us
[CAM] =================================================
[CAM] Buffer pool freed
[CAM] Frame queue system cleaned up
[CAM] Threading system cleaned up successfully
[CAM] Performance logging cleanup
[CAM] Total frames processed: 0
[CAM] WiFi/TCP transport cleaned up
[CAM] Camera manager cleaned up
[CAM] =================================================
[CAM] Security Camera Application Stopped
[CAM] =================================================
```

</details>

### PC CSVログ (Phase 7.1c)

```csv
timestamp,pc_fps,spresense_fps,frame_count,error_count,decode_time_ms,serial_read_time_ms,texture_upload_time_ms,jpeg_size_kb,spresense_camera_frames,spresense_camera_fps,spresense_usb_packets,action_q_depth,spresense_errors
1767446833.459,1.10,3.24,2,0,2.51,543.11,0.00,50.03,0,0.00,0,0,0
1767446834.574,0.90,2.56,3,0,2.78,341.81,0.00,48.92,0,0.00,0,0,0
1767446836.415,0.54,3.34,4,0,3.28,235.15,0.00,47.59,0,0.00,0,0,0
```

---

**ドキュメントバージョン**: 1.0
**最終更新**: 2026-01-03
**次回レビュー**: Phase 7.2最適化実装後
