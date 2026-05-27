# 環境ストレステスト計画 (X-5f)

**バージョン**: 1.0
**作成日**: 2026-05-04
**対象**: 要求書 v1.0 Q25 で「⚪ 未検証」と確定した屋外 / 温度範囲 / 24h 連続稼働
**位置付け**: X-5f タスク (PENDING_NFR_WORK.md), FMEA C7 (RPN 270) の対策準備

> **本書は計画書であり、実機実行は Phase 12 以降**。実行時は本書を Operations Runbook (RUNBOOK.md §3, §4) と組み合わせて使う。

---

## §1 試験対象範囲 (Q25 確定値より)

| 項目 | 計画値 | 根拠 |
|---|---|---|
| 設置場所 | 屋内 + 屋外 | Q25 |
| 照明条件 | 明 / 暗 / 変動大 | Q25 |
| 動作時間 | 24h 連続 (3 日間) | Q25 |
| 温度範囲 | -5°C 〜 +50°C | Q25 |
| 湿度 | (要求未定義) — 一般家庭環境 30-80% RH を想定 |
| 振動 | (要求未定義) — 静置のみ |

---

## §2 試験項目マトリクス

| 試験 ID | 名称 | 目的 | 期間 | 必要資源 |
|---|---|---|---|---|
| **ST-1** | 屋内常温 24h 連続 | ベースライン (Q25 must) | 24h | 屋内設置 + 電源 + WiFi |
| **ST-2** | 屋外設置 8h | 環境ノイズ + 照度変動 | 8h | 屋外設置 + 防雨 |
| **ST-3** | 高温環境 (+50°C) 8h | 上限温度耐性 | 8h | 恒温槽 or 高温室 |
| **ST-4** | 低温環境 (-5°C) 8h | 下限温度耐性 | 8h | 恒温槽 or 冷蔵庫 |
| **ST-5** | 温度サイクル (4 hours -5↔+50) | 結露・熱応力 | 24h | 恒温槽 |
| **ST-6** | WiFi 強度劣化テスト | 構造的天井 #1 顕在化 | 4h | RSSI -70 dBm 環境 |
| **ST-7** | 連続録画 24h × 3 日 (72h) | ストレージ ローテーション (X-5a) | 72h | 1GB 録画ローテーション環境 |
| **ST-8** | バッファ枯渇シナリオ | FMEA A1, B3 検証 | 1h | iperf3 で WiFi 帯域競合 |

優先度:
- **P0**: ST-1, ST-7 (実用性の最低限基準)
- **P1**: ST-2, ST-3, ST-4 (Q25 必須項目)
- **P2**: ST-5, ST-6, ST-8 (構造的天井検証)

---

## §3 計測指標 (各試験で取得)

### A. 連続性指標
- ✅ アップタイム (Spresense / PC viewer 双方)
- ✅ 強制再起動回数
- ✅ FAILED 状態移行回数 (RUNBOOK §4.3)

### B. 性能指標
- 平均 FPS (Spresense 側 + PC 側)
- ドロップ率 (action_queue overflow + JPEG validation error)
- TCP 平均/最大 send 時間 (`g_tcp_health` metrics)
- TCP Health 状態遷移ログ (HEALTHY → ... → FAILED)

### C. CPU / メモリ指標
- 各スレッド CPU% (X-6 perf_thread_cpu の output)
- メモリ使用率 (NuttX top の監視)

### D. 環境指標
- 周囲温度 (1 分粒度サンプリング、外付けセンサ推奨)
- 湿度 (同上)
- WiFi RSSI (PC 側 `iwconfig wlan0`)

### E. 画質指標 (代表サンプル)
- 1 時間ごとに 5 frame 抽出して目視チェック
- 動き検出 false positive / false negative

---

## §4 試験ごとの手順テンプレート

