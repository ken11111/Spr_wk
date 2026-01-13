# Phase 7.3.3: アプリケーション層フレーム破棄機能 実装仕様

## 概要

TCP送信が遅延してキューが飽和する問題に対し、アプリケーション層で古いフレームを破棄してキューを解放する機能を実装します。

## 背景

### 問題

Phase 7.3.3テストで173秒後にTCP切断が発生しました：

1. **送信速度 < キャプチャ速度**
   - Camera: 30fps（33ms/frame）
   - TCP送信: 110ms/frame平均（3.3倍遅い）

2. **7個のバッファプール飽和**
   - キュー深度=67-68（満杯）
   - 空バッファなし

3. **TCP送信バッファ（256KB）満杯**
   - 30秒タイムアウト → Socket切断（-107 ENOTCONN）

### 要件

ユーザー要求：
> TCP送信バッファ（256KB）満杯の時、Socket切断をする前に一度バッファをクリアして再度30secまつ。それでもタイムアウトしたら切断する

### 技術的制約

**TCP送信バッファは直接クリアできない**:
- カーネル空間で管理
- アプリケーションからアクセス不可

**代替案**:
- **アプリケーション層（Spresense側のキュー）で古いフレームを破棄**
- キュー解放により、TCP送信バッファに空きができる
- 送信が進み、タイムアウトを回避

## 設計

### A案: アプリケーション層でのフレーム破棄（採用）

#### 概要

USB/TCP threadで送信時間を監視し、連続して遅い場合に古いフレームを破棄します。

#### フロー図

```
┌─────────────────────────────────────────────────────────┐
│ USB/TCP Thread                                          │
└─────────────────────────────────────────────────────────┘
                    │
                    ▼
        ┌───────────────────────┐
        │ dequeue_frame()       │
        │ (action_queueから取得)│
        └───────────────────────┘
                    │
                    ▼
        ┌───────────────────────┐
        │ send_packet()         │
        │ (TCP送信)             │
        └───────────────────────┘
                    │
                    ▼
        ┌───────────────────────┐
        │ 送信時間計測          │
        │ send_time_ms          │
        └───────────────────────┘
                    │
                    ▼
              send_time > 500ms?
                    │
          ┌─────────┴─────────┐
          │                   │
         YES                  NO
          │                   │
          ▼                   ▼
    slow_send_count++   slow_send_count = 0
          │
          ▼
    slow_send_count >= 5?
          │
          ▼
         YES
          │
          ▼
    ┌─────────────────────────┐
    │ 古いフレーム3つを破棄   │
    │ (キュー解放)            │
    └─────────────────────────┘
          │
          ▼
    ┌─────────────────────────┐
    │ 統計カウンター更新      │
    │ dropped_frames += 3     │
    └─────────────────────────┘
          │
          ▼
    slow_send_count = 0
          │
          ▼
        続行
```

#### 判定条件

| 項目 | 閾値 | 理由 |
|------|------|------|
| **遅延判定** | 500ms | 通常の3-4倍（110ms平均）で異常と判断 |
| **連続回数** | 5回 | 一時的な遅延と持続的な遅延を区別 |
| **破棄フレーム数** | 3フレーム | キュー深度を約43%削減（7→4） |

#### パラメータ設定

```c
// camera_threads.c

// フレーム破棄機能の設定
#define SLOW_SEND_THRESHOLD_MS   500   // 遅延判定閾値（ms）
#define SLOW_SEND_COUNT_MAX      5     // 連続遅延回数
#define DROP_FRAME_COUNT         3     // 破棄フレーム数

// 統計カウンター
static uint32_t g_slow_send_count = 0;      // 連続遅延回数
static uint32_t g_dropped_frames = 0;       // 総破棄フレーム数
static uint32_t g_drop_events = 0;          // 破棄イベント回数
```

### 実装箇所

#### 1. camera_threads.c: usb_tcp_thread()

**修正箇所**: USB/TCP送信ループ

