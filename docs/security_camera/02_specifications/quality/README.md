# Quality Requirements & Glossary

**目的**: arc42 §10 (Quality Requirements) + §12 (Glossary) に対応する集約ディレクトリ。
ISO/IEC 25010 品質属性体系で散在 NFR を整理し、文書間の用語統一を提供する。

**設立日**: 2026-05-01
**親プロジェクト**: Spresense + PC Security Camera (Phase 1〜11 系)

---

## 配下ファイル

| ファイル | 役割 | arc42 章 |
|---|---|---|
| [`GLOSSARY.md`](GLOSSARY.md) | Phase 定義 + 構造的天井 + 制御工学・通信用語 ~35 項目 | §12 |
| [`QUALITY_REQUIREMENTS.md`](QUALITY_REQUIREMENTS.md) | ISO/IEC 25010 8 品質属性別 NFR 集約 | §10 |
| [`QUALITY_ATTRIBUTE_SCENARIOS.md`](QUALITY_ATTRIBUTE_SCENARIOS.md) | arc42 標準形式の QAS 10 件 | §10 |
| [`SECURITY_GAP_ANALYSIS.md`](SECURITY_GAP_ANALYSIS.md) | `SECURITY_ARCHITECTURE.md` (設計) と実装の乖離開示 | §10/§11 |
| [`CPU_BANDWIDTH_BUDGET.md`](CPU_BANDWIDTH_BUDGET.md) | CPU 予算 (単一コア共有) + 帯域予算 (SPI/WiFi/USB) + レイテンシ分解 | §10 |
| [`CROSS_CUTTING_CONCERNS.md`](CROSS_CUTTING_CONCERNS.md) | ロギング / エラー処理 / 設定管理 / 国際化 の横断方針 (IS → TO-BE) | §8 |
| [`FMEA.md`](FMEA.md) | 28 失敗モードの S/O/D/RPN 採点 + 対策状況 | §11 |
| [`THREAT_MODEL.md`](THREAT_MODEL.md) | STRIDE × DREAD 14 脅威シナリオ + Phase 12 軽減策優先度 | §10 |
| [`FUNCTIONAL_SPEC_AUDIT.md`](FUNCTIONAL_SPEC_AUDIT.md) | functional/ 7 SPEC の整合性監査 + 推奨アクション | §11 |
| [`TEST_COVERAGE_BASELINE.md`](TEST_COVERAGE_BASELINE.md) | テスト棚卸し (PC 31 件 / Spresense 0 件) + 改善計画 | §11 |
| [`PENDING_NFR_WORK.md`](PENDING_NFR_WORK.md) | 未着手 NFR タスク台帳 (P1 / P2 / 計画外, 全 12 件) | — |

---

## 親文書からのナビゲーション

- 機能要求側: [`../../01_requirements/FUNCTIONAL_REQUIREMENTS.md`](../../01_requirements/FUNCTIONAL_REQUIREMENTS.md) v1.0
- アーキテクチャ側: [`../architecture/SYSTEM_ARCHITECTURE.md`](../architecture/SYSTEM_ARCHITECTURE.md)
- 制約側: [`../architecture/SPRESENSE_TCP_CONSTRAINTS.md`](../architecture/SPRESENSE_TCP_CONSTRAINTS.md)
- 決定側: [`../../03_achievements/architecture_decisions/`](../../03_achievements/architecture_decisions/) (ADR 8 件)
- 計画側: [`../../05_future_actions/`](../../05_future_actions/) (Roadmap / Tech Debt)

---

## 設計原則

1. **既存資料を集約・引用** — 新規記述は最小化、ADR / Phase 文書 / TCP_CONSTRAINTS をハブとして再利用
2. **目標 vs 実測** — 各 NFR で「設計値」と「実測値」を併記し、達成/未達/設計のみを区別
3. **設計 vs 実装の透明化** — セキュリティ等の乖離は専用ファイル (`SECURITY_GAP_ANALYSIS.md`) で開示
4. **用語の一義性** — Phase 番号や構造的天井等の意味揺れを `GLOSSARY.md` で正規化
