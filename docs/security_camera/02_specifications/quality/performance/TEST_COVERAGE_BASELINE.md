# Test Coverage Baseline (テストカバレッジ ベースライン) — X-8

**バージョン**: 1.1 (cargo-llvm-cov 実測値反映)
**作成日**: 2026-05-05
**最終更新**: 2026-05-08
**対象**: PC viewer (Rust) + Spresense アプリ (C)
**目的**: X-3 監査で判明した「テストカバレッジ 92%」未根拠問題に対するベースライン確立 + 改善計画
**位置付け**: X-8 タスク (PENDING_NFR_WORK.md), Phase 12.1 Stage 1 完了

> **背景**: `functional/TEST_COVERAGE_ENHANCEMENT_SPEC.md` v1.0 (2026-01-23) は「カバレッジ 92% → 95% (3% 向上)」と数値目標を掲げているが、**実測値の根拠が無い**ことが X-3 監査で判明 ([`FUNCTIONAL_SPEC_AUDIT.md`](../FUNCTIONAL_SPEC_AUDIT.md) §7)。本書は実測ベースの新しい起点となる。

---

## §1 PC viewer (Rust) — 現状ベースライン (実測)

### 1.1 テスト数 棚卸し (2026-05-05 計測)

`Rust_ws/security_camera_viewer` 配下:

| ファイル | `#[test]` 関数数 | 種類 |
|---|---|---|
| `src/protocol.rs` | 6 | Unit (sync_word / CRC / JPEG validation) |
| `src/motion_detector.rs` | 8 | Unit (motion / partial / no-motion / stats) |
| `src/ring_buffer.rs` | 6 | Unit (overflow / flush / usage_ratio 等) |
| `src/recording_browser.rs` | **3** (X-5c で本セッション追加) | Unit (空 / mp4 抽出 / 不在ディレクトリ) |
| `src/metrics.rs` | 2 | Unit (sequence wraparound / FPS calc) |
| `src/mp4_recorder.rs` | 2 | Integration (`#[ignore]` 付, ffmpeg 環境依存) |
| `src/tcp_connection.rs` | 2 | Unit (`test_tcp_connection` は `#[ignore]`) |
| `src/serial.rs` | 1 | Unit |
| `src/pipeline.rs` | 1 | Unit |
| `src/gui_main.rs` | 0 | (GUI は単体テスト困難) |
| `src/ui_tokens.rs` | 0 | (定数とレイアウト関数, 副作用なし) |
| `src/main.rs` | 0 | エントリポイント |

**合計 `#[test]`: 31 件** (うち `#[ignore]` ~3 件)

### 1.2 cargo test 結果 (2026-05-05)

```
$ cargo test --features gui

[gui_main] 28 passed; 0 failed; 3 ignored
[main]      7 passed; 0 failed; 0 ignored

合計: 35 passed / 0 failed / 3 ignored
```

> **注**: gui_main と main は別バイナリで、protocol/serial/etc を **両バイナリで重複ビルド・重複実行** するため、cargo test 出力上は 35 件と表示されるが、ユニーク test 関数は 31 件。

### 1.3 ファイル別 テスト密度 (簡易指標)

| ファイル | 行数 | テスト数 | 密度 (テスト/100行) |
|---|---|---|---|
| protocol.rs | 529 | 6 | 1.13 |
| motion_detector.rs | 386 | 8 | 2.07 |
| ring_buffer.rs | 271 | 6 | 2.21 |
| recording_browser.rs | 280 | 3 | 1.07 |
| metrics.rs | 317 | 2 | 0.63 |
| mp4_recorder.rs | 333 | 2 | 0.60 |
| tcp_connection.rs | 652 | 2 | 0.31 |
| serial.rs | 232 | 1 | 0.43 |
| pipeline.rs | 643 | 1 | 0.16 |
| **gui_main.rs** | 1815 | **0** | **0.00** |
| ui_tokens.rs | 280 | 0 | 0.00 |
| main.rs | 332 | 0 | 0.00 |

**観察**:
- ✅ プロトコル / 動き検出 / リングバッファ: 高密度 (1.0+) — コアロジックは適切にカバー
- 🟡 metrics / mp4_recorder / tcp_connection / serial: 密度低 — I/O 系で mock が無いため難
- 🔴 **gui_main.rs (1815 行) は test 0** — 巨大単一ファイル + GUI が test friction の主因

### 1.4 行ベースカバレッジ (cargo-llvm-cov 実測 2026-05-08)

`cargo install cargo-llvm-cov` (v0.8.5) + `rustup component add llvm-tools-preview` で導入。

**実行コマンド**:
```bash
cd /home/ken/Rust_ws/security_camera_viewer
cargo llvm-cov --features gui --summary-only
```

**実測結果 (2026-05-08, 35 test pass)**:

