# Operations (運用ドキュメント)

**目的**: システム運用に関する手順・ランブック・チェックリストを集約。アーキテクチャ/仕様 (`02_specifications/`) や決定 (`03_achievements/`) とは別軸の **運用知識** を担当する。

**作成日**: 2026-05-02

---

## 配下ファイル

| ファイル | 役割 |
|---|---|
| [`RUNBOOK.md`](RUNBOOK.md) | 障害症状の識別 → トリアージ → 復旧手順 → エスカレーションまでの運用ランブック |

---

## 親文書からのナビゲーション

- 異常系シナリオ: [`../02_specifications/use_cases/exception_scenarios.md`](../02_specifications/use_cases/exception_scenarios.md) (ES-1〜5)
- 障害モード: [`../02_specifications/quality/FMEA.md`](../02_specifications/quality/FMEA.md) (28 失敗モード)
- 脅威モデル: [`../02_specifications/quality/THREAT_MODEL.md`](../02_specifications/quality/THREAT_MODEL.md)
- アクター: [`../02_specifications/use_cases/actors.md`](../02_specifications/use_cases/actors.md) (保守者)
- ユースケース: [`../02_specifications/use_cases/primary_use_cases.md`](../02_specifications/use_cases/primary_use_cases.md) (UC-7 障害時の人手介入)

---

## 適用対象

本書は **Phase 11 時点の現実装** に基づく。Tier 移行 (Tier 2/3/C) 後は本書の手順も再評価が必要。
