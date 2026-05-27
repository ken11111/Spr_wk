# Functional SPEC Audit — 機能仕様書 整合性監査

**バージョン**: 1.0
**作成日**: 2026-05-03
**目的**: `02_specifications/functional/` 配下の 7 SPEC ファイルが要求書 v1.0 と実装事実に対してどの程度整合しているかを監査し、必要に応じて警告バナーを追加する
**位置付け**: X-3 タスク (PENDING_NFR_WORK.md)

> **基準**:
> - 要求書 v1.0 (`01_requirements/FUNCTIONAL_REQUIREMENTS.md` Q1-Q25 確定値)
> - 実装事実 (本セッションで検証済)
> - GLOSSARY §1 Phase 番号定義 (制御工学系)
> - ADR-002 v1.1 (再接続が逆効果の発見)

---

## 監査対象ファイルと結果サマリ

| ファイル | バージョン | 日付 | 整合性 | 推奨アクション |
|---|---|---|---|---|
| ADAPTIVE_CONTROL_SPEC.md | (なし) | (なし) | 🔴 重大乖離 | 警告バナー追加 (Phase 11 .c 未実装) |
| CAMERA_CAPTURE_SPEC.md | 3.0 | 2026-01-23 | 🟡 部分乖離 | 軽微注記 (FPS 期待値) |
| CONTROL_ENGINEERING_SPEC.md | 1.1 | 2026-02-04 | 🟡 Phase 番号誤参照 | 注記 (Phase 11 = AI ではない) |
| RECORDING_SPEC.md | 3.0 | 2026-01-23 | 🟡 未実装機能多数 | 注記 (時間分割/ローテーション/アプリ内再生 未実装) |
| SECURITY_SPEC.md | 1.0 | 2026-01-23 | 🔴 設計提案のみ | 警告バナー追加 (SECURITY_GAP_ANALYSIS 参照) |
| STREAMING_SPEC.md | 4.0 | 2026-02-03 | 🟡 Phase 11 機能誤主張 | 注記 (適応バッファ/インテリジェント破棄は未実装) |
| TEST_COVERAGE_ENHANCEMENT_SPEC.md | 1.0 | 2026-01-23 | 🔴 数値未根拠 | 注記 (92% カバレッジは未根拠) |

---

## §1. ADAPTIVE_CONTROL_SPEC.md (🔴 重大乖離)

**問題**:
- Phase 11 多変数適応制御システムを「実装する」(現在進行形) として記述
- 実態: `enhanced_control.h` に API 宣言のみ、`.c` ファイル不存在、caller 0 件
- **読者は「実装済みの機能」と誤読する重大リスク**

**推奨アクション**:
- 冒頭に警告バナー追加: 「本書は仕様策定段階の設計提案であり、実装は未着手」
- L2.C 図 (`spresense_main_board_l2c_control.puml`) と SECURITY_GAP_ANALYSIS.md と同じ透明性方針

**関連 Phase 12 タスク**:
- Phase 11 .c 実装の判断 (FMEA B8 RPN 225)

---

## §2. CAMERA_CAPTURE_SPEC.md (🟡 部分乖離)

**問題**:
- v3.0 (2026-01-23) — 要求書 v1.0 (2026-05-01) より前に作成
- "Phase 9.2 TCP健全性監視対応" は概ね一致
- ただし **Q16 (100ms Want)**, **Q19 (再接続が逆効果)** の発見が反映されていない

**主張内容の整合性確認**:
- ✅ ISX012 / V4L2 RING / JPEG 直接出力は実装と一致
- 🟡 「適応的キャプチャ制御」の表現が Phase 11 機能 (未実装) と混同される余地

**推奨アクション**: 軽微注記 (要求書 v1.0 の Q16/Q19 へのリンク追加)

---

## §3. CONTROL_ENGINEERING_SPEC.md (🟡 Phase 番号誤参照)

**問題**:
- 「**Phase 11 AI統合への基盤**」と記述されているが、これは旧 Phase 11 (プラットフォーム化 → MASTER_ROADMAP v2.0 で Phase 14 に再割り当て) の名残
- 現 Phase 11 = 適応制御拡張 (AI 統合ではない)
- AI 統合は **Phase 13** (旧 Phase 10)

**推奨アクション**: Phase 番号注記追加 + GLOSSARY §1 への参照

---

## §4. RECORDING_SPEC.md (🟡 未実装機能多数)

**問題**:
- v3.0 (2026-01-23) — 要求書 v1.0 (2026-05-01) より前
- Q1.0 で確定された「未実装」項目 (X-5a〜d) が反映されていない:
  - Q8 自動ローテーション 未実装
  - Q9 時間分割 未実装
  - Q11 アプリ内再生 未実装
  - Q14 OSD 重畳 未実装

**推奨アクション**: 注記追加 (UC-3/UC-4 と要求書 v1.0 §9 「⚪ 未達成」へのリンク)

