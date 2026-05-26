# Spresense HDR カメラ防犯カメラシステム — ドキュメント INDEX

**プロジェクト**: Spresense + Sony HDR カメラ + PC viewer (Rust) による防犯カメラ
**ステータス**: Phase 10 (制御工学統合) 実装中 / Phase 12 PoC 検討中 (PTZ アーム統合)
**最終更新**: 2026-05-26

---

## 📋 5 分で全体像を把握するには

1. **要求**: [`01_requirements/FUNCTIONAL_REQUIREMENTS.md`](01_requirements/FUNCTIONAL_REQUIREMENTS.md) v1.1 (Q1〜Q25)
2. **アーキ**: [`02_specifications/architecture/SYSTEM_ARCHITECTURE.md`](02_specifications/architecture/SYSTEM_ARCHITECTURE.md)
3. **品質分析**: [`02_specifications/quality/README.md`](02_specifications/quality/README.md) (FMEA / THREAT_MODEL / **STAMP/STPA v1.11**)
4. **実データ**: [`06_evidence/metrics_analysis/performance_trends.md`](06_evidence/metrics_analysis/performance_trends.md) v1.4 (20 図 + 8 主要発見)
5. **ロードマップ**: [`05_future_actions/phase_planned/PHASE_PLANNED_ROADMAP.md`](05_future_actions/phase_planned/PHASE_PLANNED_ROADMAP.md)

---

## 📂 ディレクトリ構成 (arc42 ベース)

| ディレクトリ | 役割 | 主要ファイル |
|---|---|---|
| [`01_requirements/`](01_requirements/) | 要求書・要件 | `FUNCTIONAL_REQUIREMENTS.md` v1.1 (Q1〜Q25 確定済) |
| [`02_specifications/`](02_specifications/) | 仕様書群 (最大規模) | `architecture/` `quality/` `functional/` `interface/` `traceability/` `use_cases/` `diagrams/` |
| [`03_achievements/`](03_achievements/) | 実装成果 | `architecture_decisions/` (ADR 8 件) `performance_results/` `phase_deliverables/` |
| [`04_issues_challenges/`](04_issues_challenges/) | 課題分析 | `CRITICAL_ISSUES.md` `LESSONS_LEARNED.md` `PHASE8_BUFFER_QUEUE_*.md` |
| [`05_future_actions/`](05_future_actions/) | 計画・ロードマップ | `phase_planned/` `master_roadmap/` `technical_debt/` |
| [`06_evidence/`](06_evidence/) | 実証データ・図表 | `metrics_analysis/` (実データ可視化 22 図) `diagrams/` |
| [`07_operations/`](07_operations/) | 運用ドキュメント | `RUNBOOK.md` `STRESS_TEST_PLAN.md` 等 |

---

## 🎯 役割別 入口ガイド

### アーキテクト
- [`02_specifications/architecture/`](02_specifications/architecture/) — システム/Spresense/Security アーキ + L1/L2 図
- [`02_specifications/quality/STAMP_STPA_ANALYSIS.md`](02_specifications/quality/STAMP_STPA_ANALYSIS.md) — 制御構造起点の安全分析 (v1.11)
- [`03_achievements/architecture_decisions/`](03_achievements/architecture_decisions/) — ADR 8 件

### 開発者
- [`02_specifications/functional/`](02_specifications/functional/) — 機能仕様 7 件
- [`02_specifications/interface/`](02_specifications/interface/) — IF 仕様 13 件
- [`02_specifications/quality/STAMP_STPA_TEST_PLAN.md`](02_specifications/quality/STAMP_STPA_TEST_PLAN.md) — UCA 再現テスト計画
- [`07_operations/CPU_MEASUREMENT_GUIDE.md`](07_operations/CPU_MEASUREMENT_GUIDE.md) — CPU 計測手順

### PdM / 計画担当
- [`05_future_actions/phase_planned/PHASE_PLANNED_ROADMAP.md`](05_future_actions/phase_planned/PHASE_PLANNED_ROADMAP.md) — Phase 3, 6, 4, 10, 11, **12 (PTZ PoC)**
- [`02_specifications/quality/REQUIREMENTS_TRACEABILITY.md`](02_specifications/quality/REQUIREMENTS_TRACEABILITY.md) — Q × M × Phase 3 軸マトリクス
- [`05_future_actions/technical_debt/`](05_future_actions/technical_debt/) — 技術負債レジスタ

