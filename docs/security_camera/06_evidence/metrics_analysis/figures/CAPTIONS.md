# Performance Metrics 可視化 — 考察まとめ (差し替え準備)

**作成日**: 2026-05-22
**目的**: `visualization.py` で生成した 20 図の説明文と主要発見を整理し、`performance_trends.md` への差し替え準備とする。**本書は反映前の中間成果物** で、Caption 検討用。差し替え判断後に本書の内容を `performance_trends.md` に転記する。
**生成スクリプト**: [`analysis_tools/visualization.py`](../analysis_tools/visualization.py)
**入力**: 59 CSV / 9,395 サンプル (`/home/ken/Rust_ws/security_camera_viewer/release_windows/metrics/metrics_*.csv`)

---

## §1 スコープと処理概要

| 項目 | 値 |
|---|---|
| 入力 CSV | 59 (元 68 中、空 9 件除外) |
| 総サンプル | **9,395 行** |
| 列数 | 18 列 (元) + 数列 (datetime, phase, source_file 等) |
| 期間 | 2026-01-03 〜 2026-02-05 (約 1 ヶ月) |
| 出力 PNG | **20 枚** |
| 出力先 | `06_evidence/metrics_analysis/figures/` |

### Phase 分類

| Phase | 期間 | サンプル数 | 比率 |
|---|---|---|---|
| Phase A (baseline) | 2026-01-03 | 587 | 6.2% |
| Phase B (改良) | 2026-01-11 〜 13 | 2,993 | 31.9% |
| Phase C (現行 8/9) | 2026-01-16 〜 17 | 4,834 | 51.5% |
| Phase 10 (PID 後) | 2026-02-04 〜 05 | 981 | 10.4% |

---

## §2 主要発見 8 つ (Key Findings 拡張版)

### F-1 🔴 PC FPS は構造的に Target に届かない (既知の追認)
PC FPS の Target 達成率: **5 fps 達成 20%、10 fps 1.5%、15 fps 0%、30 fps 0%** (ds_01 ECDF)。**制御無しでは Target にも収束しない** という performance_trends.md の Key Finding #1 を 9,395 サンプルで定量確証。

### F-2 🔴 FPS と JPEG サイズに強い負相関 — Scene complexity が直接 FPS を支配 (NEW)
**Spresense FPS vs JPEG: r = -0.78 / PC FPS vs JPEG: r = -0.72** (全期間, ds_10)。
- Phase A/B では Spresense 側 (r ≈ -0.7〜-0.8) が強く律速されるが PC は別ボトルネック (r ≈ -0.1〜-0.3)
- Phase C/10 で PC FPS も JPEG に強く連動 (r ≈ -0.7) → PID 制御で同期度向上
- **STAMP/STPA の UCA-A1.5 / UCA-CAM-AE.1 を実データで裏付け**

### F-3 🔴 PID 制御で「PC ⇄ Spresense」同期度が劇的改善 (NEW)
PC FPS vs Spresense FPS のクロス相関ピーク値 (ds_08):
- **Phase A: 0.22 (PC 1 sample 先行)** → Phase B: 0.54 (PC 3 sample 先行) → Phase C: 0.56 (同時) → **Phase 10: 0.88 (同時)**
- Phase A/B の「**PC 先行**」は **PC viewer 処理遅延が Spresense に backpressure をかける** 構造を示唆 (ADR-008 bounded(3) と整合)
- Phase 10 で同期度 0.88 = **PID 制御が PC ⇄ Spresense の挙動を強く同調させている**

### F-4 🟡 ボトルネックは Spresense capture (HW) + Scene complexity (NEW)
PC FPS 低下時 (≤P10=2.80) vs 上昇時 (≥P90=7.95) の主要指標比較 (ds_09, queue=0 アーティファクト排除後):
| 律速候補 | 低/高 ratio | 結論 |
|---|---|---|
| Spresense capture | 0.45 | 🔴 主因 (HW 制約) |
| JPEG size | 1.97 | 🔴 Scene 律速 |
| USB CDC-ACM (Serial read) | 3.30 | 🟡 二次律速 |
| WiFi/TCP | 1.49 | ✓ 律速ではない |
| PC decode | 1.21 | ✓ 律速ではない |

→ **WiFi/TCP は意外にも律速していない**。律速は **Spresense HW + Scene + USB 経路**。

### F-5 🔴 Serial read time の極端な変動 (CV 202.2%) (既知の追認)
Serial read 中央値 vs mean が桁違い (P50=51 ms vs mean=487 ms, max=34228 ms = 34 秒)。**USB CDC-ACM 経路は時折数十秒級のスパイクを起こす** (hist_03, ds_02)。**UCA-DRV-USB.1** の実証データ。

