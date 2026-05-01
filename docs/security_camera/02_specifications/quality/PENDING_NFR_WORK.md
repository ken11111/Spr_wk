# Pending NFR / Quality Work — 未着手タスク台帳

**作成日**: 2026-05-01
**目的**: 本セッション (P0 ドキュメント整備計画) の中で**判明したが未着手**の NFR 関連タスクを durable に記録し、context 圧縮や session 跨ぎでの記憶喪失を防ぐ
**位置付け**: P0 完了後の P1 / P2 / 計画外タスクの全集合

> **既存台帳との棲み分け**:
> - 個別の技術負債は [`../../05_future_actions/technical_debt/TECHNICAL_DEBT_REGISTER.md`](../../05_future_actions/technical_debt/TECHNICAL_DEBT_REGISTER.md) で管理 (12 件登録済)
> - 本台帳は **品質要求 (NFR) 視点** で本セッションで判明したものを集約。重複は cross-reference で繋ぐ

---

## P1 タスク (アーキテクト推奨, 高優先)

### P1-A. CPU/帯域予算表の追記

**目的**: SPRESENSE_TCP_CONSTRAINTS.md は **メモリ予算**は完備だが、**CPU 利用率予算** と **帯域利用率** が欠落している。性能議論の精度向上のため。

**作業内容**:
- Cortex-M4F × 6 コアの利用状況 (どのスレッドが何コアで何 % か)
  - main task / camera_thread / usb_thread / control_thread / gs2200m driver task の CPU 利用率実測
  - 計測手法: NuttX `top` 相当機能 or perf_logger 経由でサンプリング
- SPI 4 MHz 利用率 (理論 500 KB/s に対する実効 % )
- WiFi 帯域実効値 (802.11n 理論 vs GS2200M 実効)
- USB CDC-ACM 帯域 (12 Mbps 上限に対する実効 KB/s)
- レイテンシ予算分解 (ISX012 capture → V4L2 dequeue → MJPEG pack → tx_buff → SPI → WiFi → PC decode → display)

**追記先**: `SPRESENSE_TCP_CONSTRAINTS.md` §2.X (新設) または §16

**関連**: QAS-2, QAS-10, QUALITY_REQUIREMENTS.md §2.2 で「⚪ 未文書化」と記載した箇所

---

### P1-B. Cross-cutting Concerns 集約文書 (arc42 §8)

**目的**: ロギング / エラー処理 / 設定管理の横断方針が散在している (config.h / wifi_config.h / mjpeg_protocol.h に分散, syslog 利用方針がアプリ全体で統一されていない)。

**作業内容**:
- ロギング方針: syslog レベル使い分け, ログ集約, ローテーション
- エラー処理方針: errno 取り扱い, retry 戦略, 失敗時のリソース解放
- 設定管理方針: config.h / wifi_config.h / mjpeg_protocol.h の集約候補
- 国際化方針: 現状日本語ログのみ → 英語化検討

**新規作成先**: `02_specifications/quality/CROSS_CUTTING_CONCERNS.md` (arc42 §8 専用)

**関連**: QUALITY_REQUIREMENTS.md §7.4 (修正性), §1 (設定の集約度)

---

### P1-C. セキュリティ仕様の「設計済」と「実装済」分離

**目的**: SECURITY_GAP_ANALYSIS.md で乖離を開示済だが、より積極的な対応として SECURITY_ARCHITECTURE.md / SECURITY_SPEC.md 自体を「設計提案」と「実装済」に**ファイル分割**することで誤読を防ぐ。

**作業内容**:
- `SECURITY_ARCHITECTURE.md` → `SECURITY_DESIGN_PROPOSAL.md` にリネーム検討 (本書は 100% 提案)
- 実装済セキュリティ要素 (CRC-16, syslog 監査) は `SECURITY_IMPLEMENTED.md` (新設) に分離記述
- functional/SECURITY_SPEC.md も同様に整理

**判断**: SECURITY_GAP_ANALYSIS.md で透明性が確保できているなら不要かもしれない。Phase 12 のセキュリティ判断 (Option A〜D) と連動して検討。

**関連**: SECURITY_GAP_ANALYSIS.md §5 Option A〜D