```c
// Phase 7.3.3: Frame drop on slow TCP send

static void *usb_tcp_thread(void *arg)
{
    // ... (既存の初期化コード) ...

    uint32_t slow_send_count = 0;

    while (g_thread_running) {
        // フレームをaction_queueから取得
        frame_t *frame = dequeue_frame(&g_action_queue);
        if (!frame) {
            usleep(10000);  // 10ms待機
            continue;
        }

        // 送信時間計測開始
        int64_t send_start = get_timestamp_us();

        // TCP/USB送信
        int ret = send_packet(connfd, frame->data, frame->size);

        // 送信時間計測終了
        int64_t send_time = get_timestamp_us() - send_start;
        int64_t send_time_ms = send_time / 1000;

        // TCP統計更新
        update_tcp_stats(send_time);

        // フレーム破棄判定（Phase 7.3.3）
        if (send_time_ms > SLOW_SEND_THRESHOLD_MS) {
            slow_send_count++;

            if (slow_send_count >= SLOW_SEND_COUNT_MAX) {
                printf("[TCP] Send is slow (%lld ms), dropping old frames...\n", send_time_ms);

                // 古いフレームを破棄
                int dropped = 0;
                for (int i = 0; i < DROP_FRAME_COUNT; i++) {
                    frame_t *old_frame = dequeue_frame(&g_action_queue);
                    if (old_frame) {
                        enqueue_frame(&g_empty_queue, old_frame);  // バッファプールに返却
                        dropped++;
                        g_dropped_frames++;
                    } else {
                        break;  // キューが空
                    }
                }

                if (dropped > 0) {
                    printf("[TCP] Dropped %d frames (total: %u, events: %u)\n",
                           dropped, g_dropped_frames, ++g_drop_events);
                }

                slow_send_count = 0;
            }
        } else {
            slow_send_count = 0;  // 正常送信ならリセット
        }

        // バッファ返却
        enqueue_frame(&g_empty_queue, frame);
    }

    // ... (既存のクリーンアップコード) ...
}
```

#### 2. camera_app_main.c: Metricsパケット拡張

**追加フィールド**:

```c
typedef struct {
    uint32_t sync_word;         // 0xCAFEBEEF
    uint32_t sequence;          // Metricsシーケンス番号
    uint32_t timestamp_ms;      // タイムスタンプ
    uint32_t camera_frames;     // カメラフレーム数
    uint32_t usb_packets;       // USB/TCP送信パケット数
    uint32_t action_q_depth;    // Action queue深度
    uint32_t errors;            // エラー数
    uint32_t tcp_avg_send_us;   // TCP平均送信時間（μs）
    uint32_t tcp_max_send_us;   // TCP最大送信時間（μs）
    // Phase 7.3.3: フレーム破棄統計
    uint32_t dropped_frames;    // 破棄フレーム数
    uint32_t drop_events;       // 破棄イベント回数
} __attribute__((packed)) metrics_packet_t;
```

**パケットサイズ**: 38バイト → 46バイト（+8バイト）

#### 3. 統計表示

**Camera stats出力に追加**:

```c
printf("[CAM] Camera stats: frame=%u, action_q=%u, empty_q=%u, dropped=%u (events=%u)\n",
       g_frame_count,
       get_queue_depth(&g_action_queue),
       get_queue_depth(&g_empty_queue),
       g_dropped_frames,
       g_drop_events);
```

### PC側対応

#### 1. protocol.rs: Metricsパケット拡張

```rust
#[derive(Debug, Clone)]
pub struct MetricsPacket {
    pub sequence: u32,
    pub timestamp_ms: u32,
    pub camera_frames: u32,
    pub usb_packets: u32,
    pub action_q_depth: u32,
    pub errors: u32,
    pub tcp_avg_send_us: u32,
    pub tcp_max_send_us: u32,
    // Phase 7.3.3: フレーム破棄統計
    pub dropped_frames: u32,
    pub drop_events: u32,
}
```

#### 2. metrics.rs: CSV記録拡張

**CSVカラム追加**:

```csv
timestamp,pc_fps,spresense_fps,frame_count,error_count,...,dropped_frames,drop_events
```

#### 3. gui_main.rs: GUI表示

**統計パネルに追加**:

```rust
ui.label(format!("Dropped Frames: {} ({} events)",
                 dropped_frames, drop_events));

if dropped_frames > 0 {
    ui.colored_label(egui::Color32::YELLOW,
                     format!("Drop Rate: {:.2}%",
                             dropped_frames as f32 / total_frames as f32 * 100.0));
}
```

## 動作例

### 正常ケース

```
[TCP] Send time: 95 ms (normal)
[TCP] Send time: 102 ms (normal)
[TCP] Send time: 88 ms (normal)
→ slow_send_count = 0 (継続リセット)
```

### フレーム破棄ケース

```
[TCP] Send time: 612 ms (slow) → slow_send_count = 1
[TCP] Send time: 758 ms (slow) → slow_send_count = 2
[TCP] Send time: 523 ms (slow) → slow_send_count = 3
[TCP] Send time: 691 ms (slow) → slow_send_count = 4
[TCP] Send time: 856 ms (slow) → slow_send_count = 5
[TCP] Send is slow (856 ms), dropping old frames...
[TCP] Dropped 3 frames (total: 3, events: 1)
→ slow_send_count = 0 (リセット)

[TCP] Send time: 110 ms (normal) → キュー解放により送信が正常化
```

## 効果予測

### キュー状態の変化

**破棄前**:
- action_queue深度: 7（満杯）
- empty_queue深度: 0
- TCP送信: 遅延継続