| ファイル | 行 % | 関数 % | 行数 | 未カバー行 |
|---|---:|---:|---:|---:|
| **motion_detector.rs** | **95.71%** | 88.89% | 210 | 9 |
| **ring_buffer.rs** | **89.12%** | 76.19% | 147 | 16 |
| protocol.rs | 36.70% | 56.25% | 297 | 188 |
| metrics.rs | 35.22% | 41.67% | 159 | 103 |
| recording_browser.rs | 34.38% | 37.50% | 192 | 126 |
| serial.rs | 8.06% | 15.38% | 124 | 114 |
| tcp_connection.rs | 3.07% | 11.11% | 326 | 316 |
| pipeline.rs | 1.46% | 6.67% | 343 | 338 |
| **gui_main.rs** | **0.00%** | 0.00% | 1151 | 1151 |
| **mp4_recorder.rs** | **0.00%** | 0.00% | 225 | 225 |
| **ui_tokens.rs** | **0.00%** | 0.00% | 155 | 155 |
| main.rs | 0.00% | 0.00% | 181 | 181 |
| **TOTAL** | **16.75%** | **30.91%** | **3510** | **2922** |

**2026-05-08 更新測定** (ui_tokens 単体テスト 11 件追加後):

| ファイル | 行 % (Δ) | 関数 % | 行数 | 未カバー行 |
|---|---:|---:|---:|---:|
| ui_tokens.rs | **44.14%** (+44.14pt) | 83.33% | 256 | 143 |
| (他は変更なし) | — | — | — | — |
| **TOTAL** | **19.41%** (+2.66pt) | **35.62%** (+4.71pt) | **3611** | **2910** |

**重要発見**:

1. **TEST_COVERAGE_ENHANCEMENT_SPEC v1.0 の「92%」主張は完全に未根拠** — 実測 16.75% で 75pt 以上の乖離 (X-3 監査が指摘した通り)
2. **コアロジック (motion / ring_buffer) は 90%+ で健全** — 既存テストの質は悪くない
3. **GUI/IO 系 (gui_main 1151 行 / mp4_recorder 225 行 / ui_tokens 155 行) は 0%**
4. **大きいファイルほどカバレッジが低い** — gui_main 32.8% / 全行に占める寄与
5. **ネットワーク系 (tcp_connection / pipeline / serial) も低カバレッジ** (3-8%) — `#[ignore]` 試験を活用しきれていない

**カバレッジを 1pt 上げる効率**:

| 対象 | 1pt 上昇に必要なテスト追加行 | コスト感 |
|---|---:|---|
| gui_main.rs | ~35 行 | 高 (GUI tests は friction 大) |
| mp4_recorder.rs | ~35 行 | 中 (ffmpeg mock 整備) |
| tcp_connection.rs | ~35 行 | 中 (network mock) |
| ui_tokens.rs | ~35 行 | **低** (純粋関数 / 定数) |
| pipeline.rs | ~35 行 | 中 (3 thread 動作 test) |

**ROI 順 (低コスト先)**: ui_tokens (paint_hud 単体テスト) → pipeline (mock channel) → mp4_recorder (file system mock) → tcp_connection (TcpListener mock)。

---

## §2 Spresense アプリ (C) — 現状ベースライン

### 2.1 テスト棚卸し (2026-05-05 計測)

`apps/examples/security_camera/` 配下:

```bash
$ grep -rln "#ifdef CONFIG_DEBUG\|UNIT_TEST\|TEST_" apps/examples/security_camera/*.c
(該当なし)
```

**合計テスト関数: 0 件**

### 2.2 構造的制約

NuttX RTOS 上の組込み C コードは以下の理由で**通常の cargo test 相当の実行が困難**:

1. **NuttX 内蔵の unit test framework が無い** (LightWeight な assert_* マクロのみ)
2. ハードウェア依存 (V4L2 / SPI / GS2200M) が多く、host 上での mock 構築コスト大
3. ビルドが `make` ベースで CI 統合が薄い (ESP-IDF ほど整っていない)

### 2.3 改善方針 (Phase 12 候補)

| 方法 | 効果 | コスト |
|---|---|---|
| **A. Pure-logic 抽出 + host build** | mjpeg_protocol / fps_controller / frame_statistics 等の純粋ロジックを `lib*.a` 化し、PC 側で gtest テスト | 中 (CMake 整備が必要) |
| **B. NuttX 上の `examples/test_*` 追加** | NuttX シェルから手動実行する test app を作成 | 小 (機能限定) |
| **C. テスト無しで進める** | 結合テストのみで品質を担保 (現状) | 0 |

**推奨**: A. Pure-logic 抽出が長期ROI高だが、現セッション外のスコープ。

---

## §3 既存仕様書との整合 (X-3 監査連動)

`functional/TEST_COVERAGE_ENHANCEMENT_SPEC.md` v1.0 の **「92% → 95%」記述は本書 §1〜§2 の実測**と乖離:

| 項目 | TEST_COVERAGE_ENHANCEMENT_SPEC v1.0 主張 | 本書ベースライン |
|---|---|---|
| Spresense 側カバレッジ | 92% (前提) | **0% 直接測定** (テスト関数 0 件) |
| PC 側カバレッジ | 92% (前提) | 行ベース未計測 / テスト関数 31 件 / 35 pass |
| 改善目標 | 95% | **§4 で実測ベースに置換** |