```
0. RUNBOOK §0 事前確認
1. 環境準備 (温度/湿度/WiFi 設定)
2. ロギング開始 (シリアル + PC viewer metrics CSV)
3. ストリーム開始 (UC-1 → UC-2)
4. 試験本体 (試験 ID 別)
5. ログ取得 (試験中ずっと)
6. 試験終了 → ファイル収集
7. 解析 (parse_cpu_log.py / metrics CSV / grep [CPU])
8. 結果記録 (ST_RESULTS.md 作成)
```

---

## §5 試験別 詳細手順

### ST-1: 屋内常温 24h 連続

```
前提: 室温 20-25°C, WiFi RSSI ≥ -65 dBm
1. PC + Spresense を屋内に設置、AP を 1m 圏内
2. PC viewer を CLI で起動 (GUI 過負荷を避ける)
3. 録画モードを「動き検出」に設定
4. 24h 放置
5. 終了後:
   - syslog からアップタイム確認
   - metrics CSV を analyze_metrics.py で集計
   - 録画ファイルの最新時刻が直近であることを確認

合格基準:
- 強制再起動 0 回 (Spresense / PC とも)
- 平均 FPS ≥ 5.0 (Phase 8 ベースの 75% 以上)
- ドロップ率 ≤ 80% (Phase 9 実測 74.2% を悪化させない)
- FAILED 状態固着 0 回
```

### ST-2: 屋外設置 8h

```
前提: 軒下や雨が当たらない場所、外気温 0-30°C
1. 防雨カバー (簡易タッパー or 専用ケース) で Spresense を保護
2. WiFi が屋外まで届く位置を確認 (RSSI ≥ -70 dBm)
3. ST-1 と同じ手順
4. 8h 後にデータ回収 + 機器外観確認

合格基準: ST-1 と同じ + 機器の物理損傷なし

異常時 (一例):
- 結露で USB 接続不能 → RUNBOOK §4.1 に従い再起動
- 太陽光直射で温度上昇 → ST-3 高温シナリオに移行
```

### ST-3 / ST-4: 温度範囲試験

```
前提: 恒温槽 (or 簡易: 夏場ベランダ +50°C / 冷蔵庫 +5°C, -5°C は冷凍庫)

1. 設定温度に環境を 30 分かけて到達
2. Spresense に電源投入 (Cold start)
3. ストリーム開始 → 8h 連続稼働
4. 1 時間ごとに環境温度・WiFi RSSI を記録
5. 終了後、室温に戻して動作確認 (温度変化に対する復元性)

合格基準: ST-1 と同じ + 温度範囲超過時の異常記録のみ許容

警告: -5°C は CXD5602 の動作温度範囲下限に近い (要データシート確認)
      +50°C は GS2200M の上限近く (要モジュール仕様確認)
```

### ST-5: 温度サイクル

```
+50 → -5 → +50 → -5 を 4 サイクル (24h)
- 結露の発生リスク高 → 防湿剤併用推奨
- 熱応力で半田クラックの可能性 (一般的な品質試験項目)
```

### ST-6: WiFi 強度劣化

```
1. Spresense と AP の距離を意図的に伸ばす (実際の防犯カメラ運用に近い)
2. RSSI -70 〜 -85 dBm 範囲で 4h 動作
3. CRITICAL → RECONNECTING → FAILED 状態遷移を観察

期待される結果: ADR-002 v1.1 で予想された通り、再接続が逆効果に。
              FAILED 固着が頻発する場合は Tier 移行判断の根拠を強化。
```

### ST-7: 録画 ローテーション (X-5a 連動)

```
1. RECORDING_DIR を 1GB 上限で設定
2. 連続録画モード (動き検出常時 ON)
3. 72h 連続稼働
4. 録画ファイル数の遷移を 1h ごとに記録

合格基準:
- 1GB 上限を超過する書き込み 0 回 (RecordingManager が古いファイルを削除)
- 最新ファイルが常に書き込み中 (停止しない)
- ファイル数が安定状態に達する (= ローテーション機能している証拠)

実装根拠: Rust_ws c737f3b の RecordingPolicy + RecordingManager
```

### ST-8: バッファ枯渇シナリオ