**破棄後**:
- action_queue深度: 4（余裕あり）
- empty_queue深度: 3
- TCP送信: 正常化の可能性

### タイムアウト回避

**シナリオ1: 成功**
```
0s    → 送信遅延開始
5s    → slow_send_count = 5 → 3フレーム破棄
6s    → 送信正常化（110ms）
30s   → タイムアウトせず
```

**シナリオ2: 失敗**
```
0s    → 送信遅延開始
5s    → 3フレーム破棄
10s   → まだ遅い → 再度3フレーム破棄
30s   → タイムアウト → 切断
```

### 副作用

**フレームドロップ**:
- 破棄フレーム数: 3フレーム/イベント
- イベント頻度: 送信遅延が5回連続（約2.5秒）
- 影響: セキュリティカメラではリアルタイム性重視のため許容範囲

**録画への影響**:
- 録画中はフレーム破棄を無効化するオプションを追加（Phase 7.4候補）

## テスト計画

### TC1: 正常動作（フレーム破棄なし）

**手順**:
1. WiFi信号良好な環境でテスト
2. 10分間連続運転
3. フレーム破棄が発生しないことを確認

**期待結果**:
- dropped_frames = 0
- drop_events = 0
- TCP切断なし

### TC2: フレーム破棄動作

**手順**:
1. WiFi信号を意図的に弱くする（距離を離す）
2. 送信時間が500ms以上になることを確認
3. 5回連続遅延 → フレーム破棄を確認

**期待結果**:
- フレーム破棄ログが出力される
- dropped_frames > 0
- drop_events > 0
- TCP切断が回避される（または遅延される）

### TC3: 長時間運転（30分）

**手順**:
1. WiFi信号中程度の環境
2. 30分間連続運転
3. 統計を記録

**期待結果**:
- TCP切断が30分以内に発生しない
- dropped_frames < 総フレーム数の5%
- Metricsパケットが正常に記録される

## 実装スケジュール

### Phase 7.3.3 (本機能)

**実装時間**: 2-3時間

1. **Spresense側実装** (1.5時間):
   - camera_threads.c修正（フレーム破棄ロジック）
   - camera_app_main.c修正（Metricsパケット拡張）
   - ビルド・動作確認

2. **PC側実装** (1時間):
   - protocol.rs修正（Metricsパケット拡張）
   - metrics.rs修正（CSV記録）
   - gui_main.rs修正（GUI表示）
   - ビルド・動作確認

3. **テスト** (1時間):
   - TC1-TC3実施
   - ドキュメント作成

### Phase 7.4 (拡張機能・候補)

**実装時間**: 2-4時間

1. **録画モード対応**:
   - 録画中はフレーム破棄を無効化
   - フラグ制御（is_recording）

2. **適応的破棄数**:
   - キュー深度に応じて破棄数を調整
   - q_depth > 6 → 5フレーム破棄
   - q_depth > 4 → 3フレーム破棄

3. **GUI設定**:
   - フレーム破棄の有効/無効切り替え
   - 閾値調整（500ms → ユーザー設定）

## リスク分析

### リスク1: フレームドロップが頻発

**発生条件**: WiFi信号が常に弱い環境

**影響**:
- 映像が途切れ途切れになる
- ユーザー体験の低下

**対策**:
- GUI警告表示（"WiFi信号が弱いです"）
- 自動フレームレート削減（30fps → 20fps）

### リスク2: 破棄数が不適切

**発生条件**: 3フレーム破棄では不足

**影響**:
- キュー飽和が解消されない
- TCP切断が依然として発生

**対策**:
- 適応的破棄数の実装（Phase 7.4）
- パラメータ調整（3→5フレーム）

### リスク3: 録画時のフレーム欠落

**発生条件**: 録画中にフレーム破棄が発生

**影響**:
- 録画ファイルにフレーム欠落

**対策**:
- 録画中はフレーム破棄を無効化
- 録画品質とリアルタイム性のトレードオフをユーザーに提示

## まとめ

### 実装内容

Phase 7.3.3でアプリケーション層フレーム破棄機能を実装します：

1. ✅ **送信時間監視**: 500ms以上で遅延判定
2. ✅ **連続遅延検出**: 5回連続で破棄トリガー
3. ✅ **キュー解放**: 3フレーム破棄でキュー深度削減
4. ✅ **統計記録**: Metricsパケット拡張、CSV/GUI表示

### 期待される効果

- ✅ **TCP切断回避**: キュー飽和を防ぎ、タイムアウトを回避
- ✅ **リアルタイム性向上**: 古いフレームより新しいフレームを優先
- ✅ **統計可視化**: 破棄状況をGUI/CSVで確認可能

### 次のステップ

1. 仕様承認後、gitマージ作業
2. Spresense側実装
3. PC側実装
4. テスト（TC1-TC3）
5. ドキュメント更新
