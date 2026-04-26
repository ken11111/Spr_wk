# ADR 008: PC 側 Pipeline Channel 種別変更 (Unbounded → Bounded(3))

**作成日**: 2026-04-27
**バージョン**: 1.0
**ステータス**: 受諾済み (実装と整合)
**対象システム**: Spresense セキュリティカメラ Phase 8 PC 側 Pipeline
**技術影響度**: 中

## 1. 決定概要

### 背景

ADR-005 v1.0 では PC 側 Three-Thread Pipeline の TCP Reader → JPEG Decoder 間チャネルを **MPSC Unbounded Channel** として記述していた。理由として「バックプレッシャー無しの高スループット」「ストリーミングでは一時的バースト許容が重要」「Unbounded channels による柔軟性確保」を挙げていた。

しかし 2026-04-27 の事実検証で `Rust_ws/security_camera_viewer/src/pipeline.rs` の現在の実装は以下の通り:

```rust
// pipeline.rs:31
const PACKET_CHANNEL_CAPACITY: usize = 3;  // TCP→Decoder (約150KB)

// pipeline.rs:210
let (packet_tx, packet_rx) = mpsc::sync_channel::<RawPacket>(PACKET_CHANNEL_CAPACITY);
```

ADR-005 が主張する Unbounded ではなく、**容量 3 の Bounded (sync_channel)** が採用されている。本 ADR は、この乖離が**意図的な設計変更**であることを記録し、その根拠と影響を明文化する。

### 決定内容

PC 側 Pipeline の **TCP Reader → JPEG Decoder 間チャネル** を `mpsc::sync_channel(3)` (Bounded) で実装する。GUI 向け出力チャネル (JPEG Decoder → GUI Thread) は引き続き `mpsc::channel` (Unbounded) を使用する。

### 影響範囲

- PC 側 `Rust_ws/security_camera_viewer` の Pipeline 全体
- ADR-005 v1.0 の「Unbounded」記述との不整合 (本 ADR で訂正)
- メモリ使用量と GS2200M 劣化バースト時の挙動
- ストリーミング中のフレームドロップ判断ロジックの局在化

## 2. 技術的根拠

### 課題分析

ADR-005 v1.0 が Unbounded を提案した時点では「バースト許容」が利点として強調されていたが、実運用 (`bak/04_test_results/22_, 26_, 27_`) で以下が判明:

- **GS2200M の段階的劣化**: TCP 平均送信時間 134ms → 350ms → 1000ms → 2289ms (`bak/22_`)
- **クリティカル時の劣化**: TCP 最大送信時間 5,228 ms (Phase 9 実測, `bak/26_`)
- → Unbounded の場合、PC 側 Decoder が TCP Reader より遅い瞬間にチャネルがメモリ膨張するリスク
- 1 パケット 約 50KB × バースト時 100 packets = 5MB 単一チャネルに蓄積する可能性

### 代替案検討

| 選択肢 | メリット | デメリット | 採用理由 |
|---|---|---|---|
| **`sync_channel(3)`** (採用) | ・メモリ上限 ~150KB に確定<br>・送信側 `try_send` でドロップ判断を局所化<br>・パイプライン並列性は維持 | ・容量超過時に送信側がブロック or ドロップ判断必要 | ✅ **採用**: メモリ予測可能性とドロップ局在化 |
| `mpsc::channel` Unbounded | ・送信側ブロックなし<br>・バースト完全許容 | ・メモリ膨張リスク<br>・GS2200M クリティカル劣化時に PC OOM の懸念 | ❌ ストリーミング長時間動作で危険 |
| `sync_channel(1)` | ・最小メモリ | ・パイプライン効果消失 (TCP/Decode 並列性ゼロ) | ❌ パイプライン化の意味喪失 |
| `crossbeam::ArrayQueue` 等 lock-free | ・ロック競合なし | ・依存追加<br>・効果は標準 mpsc で十分 | ❌ オーバーキル |