### F-6 🟡 TCP send max は常時 1 秒超で振動 — 自動再接続誤検知の根本原因 (NEW)
TCP avg 平均 160 ms vs max 平均 **1,699 ms / 最大 2,712 ms** (ts_02, hist_02)。**STAMP UCA-B1.3** (一時遅延を切断と誤判定 → 再接続発振) を実証。**M-2 (RTT 移動平均 ± 2σ 動的閾値) の必要性データ**。

### F-7 🟡 Action queue は Phase A/B では未計装 (アーティファクト発見)
**Phase A は 100%、Phase B は 94.3% が queue_depth=0**。これは「キューが空」ではなく **計装機能が未実装** だった結果。Phase C 以降で完全計装される。これにより以前の D-9 で「queue=0」と出ていたのはアーティファクト = 真の低 FPS 時は queue=4 (詰まっている) が正しい。

### F-8 🟢 Phase 10 で全体的に最も改善 — Tier 1 内で達成可能な範囲は完了
Phase 10 の特徴:
- PC FPS 平均: Phase C 比でわずかに改善 (ds_07)
- Cross-correlation 0.88 (Phase A 0.22 比 **+0.66**)
- JPEG ↔ FPS 連動 (r = -0.72) で「制御が機能している」状態
- → Tier 1 で出せる性能の頭打ちに近い。これ以上の根治には **Tier 2/3 移行** が必要 (STAMP §10.6)

---

## §3 各図の説明文 (20 枚, 1-2 行ずつ)

### 時系列 3 枚 (Phase 区切り shaded 付き)

**ts_01_fps.png — PC / Spresense FPS の推移**
PC FPS (青, 平均 4.12 / CV 64.2%) と Spresense FPS (橙, 平均 9.83 / CV 53.3%) を全期間で重ね描き。**Spresense は最大 30 fps まで出るのに PC 側は 4 fps 前後で頭打ち** という構造的ボトルネックを Phase 区切り上で視覚化。

**ts_02_tcp_send.png — TCP send time 推移 (対数軸)**
TCP 平均送信時間 (青, 平均 160 ms) と最大送信時間 (赤, 平均 1,699 ms / 最大 2.7 秒) を Phase C 基準 134 ms と並べて表示。**最大値が常時 1 秒超で振動** しており、UCA-B1.3 (自動再接続誤検知) の実証データ。

**ts_03_queue.png — Action Queue Depth 推移**
キュー深度 (緑) を PID setpoint 3.5 (赤破線) と並べる。**Phase A/B では計装機能が無く常に 0** (赤い注記で明示)、Phase C 以降で setpoint 周辺で振動。

### ヒストグラム 3 枚 (P50/P90/P95/P99 線付き)

**hist_01_fps.png — FPS 分布 + Target 達成度 + パーセンタイル**
PC / Spresense FPS の分布を Target 5/10/15/30 線とパーセンタイル線で重ね描き。**PC P50=4.0, P90=7.7** で Target 10 はほとんど達成不可。

**hist_02_tcp_send.png — TCP send time 分布 (対数軸 + パーセンタイル)**
TCP avg と max を対数軸ヒストグラム化。**avg P95 ≈ 220 ms, max P95 > 2,500 ms** で桁レベルの乖離。Phase 9.2 健全性監視で「max の急変」を予兆信号として使う設計根拠。

**hist_03_serial_read.png — Serial read time 分布 (対数軸 + パーセンタイル)**
Serial read 時間の分布。**CV 202.2%** の極端な変動 (Mean 487 ms / Median 51 ms 程度、最大 34 秒)。USB CDC-ACM 経路の不安定性を可視化 (UCA-DRV-USB.1 の実証)。

### 散布図 2 枚

**scatter_01_fps_queue.png — PC FPS vs Queue depth (色 = TCP send)**
PC FPS とキュー深度の相関、各点の色で TCP send を表現。**キュー深度が高い (右側) ほど色が明るく (TCP 遅延大)** なる傾向 = キュー滞留と TCP 遅延の正相関。

**scatter_02_tcp_jitter.png — TCP avg send vs jitter (両対数 + 回帰直線)**
横軸 TCP avg / 縦軸 jitter (= max - avg)。**両者は強い正相関、log-log slope ≈ +1.x** で「平均遅延が増えると最悪値が指数的に増加」。CTRL-A 代理指標として `send_time_jitter` を使う根拠 (M-1 §6.1.x)。

### データサイエンス系 5 枚 (D-1〜D-5)