---

## P2 タスク (中優先)

### P2-A. Use Case / Scenarios 文書 (4+1 view §5)

**目的**: 「誰が・何のために・どう使うか」が文書化されていない。4+1 view の Scenarios が完全欠落。

**作業内容**:
- アクター定義 (運用者 / 設置者 / 不正侵入者 / 監視者)
- 主要ユースケース 5-7 件 (起動 / 録画 / 動き検出時録画 / 切断復旧 / 録画再生 / 設定変更 / 障害時対応)
- 各 UC をシナリオ形式で記述

**新規作成先**: `02_specifications/use_cases/` ディレクトリ新設 + 個別 UC ファイル

**関連**: 要求書 v1.0 Q6 (録画トリガー), Q15 (通知), Q19 (エラー回復)

---

### P2-B. FMEA テーブル (失敗モード影響解析)

**目的**: 個別障害は ADR-002 / health state machine 等に断片的に存在するが、**系統的な FMEA** がない。信頼性 NFR の根拠化。

**作業内容**:
- 障害モード 20-30 件抽出 (構造的天井 #1〜#5 + ソフト失敗 + ハード失敗 + 運用失敗)
- 各モードについて: 影響 (Severity) × 発生確率 (Occurrence) × 検知容易性 (Detection) = RPN 計算
- 高 RPN モードへの対策と現状実装の対応

**新規作成先**: `02_specifications/quality/FMEA.md`

**関連**: QAS-1, QAS-8, ADR-002 v1.1, gs2200m_health_state_machine.puml

---

### P2-C. 詳細 Threat Model (STRIDE / DREAD)

**目的**: SECURITY_GAP_ANALYSIS.md に簡易 STRIDE があるが、各脅威について DREAD 等の数値評価が未実施。

**作業内容**:
- 各 STRIDE カテゴリで脅威シナリオ 3-5 件詳述
- DREAD (Damage / Reproducibility / Exploitability / Affected users / Discoverability) で 1-10 採点
- 軽減策の優先順位付け

**新規作成先**: `02_specifications/quality/THREAT_MODEL.md` または SECURITY_GAP_ANALYSIS.md §3 を拡充

**関連**: SECURITY_GAP_ANALYSIS.md, Phase 12 セキュリティ判断

---

## 計画外タスク (本セッション中に派生, 記録のみ)

### X-1. RTM v5.0 改訂

**理由**: 要求書 v0.1 → v1.0 確定により、RTM (要求トレーサビリティマトリクス) v4.0 の前提が変わった。各 Q の確定値と仕様/実装/テストの紐付けを再構築する必要。

**対象**: `02_specifications/traceability/REQUIREMENTS_TRACEABILITY_MATRIX.md` (現 v4.0, 800 行)

**規模**: 中〜大 (要求 25 件 × 仕様 × 実装 × テストのマトリクス更新)

---

### X-2. MASTER_ROADMAP_2026 v2.0 改訂

**理由**: 本セッションで Phase 10/11 定義の競合を「制御工学系を正規」と確定した (GLOSSARY §1)。`MASTER_ROADMAP_2026.md` の Phase 10 (AI統合・マルチカメラ) / Phase 11 (プラットフォーム化) は **Phase 番号再割り当て**が必要。

**対象**: `05_future_actions/master_roadmap/MASTER_ROADMAP_2026.md`

**規模**: 大 (ロードマップ全体の再構成)

**注意**: GLOSSARY.md §1 で「将来 v2.0 改訂で再割り当て予定」と既に予告済

---

### X-3. 個別 functional/ 仕様書 6 件の整合性チェック

**理由**: 要求書 v1.0 確定により、`functional/` 配下 6 件 (CAMERA_CAPTURE / STREAMING / RECORDING / ADAPTIVE_CONTROL / CONTROL_ENGINEERING / SECURITY) との整合性が変化している可能性。

**対象**: `02_specifications/functional/*.md` 6 件

**規模**: 中 (各仕様書を要求書 v1.0 に対して読み合わせ)

---

### X-4. 運用ランブック (Operations Runbook)

**理由**: ADR-002 v1.1 の発見 (再接続 5 回失敗で FAILED 固着) に対応する**人手介入手順**が未文書化。QAS-8 で「未定義」と明記済。

**作業内容**:
- Spresense 再起動手順
- GS2200M リセット手順
- Tier C (USB-only) 切替手順
- 連絡先・エスカレーション体制

**新規作成先**: `07_operations/RUNBOOK.md` (新設ディレクトリ)

**関連**: QAS-8, ADR-002 v1.1, 構造的天井 #1

---

### X-5. 機能仕様の未実装項目への対応

要求書 v1.0 §9 で「⚪ 未達成 / 未検証」と確定したもの:

| ID | 項目 | 関連 Q | 規模 |
|---|---|---|---|
| X-5a | ストレージ ローテーション (1GB 上限到達時の古いファイル削除) | Q8 | 中 |
| X-5b | 録画ファイルの時間分割 (1 時間ごとなど) | Q9 | 小〜中 |
| X-5c | アプリ内再生 UI (egui で MP4 再生) | Q11 | 中 |
| X-5d | OSD 重畳 (映像にタイムスタンプ書き込み) | Q14 | 中 |
| X-5e | Windows ネイティブビルド検証 | Q21 | 小 |
| X-5f | 屋外 / 温度範囲 / 24h 連続稼働ストレステスト | Q25 | 大 |

**関連**: TECHNICAL_DEBT_REGISTER.md と一部重複の可能性 → 統合管理候補

---

### X-6. CPU 予算の実測手段の確立

**理由**: P1-A の前提として、Cortex-M4F × 6 の CPU 利用率を実測する手段が未確立。

**作業内容**:
- NuttX の `top` 相当機能の有効化 (`CONFIG_FS_PROCFS` + `CONFIG_FS_PROCFS_REGISTER`)
- perf_logger 拡張で per-thread CPU time 計測
- 計測結果を SPRESENSE_TCP_CONSTRAINTS に追記

**規模**: 中 (実装 + 計測ループ)

---

## 取り組み優先度の提案

### 即時着手候補 (Phase 12 序盤)
- **P1-A** CPU/帯域予算 — 数値根拠が定量化されると Tier 移行判断の精度向上
- **X-4** 運用ランブック — 現状の運用ギャップを最小コストで埋める
- **X-1** RTM v5.0 — 要求書 v1.0 確定の論理的続編

### 中期 (Phase 12〜13)
- **P1-B** Cross-cutting Concerns
- **P2-A** Use Case / Scenarios
- **X-3** functional/ 仕様書整合性チェック

### Phase 12 セキュリティ判断と連動
- **P1-C** セキュリティ仕様分離
- **P2-C** 詳細 Threat Model (STRIDE/DREAD)

### Phase 13 以降または Tier 移行と同時
- **P2-B** FMEA
- **X-2** MASTER_ROADMAP_2026 v2.0
- **X-5** 個別未実装項目 (a〜f)
- **X-6** CPU 予算実測手段

---

## 関連ドキュメント

- 完了済: [`README.md`](README.md), [`GLOSSARY.md`](GLOSSARY.md), [`QUALITY_REQUIREMENTS.md`](QUALITY_REQUIREMENTS.md), [`QUALITY_ATTRIBUTE_SCENARIOS.md`](QUALITY_ATTRIBUTE_SCENARIOS.md), [`SECURITY_GAP_ANALYSIS.md`](SECURITY_GAP_ANALYSIS.md)
- 既存台帳: [`../../05_future_actions/technical_debt/TECHNICAL_DEBT_REGISTER.md`](../../05_future_actions/technical_debt/TECHNICAL_DEBT_REGISTER.md) (12 件登録済)
- 要求書: [`../../01_requirements/FUNCTIONAL_REQUIREMENTS.md`](../../01_requirements/FUNCTIONAL_REQUIREMENTS.md) v1.0 §10 「次のステップ」

---

## 改訂履歴

| バージョン | 日付 | 変更内容 |
|---|---|---|
| 1.0 | 2026-05-01 | 初版。本セッション (P0 ドキュメント整備) で判明した P1 (3 件) / P2 (3 件) / 計画外 (6 件) を集約 |
