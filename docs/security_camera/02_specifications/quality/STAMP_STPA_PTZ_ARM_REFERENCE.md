# STAMP/STPA 参考: SO-ARM101 PTZ 統合 (PoC 前 draft)

**バージョン**: 0.1 (PoC 着手前 draft)
**作成日**: 2026-05-26
**ステータス**: 🟡 **参考記録のみ — 本書 [`STAMP_STPA_ANALYSIS.md`](STAMP_STPA_ANALYSIS.md) v1.11 への統合は PoC 結果次第で判断**
**目的**: Hugging Face LeRobot SO-ARM101 Pro をカメラ pan/tilt 機構として既存防犯カメラに統合する場合の安全分析を **PoC 着手前** に draft として記録。物理可動部追加に伴う新規 Loss / Hazard / UCA / 対策を整理し、PoC で検証すべき項目を明示する。

---

## 📋 エグゼクティブサマリ

### 本書の位置付け

| 項目 | 内容 |
|---|---|
| **対象** | 既存 Spresense 防犯カメラ + SO-ARM101 Pro (6 DoF アーム) の **PTZ (pan/tilt/zoom) 統合構想** |
| **想定構成** | (a) 案: SO-ARM101 先端に Spresense カメラを搭載、被写体追尾のため pan/tilt 動作 |
| **ステータス** | **PoC 着手前** — 実機検証で実現性を確認してから本書 [`STAMP_STPA_ANALYSIS.md`](STAMP_STPA_ANALYSIS.md) への統合可否を判断 |
| **本書を読む対象者** | PTZ 統合 PoC を実施する開発者 / 安全分析担当 |
| **本書の制約** | **机上検討のみ** — 実機データ無しで設計したため、PoC 結果で見直し必須 |

### 本書の最大の論点 3 つ

1. **物理可動部追加で安全領域が大幅に拡大** — 既存の電気・通信障害ベースの分析から、**機械的衝突 / 過負荷 / 緊急停止** といった機械安全領域に踏み込む
2. **遅延ループの新規発生** — capture → PC → サーボ動作で **300-500 ms 遅延** = 高速追従不能。STAMP UCA-A1.3 (制御遅延) の物理版が出現
3. **既存 STAMP §10.6 構造的天井との連動** — RAM 1.5MB / SPI 帯域は変わらないため、**追尾制御を PC 側に置くしか選択肢がない** (案 A 必然)

### PoC 後の判断フローチャート

```
[PoC 実施]
   │
   ├─ 遅延 < 200 ms 達成 ?         No  →  追尾アルゴリズム再設計 (LP filter / Kalman) 必要
   │     │ Yes
   │     ▼
   ├─ 配線ストレス確認 OK ?         No  →  WiFi-only 構成へ
   │     │ Yes
   │     ▼
   ├─ 視覚フィードバック収束 ?      No  →  PoC 失敗 → 構成見直し
   │     │ Yes
   │     ▼
   └─ 物理衝突リスク許容範囲 ?      No  →  領域制限ハード化 (リミットスイッチ等)
         │ Yes
         ▼
   本書 STAMP_STPA_ANALYSIS.md v2.0 として本格統合
```

---

## §1 想定システム構成 (案 A: PC 経由 PTZ 型)

```
[ 撮影対象 (人/物/動体) ]
        │
        ▼ (動き → 検知)
┌─ Spresense + カメラ ─────┐
│ (SO-ARM101 末端搭載)      │ ──WiFi/8888──┐
└──────────────────────────┘                │
        ▲                                    ▼
        │ サーボ指令                  ┌─ PC viewer ─┐
        │                              │ motion_     │
        │                              │ detector +  │
        │                              │ 追尾制御    │
        │                              │ (新規)      │
        │                              └──────┬──────┘
        │                                     │
[ SO-ARM101 Pro (6 DoF) ] ◄──USB-RS485────────┘
                                  (LeRobot SDK + Feetech Bus)
```