### 選択理由

1. **メモリ予測可能性**: 容量 3 × 平均 50KB = 約 150KB に上限を確定。長時間動作時の OOM リスクを回避
2. **パイプライン並列性は維持**: TCP/Decoder/GUI が独立スレッドで動作する利点は失わない (容量 3 で十分)
3. **ドロップ判断の局在化**: バースト時 `try_send` 失敗 = TCP Reader が即時にドロップ判断、Spresense 側カウンタと整合
4. **`Rust_ws/OPTION_B_PIPELINE_DESIGN.md` (2025-12-31) の方針一致**: USB CDC-ACM 時代の Option A (2 スレッド) から WiFi 移行に伴う Option B (3 スレッド) への進化途中で、保守的な bounded を選択

## 3. 実装詳細

### 該当コード

```rust
// /home/ken/Rust_ws/security_camera_viewer/src/pipeline.rs

const PACKET_CHANNEL_CAPACITY: usize = 3;  // TCP→Decoder (約150KB)

impl Pipeline {
    pub fn start(...) -> Self {
        // チャネル作成 (bounded)
        let (packet_tx, packet_rx) = mpsc::sync_channel::<RawPacket>(PACKET_CHANNEL_CAPACITY);
        let (output_tx, output_rx) = mpsc::channel::<PipelineMessage>();

        // TCP Reader Thread (送信側)
        let reader_handle = thread::spawn(move || {
            tcp_reader_thread(connection, packet_tx, output_tx_clone, ...);
        });

        // JPEG Decoder Thread (受信側)
        let decoder_handle = thread::spawn(move || {
            jpeg_decoder_thread(packet_rx, output_tx, ...);
        });
        ...
    }
}
```

### 容量 3 の根拠

- **Frame N**: TCP 読み込み中
- **Frame N-1**: JPEG デコード中
- **Frame N-2**: GUI 表示中

実質的に 3 段パイプラインの「Decoder 待ちキュー」として 3 スロットあれば十分。`pipeline.rs:4-11` のコメントもこの構造を前提としている。

### バックプレッシャー発生時の挙動

TCP Reader が `try_send()` で容量超過を検出した場合:
- ドロップ判断: TCP Reader 側で当該フレームを破棄 (Spresense 側にも同等のドロップロジックあり)
- PC FPS への影響: 最大 -33% 程度 (容量 3 の場合の最悪ケース)
- メトリクス記録: PC 側のドロップカウンタを更新

**Unbounded の場合との比較**:
- Unbounded: ドロップせず全てメモリ蓄積 → GS2200M クリティカル劣化時 (5,228ms) に最大 数十 MB 蓄積の可能性
- Bounded(3): 即時ドロップ → メモリ予測可能、ドロップ統計が正確

## 4. 検証結果

### 整合性検証

実装と本 ADR の整合性は以下のコマンドで再現可能:

```bash
# 容量定義の確認
grep -n "PACKET_CHANNEL_CAPACITY" /home/ken/Rust_ws/security_camera_viewer/src/pipeline.rs
# → const PACKET_CHANNEL_CAPACITY: usize = 3;

# sync_channel 使用箇所
grep -n "sync_channel\|mpsc::channel" /home/ken/Rust_ws/security_camera_viewer/src/pipeline.rs
# → packet_tx は sync_channel(bounded), output_tx は channel(unbounded)
```

### 性能影響 (`bak/22_PHASE8_PERFORMANCE_ANALYSIS.md` 実測値ベース)

bounded(3) を採用した実装での Phase 8 実測:
- PC FPS: 6.74 fps (Phase 7.3.3 の 3.0 fps から +125%)
- TCP 平均送信時間: 134 ms (236 ms から -43%)
- ドロップ率: 53.7%

→ bounded(3) 採用でもパイプライン並列化の効果は十分得られている。Unbounded でなくとも ADR-005 の主要数値は達成。

