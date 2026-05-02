# Use Cases (ユースケース) — 4+1 View Scenarios + arc42 §5

**目的**: 「誰が・何のために・どう使うか」を文書化し、要求 / 機能仕様 / 品質要求を**ユースケース**という第 4 軸で結び付ける。

**作成日**: 2026-05-02
**親プロジェクト**: Spresense + PC Security Camera (Phase 1〜11)
**準拠**: 4+1 View Scenarios + arc42 §5 + Cockburn UC フォーマット

---

## 配下ファイル

| ファイル | 役割 |
|---|---|
| [`actors.md`](actors.md) | 6 アクター定義 (運用者 / 設置者 / 保守者 / 不正侵入者 / Spresense / PC viewer) |
| [`use_case_overview.puml`](use_case_overview.puml) (+ `.png`) | UML use case diagram (全 UC + アクター関係を 1 枚で俯瞰) |
| [`primary_use_cases.md`](primary_use_cases.md) | 主要 UC 7 件 (UC-1〜7) — 起動 / ストリーミング / 動き検出録画 / ファイル管理 / 切断復旧 / 設定変更 / 障害時介入 |
| [`exception_scenarios.md`](exception_scenarios.md) | 5 異常系シナリオ (構造的天井 #1 / WiFi 切断 / USB 切断 / PC クラッシュ / 設定誤り) |

---

## 親文書からのナビゲーション

- 上位要求: [`../../01_requirements/FUNCTIONAL_REQUIREMENTS.md`](../../01_requirements/FUNCTIONAL_REQUIREMENTS.md) v1.0 (Q1-Q25 確定値)
- 機能仕様: [`../functional/`](../functional/) (6 SPEC: CAMERA_CAPTURE / STREAMING / RECORDING / ADAPTIVE_CONTROL / CONTROL_ENGINEERING / SECURITY)
- 品質要求: [`../quality/QUALITY_REQUIREMENTS.md`](../quality/QUALITY_REQUIREMENTS.md) (ISO/IEC 25010)
- 品質シナリオ: [`../quality/QUALITY_ATTRIBUTE_SCENARIOS.md`](../quality/QUALITY_ATTRIBUTE_SCENARIOS.md) (QAS-1〜10)
- アーキテクチャ: [`../architecture/SYSTEM_ARCHITECTURE.md`](../architecture/SYSTEM_ARCHITECTURE.md)
- 用語集: [`../quality/GLOSSARY.md`](../quality/GLOSSARY.md)

---

## ドキュメント階層との関係

```
要求書 v1.0 (Q1-Q25)
    ↓
[本ディレクトリ: ユースケース]    ← UC-N が要求と機能仕様を結ぶ
    ↓
機能仕様 (functional/)
    ↓
アーキテクチャ (architecture/)
    ↓
実装 (apps/, Rust_ws/)
    ↑
品質要求 (quality/) — UC の応答測定基準を提供
```

---

## 設計原則

1. **既存資料を参照、新規記述は最小** — 各 UC は要求 Q番号 / ADR / QAS / 実装ファイルへのポインタを持つ (詳細はそちらに委譲)
2. **目標 vs 実測** — UC の Response Measure は QAS から引用、達成/未達/設計のみを区別
3. **ハッピーパスと例外を分離** — 主要 UC は成功シナリオ中心、異常系は別ファイルで横断的に整理
4. **Cockburn フォーマット (一部簡略化)** — 主アクター / 前提 / 主シナリオ / 例外 / 関連 (要求/ADR/QAS/実装) の 5 軸固定