### 構成変更点 (既存防犯カメラから)

| 既存 (v1.11) | 追加 (PTZ 統合時) |
|---|---|
| Spresense + カメラ (固定) | + **SO-ARM101 末端搭載** |
| PC viewer = MJPEG 受信 + 録画 | + **LeRobot SDK + 追尾制御ループ** |
| 通信: WiFi/USB-CDC | + **USB-RS485 (PC ↔ SO-ARM101)** |
| 制御 Controller (Spresense 内 PID) | + **PC 側 CTRL-ARM (新規)** |
| ペイロード | + Spresense + カメラ + 配線 ~80g (SO-ARM101 末端容量内) |

---

## §2 新規 Losses / Hazards / Safety Constraints

### 2.1 新規 Losses (本書 §1.2 拡張案)

| ID | 損失 | 主要利害 |
|---|---|---|
| **L-8** (PTZ 統合時) | アーム動作による人・物への物理衝突 / 機材損傷 | 安全・資産 |
| **L-9** (PTZ 統合時) | 追尾失敗で防犯目的の対象を見失う (本来の防犯機能の本質) | 運用 |

### 2.2 新規 Hazards (本書 §1.3 拡張案)

| ID | ハザード | 関連 Loss |
|---|---|---|
| **H-19** | アームが想定外領域 (人の作業空間, 機材等) に進入する状態 | L-8 |
| **H-20** | 緊急停止 (E-Stop) が機能しない or 機能までの時間が長すぎる状態 | L-8 |
| **H-21** | 視覚フィードバックループが発振する状態 (Visual Servoing 不安定) | L-1 (映像), L-9 |
| **H-22** | カメラ配線・電源が機械動作で断線/接触不良になっている状態 | L-1, L-8 |

### 2.3 新規 System Safety Constraints (本書 §1.4 拡張案)

| ID | 安全制約 | 対応 H |
|---|---|---|
| **SC-18** | アーム動作可能領域を電子的に制限 (角度範囲・速度上限) + 越境時即停止 | H-19 |
| **SC-19** | E-Stop 押下から 100 ms 以内に全サーボ電源遮断 | H-20 |
| **SC-20** | 追尾制御ループは LP filter / Kalman で発振抑制、収束時間 ≤ 1 秒 | H-21 |
| **SC-21** | 末端配線は **WiFi 通信中心 + 電源のみ有線**、機械動作で断線しない引き回し | H-22 |

---

## §3 新規 Controller と UCA

### 3.1 新規 Controller — CTRL-ARM (Arm Trajectory Controller, PC 側)

| 項目 | 内容 |
|---|---|
| **配置** | PC viewer (Python LeRobot SDK ベース) |
| **入力** | motion_detector からの bounding box (cx, cy), 視野角情報 |
| **出力** | サーボ角度指令 (pan, tilt) — LeRobot SDK 経由で USB-RS485 → Feetech サーボ |
| **制御周期** | 10 Hz (PC 側で同期、画像 fps と整合) |
| **Process Model** | 「画像中心 ↔ bbox 中心の偏差を最小化すれば追尾成功」「サーボ応答は線形」 |
| **Process Model 盲点** | 機械的バックラッシ、サーボ過渡応答、視野ズレ (キャリブ誤差)、被写体ロスト |

### 3.2 新規 UCA — CTRL-ARM (CA-ARM.1〜.3)

> 既存本書 §3 と同フォーマット。NP/P/TL/SL の 4 タイプで分類。