### テスト担当
- [`02_specifications/quality/STAMP_STPA_TEST_PLAN.md`](02_specifications/quality/STAMP_STPA_TEST_PLAN.md) — UCA 再現 + Pentest + KPI
- [`02_specifications/quality/TEST_COVERAGE_BASELINE.md`](02_specifications/quality/TEST_COVERAGE_BASELINE.md) — テストカバレッジ
- [`07_operations/STRESS_TEST_PLAN.md`](07_operations/STRESS_TEST_PLAN.md) — ストレステスト

### データサイエンス / 性能評価
- [`06_evidence/metrics_analysis/performance_trends.md`](06_evidence/metrics_analysis/performance_trends.md) v1.4
- [`06_evidence/metrics_analysis/figures/`](06_evidence/metrics_analysis/figures/) — 22 PNG (時系列 / ヒスト / 散布 / DS 系)
- [`06_evidence/metrics_analysis/analysis_tools/visualization.py`](06_evidence/metrics_analysis/analysis_tools/visualization.py) — 再生成スクリプト

### セキュリティ担当
- [`02_specifications/quality/THREAT_MODEL.md`](02_specifications/quality/THREAT_MODEL.md) — STRIDE × DREAD 14 件
- [`02_specifications/quality/SECURITY_GAP_ANALYSIS.md`](02_specifications/quality/SECURITY_GAP_ANALYSIS.md) — 設計 vs 実装乖離
- [`02_specifications/quality/STAMP_STPA_ANALYSIS.md`](02_specifications/quality/STAMP_STPA_ANALYSIS.md) §8 — STPA-Sec

---

## 🔗 関連リポジトリ / 外部ワークスペース

| 場所 | 内容 |
|---|---|
| `/home/ken/Spr_ws/GH_wk_test/` (本リポジトリ) | docs/ + Spresense アプリ + spresense submodule |
| `/home/ken/Spr_ws/GH_wk_test/spresense/` (submodule, フォーク版) | Spresense SDK + NuttX, 本リポジトリでは管理対象外 (`.gitignore`) |
| `/home/ken/Rust_ws/security_camera_viewer/` (別ワークスペース) | PC viewer (Rust) 実装 |
| (将来) LeRobot SO-ARM101 統合 | Phase 12 PoC で配置先検討中 → [`05_future_actions/phase_planned/PTZ_ARM_POC_PLAN.md`](05_future_actions/phase_planned/PTZ_ARM_POC_PLAN.md) |

---

## 📜 主要バージョン履歴 (主要文書)

| 文書 | 最新 ver | 主要進化 |
|---|---|---|
| FUNCTIONAL_REQUIREMENTS | v1.1 | Tier 1 + 家庭用 で恒久化 (2026-05-05) |
| STAMP_STPA_ANALYSIS | v1.11 | UCA 67+ / SC 17 / 対策 M-1〜M-40 / 図 6 枚 (2026-05-26) |
| performance_trends | v1.4 | 実データ 9,395 サンプル + 20 図 + 8 主要発見 |
| REQUIREMENTS_TRACEABILITY | v1.0 | Q × M × Phase 3 軸マトリクス (新規) |

---

## 🔧 ドキュメント運用

- arc42 ベース階層構造 (`MIGRATION_PLAN` で 2026-01 に確立, [`03_achievements/phase_deliverables/MIGRATION_PLAN.md`](03_achievements/phase_deliverables/MIGRATION_PLAN.md))
- **可視化図再生成**: `cd 06_evidence/metrics_analysis/analysis_tools/ && .venv/bin/python visualization.py`
- **PlantUML SVG 再生成**: `cd 02_specifications/quality/ && docker run --rm -v "$PWD:/work" -w /work plantuml/plantuml:latest -tsvg *.puml`
- Minto Pyramid 原則準拠で主要分析文書を構成 (各文書冒頭にエグゼクティブサマリ配置)

---

## 📝 改訂履歴

| Date | 変更内容 |
|---|---|
| 2026-05-26 | 初版作成 — 最上位ナビゲーション + 役割別入口ガイド |