→ Phase 12 で TEST_COVERAGE_ENHANCEMENT_SPEC.md を本書ベースに改訂する必要あり (X-3 監査の延長作業)。

---

## §4 改善計画 (Phase 12 以降)

### Stage 1: 計測ツール導入 (本セッション後)

- [ ] `cargo install cargo-llvm-cov` を CI/開発環境に導入
- [ ] `.github/workflows/ci.yml` に coverage job を統合 (既存 codecov ステップを置換)
- [ ] **本書 §1.4 を実測値で更新**

### Stage 2: PC viewer 重点改善

| 優先 | 対象 | 目標 | 理由 |
|---|---|---|---|
| 1 | gui_main.rs を 5+ サブモジュールに分割 | 各モジュールに 30%+ test カバレッジ | 1815 行の単一テスト 0 はリスク |
| 2 | tcp_connection.rs / pipeline.rs に integration test | 50% line coverage | 既に `#[ignore]` 試験あり、network mock の整備で活用 |
| 3 | serial.rs / metrics.rs | 70% | mock シリアル port で達成可 |

### Stage 3: Spresense アプリ Pure-Logic 抽出

| 対象 | 切出方針 |
|---|---|
| `mjpeg_protocol.c` (CRC / pack / validate) | I/O 依存なし → そのまま host build OK |
| `fps_controller.c` (PID 計算) | 同上 |
| `frame_statistics.c` (移動窓統計) | 同上 |
| `frame_queue.c` | mutex を std::mutex 互換に置換 |

各モジュールで:
- gtest 単体テスト (>= 5 ケース / モジュール)
- 行カバレッジ 80%+ 目標
- CI で host build + test 実行

### Stage 4: 数値目標 (2026-05-08 実測ベースで再設定)

**現状**: PC viewer 行カバレッジ = **16.75%** (実測, motion / ring_buffer の 90%+ が支える)

| 領域 | 1 ヶ月後 (Phase 12 内) | 3 ヶ月後 (Phase 13 検討時) |
|---|---|---|
| PC viewer 行カバレッジ全体 | **25%** (現 16.75% から +8pt — ROI 高い箇所優先) | **40%** (gui_main 部分テスト + GUI 抽出) |
| ui_tokens.rs | 60% (paint_hud 関数を切出してテスト可能化) | 80% |
| mp4_recorder.rs | 30% (RecordingPolicy / ローテーションロジックの単体テスト) | 50% |
| tcp_connection.rs | 30% (TcpListener mock 整備) | 50% |
| Spresense Pure-Logic 行カバレッジ | (Stage 3 着手判断 — Phase 13 候補) | 60% (mjpeg_protocol 抽出後) |
| CI 上の自動測定 | ✅ 有効化済 + 失敗時に PR block | weekly トレンドレポート |

**改訂方針**: 旧目標 (1ヶ月で 30% / 3ヶ月で 50%) は実測前の予測値。実測 16.75% から見ると **8pt 上昇 (25%)** が現実的。Stage 2 の gui_main 分割を待たずに ui_tokens 単体テスト追加で +5pt 程度は短期実現可能。

---

## §5 関連文書

- 整合性監査 (X-3): [`FUNCTIONAL_SPEC_AUDIT.md`](../FUNCTIONAL_SPEC_AUDIT.md) §7
- 旧仕様 (要改訂): [`../functional/TEST_COVERAGE_ENHANCEMENT_SPEC.md`](../functional/TEST_COVERAGE_ENHANCEMENT_SPEC.md)
- 品質要求 §7.5 (試験性): [`QUALITY_REQUIREMENTS.md`](../QUALITY_REQUIREMENTS.md)
- 残課題: [`PENDING_NFR_WORK.md`](../PENDING_NFR_WORK.md) X-8
- CI 設定: `Rust_ws/security_camera_viewer/.github/workflows/ci.yml`

---

## 改訂履歴

| バージョン | 日付 | 変更内容 |
|---|---|---|
| 1.0 | 2026-05-05 | 初版。X-8 ベースライン確立。PC 側 31 test (35 pass / 0 fail / 3 ignore) を計測、密度指標を導入、ツール導入 (Stage 1) → モジュール分割 (Stage 2) → Spresense Pure-Logic 抽出 (Stage 3) の 3 段改善計画を策定 |
| 1.1 | 2026-05-08 | **Phase 12.1 Stage 1 完了**: cargo-llvm-cov v0.8.5 導入 + 実測。総合行カバレッジ **16.75%** (旧主張 92% から 75pt の乖離確認)。motion 95.71% / ring_buffer 89.12% が支える、gui_main / mp4_recorder / ui_tokens は 0%。Stage 4 数値目標を実測ベースに改訂 (1ヶ月 25% / 3ヶ月 40%) |
| 1.2 | 2026-05-08 | **C: ui_tokens 単体テスト追加** で総合 **16.75% → 19.41%** (+2.66pt)。ui_tokens.rs **0.00% → 44.14%** (大幅向上, テスト 11 件追加)。1ヶ月目標 25% への進捗 33% (8pt 中 2.66pt 進行) |