```
1. 別 PC から iperf3 -c <AP_IP> で WiFi 帯域競合を発生させる
2. 1h 連続して TCP send 時間が >1000ms になる頻度を計測
3. Auto-Reconnect の動作を観察

期待される結果: FMEA B3 (RPN 300) の挙動を再現
            → タイミング図 T-4 の予想と一致するか検証
```

---

## §6 結果記録テンプレート

各試験完了後に作成:

```
# Stress Test Result: ST-N (YYYY-MM-DD)

## 試験条件
- 試験 ID:
- 開始/終了:
- 環境 (温度/湿度/RSSI):
- ハードウェア構成: Spresense + GS2200M + USB CDC

## 計測結果
- アップタイム:
- 平均 FPS:
- ドロップ率:
- TCP send avg/max:
- FAILED 状態回数:

## 異常事象
- (発生したら時刻 + 内容)

## 反映先
- CPU_BANDWIDTH_BUDGET §2 / §4 (実測値)
- QUALITY_REQUIREMENTS §5 (信頼性 MTBF)
- FMEA C7 (RPN 270) の対策完了マーク

## 判定
- 合格 / 部分合格 / 不合格
- 不合格時の Tier 移行判断推奨度
```

---

## §7 必要資源リスト

### 機材
- Spresense ボード × 1 (予備推奨 1)
- GS2200M モジュール
- USB CDC-ACM ケーブル
- 屋外用簡易ケース (ST-2)
- 恒温槽 or 環境シミュレータ (ST-3/4/5) — 個人レベルなら**冷蔵庫/冷凍庫/車内夏場で代替可**
- 温湿度ロガー (1 分粒度) — 100 円ショップでも可
- 別 PC (iperf3 用, ST-6/8)

### ソフト
- `parse_cpu_log.py` (X-6, 既存)
- `metrics analyze script` (新規 — Phase 12 で必要なら作成, 現状は手動 CSV)

### 推定所要時間
- 全試験 (ST-1〜8) 累計: 約 145h (うち多くは並行実行可)
- 結果まとめ: 各試験 +2h
- 総工数: 約 1 人月 (準備含む)

---

## §8 リスクと対策

| リスク | 影響 | 対策 |
|---|---|---|
| Spresense 物理損傷 (ST-3/4/5) | 機材損失 | 予備機材 + メーカ動作温度範囲を厳守 |
| 録画データ消失 | 試験結果の欠損 | リアルタイムで PC viewer に転送 (録画は補助) |
| 試験中の電源断 | 試験無効化 | UPS or バッテリ駆動 (ST-1 の長時間試験では必須級) |
| 自宅環境制約 (特に屋外) | 試験不可 | レンタル試験室 / 大学・職場の設備 |
| ハードウェア限界突破 | 修理不能 | ST-3/4/5 で各 -5/+50 の境界を**段階的に**接近 |

---

## §9 関連文書

- 要求書: [`../01_requirements/FUNCTIONAL_REQUIREMENTS.md`](../01_requirements/FUNCTIONAL_REQUIREMENTS.md) Q25
- 失敗モード: [`../02_specifications/quality/risk_analysis/FMEA.md`](../02_specifications/quality/risk_analysis/FMEA.md) C7 (RPN 270)
- 運用ランブック: [`RUNBOOK.md`](RUNBOOK.md)
- CPU 計測ガイド: [`CPU_MEASUREMENT_GUIDE.md`](CPU_MEASUREMENT_GUIDE.md)
- Phase 12 引継: [`../02_specifications/quality/PENDING_NFR_WORK.md`](../02_specifications/quality/PENDING_NFR_WORK.md) X-5f

---

## 改訂履歴

| バージョン | 日付 | 変更内容 |
|---|---|---|
| 1.0 | 2026-05-04 | 初版。Q25 屋外/温度/24h ストレステスト計画 8 試験 (ST-1〜8) を策定。試験別手順 + 計測指標 + 結果テンプレート + 資源リスト + リスク管理を含む |