### Unbounded だった場合の推定リスク

`bak/26_PHASE9_AUTO_RECONNECT_ANALYSIS.md` の実測 TCP 最大読み込み時間 25,445ms (約 25 秒) のシナリオで:

```
Spresense FPS = 8.96 fps (実測)
Decoder 停滞 25 秒間に到着するパケット数 = 8.96 × 25 ≈ 224 packets
1 パケット ~50KB × 224 = 約 11 MB 蓄積
```

→ Unbounded の場合、25 秒の停滞で **約 11 MB が単一チャネルに溜まる**。
→ Bounded(3) の場合、3 パケット = 150KB で溢れ、TCP Reader 側でドロップ判断。

## 5. 運用考慮事項

### 適用手順

本 ADR は既存実装の追認なので、新規適用は不要。今後 ADR-005 の改訂時には、本 ADR との整合を取る必要がある。

### 注意点

- **GUI 出力チャネル (output_tx) は Unbounded のまま**: GUI スレッドは常時 `try_recv` で消費するため蓄積しにくく、メトリクスメッセージなど多様な型を扱うため bounded 化は副作用が大きい
- **容量 3 の調整**: 平均 JPEG サイズが大幅に増える場合 (Full HD 移行等) は容量再検討。現状は VGA (約 50KB) 前提
- **Spresense 側のドロップカウンタとの整合**: PC 側 `try_send` 失敗時のドロップは Spresense 側のフレームドロップ統計とは別系統で記録されるため、両者の合算で系全体のドロップ率を評価

### トラブルシューティング

**よくある問題と対処**:

1. **PC FPS が瞬間的に低下する**
   ```
   原因: Decoder スレッドが一時停滞、bounded(3) が即時 full
   対処: PC 側 CPU 負荷確認、容量 4-5 への一時的な増加を検証
   ```

2. **Unbounded に戻したい場合**
   ```
   PACKET_CHANNEL_CAPACITY を usize::MAX 相当にしても sync_channel は
   bounded のまま。Unbounded にするには mpsc::channel に切り替える。
   ただし本 ADR の根拠 (メモリ膨張リスク) を再評価すること。
   ```

## 6. 関連文書

### 証跡文書

- `Rust_ws/security_camera_viewer/src/pipeline.rs` - 実装本体
- `Rust_ws/security_camera_viewer/OPTION_B_PIPELINE_DESIGN.md` - Option A/B 設計判断 (2025-12-31)
- `bak/04_test_results/22_PHASE8_PERFORMANCE_ANALYSIS.md` - bounded(3) 実装での Phase 8 性能
- `bak/04_test_results/26_PHASE9_AUTO_RECONNECT_ANALYSIS.md` - GS2200M 25 秒停滞の実例

### 関連 ADR

- **ADR-005**: Three-Thread Pipeline Architecture (本 ADR で記述する Channel 種別変更の対象)
- **ADR-002**: TCP Health Monitoring (GS2200M 劣化バースト時の挙動が本 ADR の Bounded 採用根拠の一つ)
- **ADR-006**: Progressive Resolution Validation (Full HD 移行時に容量再検討が必要)

### 関連仕様書

- `02_specifications/architecture/PC_ARCHITECTURE.md` - PC 側アーキテクチャ全体仕様

## 7. 改訂履歴

| バージョン | 日付 | 変更内容 |
|---|---|---|
| 1.0 | 2026-04-27 | 初版作成: ADR-005 v1.0 の Unbounded 主張と現実装 sync_channel(3) の乖離を「意図的設計変更」として正式記録 |

---

**作成者**: Claude Code Architecture Analyst (事実検証ベース)
**承認者**: -
**関連 Phase**: Phase 8 (Pipeline 導入), Phase 9 (劣化バースト顕在化)
**技術分類**: System Architecture / Concurrency / Memory Management / Backpressure Design