| UCA ID | タイプ | 不適切な制御アクション | 結果ハザード |
|---|---|---|---|
| **UCA-ARM.1** | NP | 衝突検知 (リミットスイッチ等) 信号を受けても停止指令を出さない | H-19, **L-8** |
| **UCA-ARM.2** | TL | PC ↔ アーム通信遅延 300-500 ms で軌道追従できない (visual servoing の遅延) | H-21, L-9 |
| **UCA-ARM.3** | NP | 追尾対象ロスト時の挙動 (再探索 / 停止 / 待機) が未定義 | H-21, L-9 |
| **UCA-ARM.4** | P | 物理可動範囲外の角度指令でサーボ過負荷 (Feetech サーボの保護機能に依存) | H-19, L-8 |
| **UCA-ARM.5** | SL | 発振状態 (Visual Servoing 振動) が検知されず長期継続 | H-21, L-1 (映像揺れ) |

---

## §4 新規対策 M-41 / M-42

| ID | 対策 | 対応 UCA / Hazard | 工数 (目安) | 担当 | Phase |
|---|---|---|---|---|---|
| **M-41** | アーム動作領域の電子的制限 (Soft Limit) + 衝突検知 (電流監視 or リミットスイッチ) + E-Stop (ハード + ソフト両方) | UCA-ARM.1, .4, H-19, H-20 | 5-10 人日 | PC + HW 設計 | Phase X.3-X.4 |
| **M-42** | Visual Servoing PID + LP filter / Kalman で発振抑制、収束時間 SLA 1 秒 | UCA-ARM.2, .5, H-21 | 5-10 人日 | PC | Phase X.4 |
| **M-43** | 配線レイアウト設計 (WiFi-only データ + 電源のみ有線, ケーブルガイド) | H-22 | 2-3 人日 (HW 設計) | HW | Phase X.2 |
| **M-44** | 追尾対象ロスト時の挙動規定 (10 秒ロスト → 中央位置に戻る + 警告 metrics) | UCA-ARM.3 | 1-2 人日 | PC | Phase X.4 |

---

## §5 既存 STAMP/STPA との連動

### 5.1 既存対策との関係

| 既存対策 | PTZ 統合での扱い |
|---|---|
| **M-1** (PID 多変数化) | 既存 Spresense 内 PID は **変更不要** (FPS 制御のみ)。アーム追尾は別の制御ループ (CTRL-ARM, PC 側) |
| **M-7 / M-35c** (副経路 / E2E trace) | アーム動作も metrics に追加 (pan/tilt 角度, 衝突検知状態) |
| **M-25** (motion FP/FN コーパス) | **PTZ 統合で重要性大幅増** — motion_detector の FN が追尾失敗 (L-9) に直結 |
| **M-34** (画像サイズ上限制御) | 追尾アルゴリズムの応答性確保のため帯域余裕を維持する観点で **重要度上昇** |
| **M-36** (Sub-Core オフロード) | 追尾制御は PC 側のため Sub-Core オフロードとは独立 |

### 5.2 既存 Hazard との関係

- **H-1 (データパス停止)**: アーム使用時に WiFi 切断すると追尾不能 → SC-1 (3 秒回復) と整合
- **H-3 (再接続発振)**: 追尾中の WiFi 切断 → アームが停止位置で固まる、または最後の指令で動き続けるリスク
- **L-3 (完全性侵害)**: 追尾中の映像が証拠として使用される場合、M-39 (OSD 重畳, pan/tilt 角度も焼き込み) との連動が必要

---

## §6 PoC で検証すべき項目チェックリスト

PoC 着手前にこの項目を確認することで、本書 STAMP_STPA_ANALYSIS.md への統合可否を判断する。

### 6.1 物理層 (Phase X.2 で確認)

- [ ] Spresense + カメラ + 配線 の総重量 (測定値) と SO-ARM101 末端ペイロード許容値の余裕
- [ ] 配線レイアウトで 6 DoF 全動作範囲を試走、捻れ/断線リスクの有無
- [ ] 電源供給 (USB バスパワー or バッテリ) の電圧降下確認
- [ ] WiFi 通信距離 (アーム動作で位置が変わる) の安定性

### 6.2 制御層 (Phase X.3-X.4 で確認)