---

## §5. SECURITY_SPEC.md (🔴 設計提案のみ・実装乖離)

**問題**:
- SECURITY_ARCHITECTURE.md と並んで **完全な設計提案** (TLS 1.3, WPA2-PSK 詳細仕様, JWT 等)
- 実装は WPA2-PSK のみ (TLS / JWT / 暗号化 / 認証なし)
- THREAT_MODEL DREAD 上位 5 件のうち 3 件 (TI-1/TS-1/TI-2) を本書の設計が想定する機構で緩和可能だが**未実装**
- SECURITY_ARCHITECTURE.md には警告バナー追加済 (P0 完了時)、本書にも同様のバナー必要

**推奨アクション**: 警告バナー追加 (SECURITY_GAP_ANALYSIS.md / THREAT_MODEL.md 参照)

---

## §6. STREAMING_SPEC.md (🟡 Phase 11 機能誤主張)

**問題**:
- v4.0 (2026-02-03) — 要求書 v1.0 より前
- 「PID制御による動的最適化」: ✅ Phase 10 で実装済 (`fps_controller.c`)
- 「**適応的バッファ管理**」: 🔴 Phase 11 未実装 (`enhanced_control.h` の `buffer_manager_t` だけ存在)
- 「**インテリジェントフレーム破棄**」: 🔴 同上 (Phase 11 未実装)
- 「Phase 10 制御工学統合実装」表記は誤りではないが、Phase 11 機能を「実装」として読み取れる箇所あり

**推奨アクション**: 注記追加 (Phase 11 部分は仕様策定段階)

---

## §7. TEST_COVERAGE_ENHANCEMENT_SPEC.md (🔴 数値未根拠)

**問題**:
- 「テストカバレッジ 92% → 95% (3%向上)」と数値主張
- 現状 RTM v5.0 §A 監査結果: テストカバレッジは **未計測** (QUALITY_REQUIREMENTS.md §7.5 で ⚪ 未計測 と判定済)
- 「Phase 5 品質向上」は文書整備の Phase であり、実テスト整備ではない

**推奨アクション**: 注記追加 (92% 数値は未根拠であり、実測データなし)

---

## 監査結果による派生タスク (Phase 12 候補)

| 派生タスク | 影響 SPEC | 既存 PENDING との関連 |
|---|---|---|
| Phase 11 .c 実装 or 仕様削除判断 | ADAPTIVE_CONTROL, STREAMING, CONTROL_ENGINEERING | FMEA B8 |
| RECORDING SPEC を要求書 v1.0 に整合 | RECORDING | X-5a/b/c/d |
| セキュリティ仕様の段階的実装 (Option A〜D 判断) | SECURITY | Phase 12 セキュリティ判断 |
| テストカバレッジ実測 + ベースライン確立 | TEST_COVERAGE | (新規, X-8 候補) |

---

## 総合評価

| 観点 | 状態 |
|---|---|
| 機能仕様書全体としての整合性 | 🟡 個別 SPEC は古いものが多い (主に 2026-01〜02 作成) |
| 主要乖離の透明化 | 🔴 → 🟡 (本監査 + 警告バナー追加で改善) |
| Phase 12 への引継ぎ | ✅ 派生タスクとして整理 |

**結論**: 各 SPEC を全面改訂せず、**警告バナー + 関連文書への cross-link 追加** で透明性を確保する方針が現実的。Phase 11 機能 (適応制御 / バッファ管理) と セキュリティ機能 (TLS/JWT) は **設計提案** として扱い、実装は別タスクで判断する。

---

## 関連文書

- 上位要求: [`../../01_requirements/FUNCTIONAL_REQUIREMENTS.md`](../../01_requirements/FUNCTIONAL_REQUIREMENTS.md) v1.0
- 用語集 (Phase 番号定義): [`GLOSSARY.md`](GLOSSARY.md) §1
- セキュリティ乖離: [`SECURITY_GAP_ANALYSIS.md`](risk_analysis/SECURITY_GAP_ANALYSIS.md)
- 脅威モデル: [`THREAT_MODEL.md`](risk_analysis/THREAT_MODEL.md)
- 失敗モード台帳: [`FMEA.md`](risk_analysis/FMEA.md)
- 残タスク: [`PENDING_NFR_WORK.md`](PENDING_NFR_WORK.md)
- ユースケース: [`../use_cases/primary_use_cases.md`](../use_cases/primary_use_cases.md)
- RTM v5.0: [`../traceability/REQUIREMENTS_TRACEABILITY_MATRIX.md`](../traceability/REQUIREMENTS_TRACEABILITY_MATRIX.md)

---

## 改訂履歴

| バージョン | 日付 | 変更内容 |
|---|---|---|
| 1.0 | 2026-05-03 | 初版。functional/ 配下 7 SPEC を要求書 v1.0 と実装事実に対して監査、推奨アクション (警告バナー追加) を整理 |