**ds_01_ecdf.png — FPS ECDF + Target 達成率**
PC / Spresense FPS の累積分布関数。**Target 5: PC 20% 達成 / Target 10: 1.5% / Target 15-30: 0%** を 1 図で同時表示。F-1 の根拠。

**ds_02_boxplot.png — 主要 8 指標の箱ひげ (対数軸 + whisker=P5-P95)**
PC fps / Spr fps / TCP avg/max / Serial read / Decode / Queue (>0) / JPEG size を並列箱ひげ。**Serial read の外れ値が極端**、Queue は queue>0 ベースに修正済。

**ds_03_corr_heatmap.png — 16 列の Pearson 相関ヒートマップ**
Phase C/10 のみで計算 (queue=0 アーティファクト排除, n=5,815)。**FPS vs JPEG size の負相関**、**TCP avg vs jitter の正相関** などが視覚的に確認。

**ds_04a_drilldown_phaseA.png — Phase A 個別セッション drill-down**
Phase A 最長セッションの 4-panel: FPS+JPEG (第 2 軸) / TCP send / Queue+Serial / Drop イベント Δ。**Pearson r: PC vs JPEG = -0.34, Spr vs JPEG = -0.78** をタイトルに表示。queue は未計装でフラット。

**ds_04b_drilldown_phaseB.png — Phase B 個別セッション drill-down**
Phase B 最長セッション。**r: PC vs JPEG = -0.10, Spr vs JPEG = -0.72**。Phase B でも queue 未計装。

**ds_04c_drilldown_phaseC.png — Phase C 個別セッション drill-down**
Phase C 最長セッション (旧 ds_04)。**r: PC vs JPEG = -0.69, Spr vs JPEG = -0.50**。Phase C で queue 計装完了、setpoint 3.5 周辺で振動。

**ds_04d_drilldown_phase10.png — Phase 10 個別セッション drill-down**
PID 制御後の最長セッション。**r: PC vs JPEG = -0.72, Spr vs JPEG = -0.67**。queue depth が PID setpoint に近づく挙動が見える。

**ds_05_drop_timeline.png — TCP send + drop_events タイムライン**
TCP send 時系列 (上段) と drop_events/error_count の発生イベントを縦線 (下段) で重ね描き。**drop が TCP max スパイク時に集中** していることが視覚化される。

### Phase 比較 + 因果分析 4 枚 (D-7, D-8, D-9, D-10)

**ds_07_phase_compare.png — 4 Phase × 6 指標の箱ひげ並列**
Phase A/B/C/10 を各指標の箱ひげで並列比較。queue depth は Phase A/B で「N/A 未計装」表示。**PC FPS 中央値は Phase 進化で改善** (A→10 で右上がり)。

**ds_08_cross_correlation.png — PC FPS vs Spresense FPS の lag analysis (Phase 別)**
4 panel で Phase 別の cross-correlation。ピーク lag と相関係数を注記。**Phase A 0.22 → B 0.54 → C 0.56 → 10 0.88** と PID 制御で同期度が劇的に改善 (F-3)。

**ds_09_bottleneck.png — PC FPS 低下時 vs 上昇時 のボトルネック絞り込み**
Phase A/B 除外 (queue=0 排除) で n=5,815。**Spresense capture (0.45 ratio) と USB (3.30) が主因**、**TCP は不変 (1.49) で律速ではない**。F-4 の根拠。

**ds_10_fps_jpeg.png — FPS vs JPEG size 相関 (Phase 別 8 panel)**
2x4 grid (上段 PC FPS、下段 Spr FPS、列が Phase A/B/C/10)。各 panel に **回帰直線 + Pearson r + slope** を表示。全期間 PC r=-0.72 / Spr r=-0.78 の強い負相関を実証 (F-2)。

---

## §4 STAMP/STPA 分析との対応関係

実データが STAMP_STPA_ANALYSIS.md v1.7.1 の主張をどう裏付け / 修正するか:

| STAMP UCA / SC | 関連図 | 実データの裏付け |
|---|---|---|
| **UCA-A1.1** (キュー滞留時 FPS 低減指令を出さない) | ts_03, ds_04c, ds_04d | Phase C/10 では queue が setpoint 3.5 を上下振動 — 制御は機能しているが収束はしていない |
| **UCA-A1.5** (Scene 急変対応無し) (v1.5) | **ds_10, ds_04a-d, ds_09** | **r(Spr FPS, JPEG)=-0.78 / r(PC FPS, JPEG)=-0.72 で強い負相関** = Scene 影響直接実証 |
| **UCA-B1.3** (一時遅延を切断と誤判定) | ts_02, hist_02 | TCP max が常時 1 秒超で振動 — 静的閾値では誤判定不可避 |
| **UCA-CAM-AE.1** (ISX012 内蔵 AE → JPEG 膨張) (v1.5) | ds_10, ds_09 | JPEG 大 → FPS 低 の関係を全 Phase で確認 |
| **UCA-DRV-USB.1** (USB CDC-ACM 不安定) (v1.6) | hist_03, ds_02, ds_09 | Serial read CV 202%, ratio 3.3x (低 FPS 時に 3 倍遅い) で実証 |
| **SC-2** (天井を超える品質目標を出さない) | ds_07 | Phase 10 の slope -0.31 (Spr vs JPEG) は **JPEG 1 KB 増で 0.31 FPS 減** の線形関係 — 制御パラメータの根拠 |
| **SC-11** (Scene 起因 JPEG 膨張時自動低減) (v1.5) | ds_10 全 Phase | Phase 10 でも slope は依然 -0.21 (PC) = **M-24 (Scene complexity feedforward) の効果はまだ薄い**。今後の改善余地データ |
| **(Phase 10 PID 効果)** | ds_08 | Cross-correlation 0.22 → 0.88 で **PID が PC ⇄ Spresense 同期度を 4 倍向上** (M-1 改善前のベースライン) |

---

## §5 `performance_trends.md` への反映マッピング案

差し替え時の挿入位置の案。`performance_trends.md` の既存セクションに対応:

| md セクション | 挿入する図 | 説明文 |
|---|---|---|
| **§ FPS Performance Trends** | ts_01_fps, hist_01_fps, ds_01_ecdf | 時系列 + 分布 + 累積分布 (F-1) |
| **§ Network Latency Trends** | ts_02_tcp_send, hist_02_tcp_send, scatter_02_tcp_jitter | TCP avg/max + ジッタ相関 (F-6) |
| **§ Frame Processing Trends** | hist_03_serial_read, ds_02_boxplot | Serial read 変動 (F-5) |
| **§ Queue Depth Patterns** | ts_03_queue, scatter_01_fps_queue, ds_04c, ds_04d | queue 推移 (F-7 アーティファクト注記必須) |
| **§ NEW: Phase Comparison (新規セクション)** | ds_07_phase_compare, ds_08_cross_correlation, ds_04a-d | Phase 別比較 (F-3, F-8) |
| **§ NEW: Bottleneck Analysis (新規セクション)** | ds_09_bottleneck, ds_03_corr_heatmap, ds_05_drop_timeline | 律速絞り込み (F-4) |
| **§ NEW: Scene-FPS Correlation (新規セクション)** | ds_10_fps_jpeg | JPEG-FPS 関係 (F-2) |
| **§ Key Findings (既存)** | (本書 §2 で再構成した F-1〜F-8 に置換) | 全発見の俯瞰 |

---

## §6 反映前の確認ポイント (チェックリスト)

差し替え前に以下を確認:

- [ ] 図の埋め込み形式: `![alt](figures/xxx.png)` と Markdown 標準形式で
- [ ] パス整合性: `performance_trends.md` の場所 (`06_evidence/metrics_analysis/`) からの相対パス `figures/xxx.png`
- [ ] Key Findings の更新: 既存 5 個 → 新規 8 個 (F-1〜F-8) に置換 or 統合
- [ ] queue=0 アーティファクトに関する注記を追加 (F-7)
- [ ] STAMP/STPA への相互リンク (§4 のマッピングを実 link 化)
- [ ] 図ファイルを Git 管理対象に追加 (`figures/*.png`)
- [ ] visualization.py スクリプトも Git 管理 (再生成可能性確保)

---

## §7 残課題 / Open Question (差し替え時に判断必要)

1. **既存 ASCII 時系列** (`visual_evidence.md`) との重複扱い — そちらの ASCII は廃止するか保持するか
2. **Phase 10 (n=981) のサンプル不足** — もう少しデータ取得すべきか、現状で結論を出すか
3. **Phase A/B のキュー未計装** — 過去データなので変更不可。これ以降の Phase 比較で常に注記が必要
4. **Cross-correlation の解釈** — 「PC 先行」が backpressure を意味するという解釈は仮説。granger causality 等で更に検証する価値があるか

---

## 関連文書

- 親: [`performance_trends.md`](../performance_trends.md) (差し替え先)
- 兄弟: [`visual_evidence.md`](../visual_evidence.md) (ASCII 時系列、廃止候補)
- STAMP/STPA: [`../../02_specifications/quality/safety_analysis/STAMP_STPA_ANALYSIS.md`](../../../02_specifications/quality/safety_analysis/STAMP_STPA_ANALYSIS.md) v1.7.1
- スクリプト: [`../analysis_tools/visualization.py`](../analysis_tools/visualization.py)