- [ ] **遅延実測**: capture → motion_detection → サーボ指令 → 物理移動完了 までの E2E 遅延 (目標 < 200 ms)
- [ ] 視覚フィードバックループの発振有無 (PID Kp/Ki/Kd 初期値の探索)
- [ ] 追尾アルゴリズムの収束時間 (静止対象に対し中心捕捉まで何秒)
- [ ] 移動対象 (歩行速度 1-2 m/s 想定) への追従可能性

### 6.3 安全層 (Phase X.4-X.5 で確認)

- [ ] E-Stop の応答時間 (押下 → 全サーボ電源遮断 ≤ 100 ms)
- [ ] Soft Limit 越境時のサーボ動作 (即停止 / クランプ / エラー)
- [ ] 衝突検知 (電流監視) の感度と誤検知率
- [ ] 緊急時のアーム姿勢 (危険な姿勢で停止しないか)

### 6.4 統合層 (Phase X.5 で確認)

- [ ] 既存防犯カメラ機能 (motion_detector, 録画, ストリーミング) が追尾統合後も動作
- [ ] アーム動作時の Spresense CPU / WiFi 負荷増加 (既存 70-100% 推定との影響)
- [ ] M-39 (OSD) と統合: pan/tilt 角度を映像に焼き込み

---

## §7 PoC 結果に基づく本書統合の判断基準

PoC 完了後、以下の基準で **STAMP_STPA_ANALYSIS.md v2.0 への統合判断** を行う。

| 判断軸 | 統合 OK の条件 | 統合 NG なら... |
|---|---|---|
| **遅延** | E2E < 200 ms | 構成見直し (例: edge AI で追尾を Spresense で完結) |
| **発振** | LP filter / Kalman で収束時間 < 1 秒 | アルゴリズム再設計 |
| **配線安定性** | 24 時間連続動作で断線・接触不良 0 | WiFi-only + ワイヤレス電源 |
| **物理安全** | E-Stop 100 ms 達成、Soft Limit 越境 0 | リミットスイッチ等のハード安全機構追加 |
| **既存機能** | motion_detector / 録画 / ストリーミング 維持 | 統合スコープ縮小 |

統合する場合の本書への影響範囲:
- §1: L-8/L-9 を Loss に追加、H-19〜H-22 を Hazard に追加、SC-18〜21 を SC に追加
- §2: 制御構造図に CTRL-ARM 追加 (新規詳細図 stamp_control_structure_ptz_arm.puml)
- §3: §3.7.12 として CTRL-ARM の UCA セクション追加
- §6: §6.5h として M-41〜M-44 を対策追加
- §10: §10.10 として「Safety-critical 領域への拡張に伴う本書の方法論的限界」を追記

---

## §8 関連文書

- 本書統合先候補: [`STAMP_STPA_ANALYSIS.md`](STAMP_STPA_ANALYSIS.md) v1.11
- 構造的天井: [`STAMP_STPA_ANALYSIS.md §10.6`](STAMP_STPA_ANALYSIS.md#106-構造的天井と-tier-移行の戦略) (RAM 1.5MB が PTZ にも影響)
- 要求書: [`../../01_requirements/FUNCTIONAL_REQUIREMENTS.md`](../../01_requirements/FUNCTIONAL_REQUIREMENTS.md) Q12 (動き検出), Q13 (複数カメラ前提)
- 要求トレース: [`REQUIREMENTS_TRACEABILITY.md`](REQUIREMENTS_TRACEABILITY.md)
- 外部参照: Hugging Face LeRobot SDK (Feetech サーボ制御 + 模倣学習フレームワーク)

## 改訂履歴

| Version | Date | 変更内容 |
|---|---|---|
| 0.1 | 2026-05-26 | 初版 (PoC 前 draft) — L-8/9, H-19〜22, SC-18〜21, CTRL-ARM, UCA-ARM.1〜5, M-41〜44 を案として整理 |
