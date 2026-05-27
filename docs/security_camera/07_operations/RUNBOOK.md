# 運用ランブック (Operations Runbook)

**バージョン**: 1.0
**作成日**: 2026-05-02
**対象アクター**: 👤 保守者 (Maintainer) / 👤 運用者 (Operator)
**目的**: 障害症状の識別 → トリアージ → 復旧手順 → エスカレーションまでを 1 か所に集約。X-4 タスク (PENDING_NFR_WORK.md) の成果物。
**適用範囲**: Phase 11 現実装 (Tier 1: Spresense + GS2200M) — Tier 移行後は要再評価

> **本書の使い方**:
> 1. **§1 症状チェックリスト** で観測事象を識別
> 2. **§2 トリアージ** で重症度と次のアクションを判定
> 3. **§3〜§5 復旧手順** から該当のものを実施
> 4. 復旧不能なら **§6 エスカレーション** で Tier 移行判断へ
> 5. 復旧後は **§7 事後対応** で再発防止と記録

---

## §0 事前確認 (Pre-flight Check)

運用開始前に毎回確認:

- [ ] **LAN 隔離前提の確認**: Spresense + AP + PC viewer が同一物理 LAN 内に閉じている
  - 公開 NW 経由の運用は本実装では未保証 (THREAT_MODEL.md TI-1, TS-1, TD-1 参照)
- [ ] **WiFi AP 稼働中**: ノート PC の AP 機能が ON、SSID と Pass が `wifi_config.h` と一致
- [ ] **Spresense 電源**: 給電中、シリアルコンソールが応答する
- [ ] **PC viewer 起動**: `cargo run` または実行ファイルで起動済み

---

## §1 症状チェックリスト

| # | 症状 | 重症度 | 推奨手順 |
|---|---|---|---|
| S1 | PC viewer に映像が出ない (起動直後) | 中 | §3.1 → §3.2 |
| S2 | 映像が時々止まる / カクつく | 中 | §3.3 (TCP Health 確認) |
| S3 | 映像が完全に止まり、復帰しない | 高 | §3.4 (FAILED 状態確認) → §4 |
| S4 | PC viewer がクラッシュ | 中 | §3.5 |
| S5 | Spresense シリアルコンソールにエラー連発 | 高 | §3.6 |
| S6 | 録画ファイルが作成されない | 中 | §3.7 |
| S7 | システム全体が無応答 (シリアルも反応なし) | 高 | §4.1 (Spresense 強制再起動) |
| S8 | 同じ障害が頻発 (連日) | 高 | §6 (Tier 切替判断へ) |

---

## §2 トリアージ (Triage)

### 質問フロー

1. **PC viewer は起動しているか?**
   - No → §3.1 PC viewer 起動 + §3.2 接続確立
   - Yes → 次へ

2. **Spresense シリアルコンソールに反応があるか?**
   - No → §4.1 Spresense 強制再起動 (重症: ハード or FW フリーズ)
   - Yes → 次へ

3. **TCP 接続は確立しているか?** (PC viewer ステータス確認)
   - No → §3.2 接続確立試行 → 失敗なら §3.4
   - Yes → 次へ

4. **健全性状態は?** (TCP Health Monitor の表示確認)
   - HEALTHY → §3.3 (一時的なジッタの可能性)
   - CAUTION/WARNING → §3.3 (継続観察、悪化したら §4)
   - CRITICAL/RECONNECTING → §3.4 (Auto-Reconnect 進行中、5 分待機)
   - **FAILED** → §4 (人手介入が必要)

---

## §3 軽症障害の復旧手順

### §3.1 PC viewer の起動・再起動

```bash
# 1. プロセス確認
ps aux | grep security_camera_viewer

# 2. 起動 (停止中の場合)
cd /home/ken/Rust_ws/security_camera_viewer
cargo run --release  # または実行ファイル直接起動

# 3. クラッシュした場合は core/log を確認
ls -lt ~/.cache/security_camera_viewer/  # ログ場所は要確認
```

**確認**: GUI が起動し、接続待ち状態になる。

### §3.2 TCP 接続の確立

```bash
# 1. Spresense の IP を確認 (シリアルコンソールから)
#    例: "Got IP: 192.168.1.42"

# 2. PC から Spresense への ping
ping -c 3 192.168.1.42

# 3. TCP 8888 ポート到達確認
nc -zv 192.168.1.42 8888

# 4. PC viewer の接続先設定が正しいか確認
#    (gui_main.rs / tcp_connection.rs の接続先設定)
```

**接続成功**: PC viewer に映像が表示される。

**接続失敗時**:
- `ping` 失敗 → WiFi AP 確認 (§3.6) → §4.2 (Spresense WiFi 再接続)
- `ping` OK だが TCP 失敗 → Spresense 側 TCP listen 状態確認 → §3.6 (シリアルログ)

### §3.3 一時的な品質劣化への対応 (CAUTION / WARNING)

```
症状: 映像が時々止まる、TCP Health Monitor が CAUTION/WARNING
原因候補:
  - 構造的天井 #1 (GS2200M tx_buff[1]) の一時的負荷
  - WiFi 電波弱化 (距離 / 干渉)
  - PC 側 CPU 負荷増 (録画 + 動き検出同時)
```

**手順**:

1. **観察**: 5 分間、TCP Health の遷移を観察
   - HEALTHY に戻れば対処不要 (一時的なバースト)
   - CRITICAL/FAILED へ悪化 → §3.4 / §4

2. **PC 側負荷軽減** (録画ストレージ容量確認):
   ```bash
   df -h ~/録画ディレクトリ  # 1GB 上限近いか確認
   ```

3. **WiFi 電波確認** (PC で):
   ```bash
   iwconfig wlan0  # Link Quality 確認、低ければ Spresense を AP 近くに移動
   ```

4. 改善なし → §4 へエスカレーション

### §3.4 RECONNECTING 状態の確認

**自動再接続が進行中** (max=5 回, exp backoff):

| 試行 | backoff 待機 |
|---|---|
| 1 回目 | 1s |
| 2 回目 | 3s |
| 3 回目 | 5s |
| 4 回目 | 7s |
| 5 回目 | 9s |

**累計**: 約 25 秒で 5 回試行が終わる。

**手順**:
1. シリアルコンソールで `Reconnect attempt N/5` のログを観察
2. 5 回試行完了まで **30 秒待機**
3. 復旧 → 完了
4. **5 回失敗 → FAILED 状態 → §4 必須**

> ⚠ ADR-002 v1.1 で判明: 自動再接続は構造的天井 #1 が原因の場合**逆効果**で、復旧確率が低い。FPS 6.74 → 2.77 (-59%) に悪化する。FAILED 固着は **構造的に発生しやすい**。

### §3.5 PC viewer クラッシュへの対応

```bash
# 1. 強制終了 (フリーズしている場合)
killall security_camera_viewer

# 2. ログ確認
journalctl --user -u security_camera_viewer 2>/dev/null
# または core dump
ls -lt /var/lib/systemd/coredump/ 2>/dev/null

# 3. 再起動 (§3.1 へ)
```

**よくあるクラッシュ原因**:
- bounded(3) channel overflow (ADR-008 で防御済だが完全ではない)
- Rust panic (unwrap on None) — 発見次第 GitHub issue
- OS の OOM killer (録画 1GB ファイル + 大量バッファ)

### §3.6 Spresense シリアルログの解析

```bash
# シリアルコンソール接続 (Linux)
screen /dev/ttyUSB0 115200
# または
minicom -D /dev/ttyUSB0 -b 115200

# ログ重要項目:
# - "[CAM] Camera thread started"  → 起動正常
# - "[CAM] Failed to associate"    → §4.2 WiFi 再接続
# - "[CAM] Got IP: x.x.x.x"        → ネットワーク準備 OK
# - "[CAM] TCP listen on port 8888" → ストリーミング待機中
# - "TCP disconnected, waiting Xms" → Auto-Reconnect 進行中
# - "Max reconnect attempts reached" → §4 FAILED 状態
```

**ログレベルの注意**: 現状 LOG_INFO 以上のみ出力される (CONFIG_DEBUG_ENABLE=1)。詳細な情報は LOG_DEBUG が必要だが現状無効化されている (`CROSS_CUTTING_CONCERNS.md` §1 参照)。

### §3.7 録画ファイルが作成されない

```bash
# 1. PC viewer の動き検出設定確認 (motion_detector 有効か)
# 2. 録画ディレクトリのパーミッション
ls -ld ~/録画ディレクトリ
# 3. ffmpeg 動作確認 (mp4_recorder.rs は ffmpeg subprocess)
which ffmpeg && ffmpeg -version

# 4. 1GB 上限到達 (ローテーション未実装の問題)
du -sh ~/録画ディレクトリ
# 1GB 近ければ古いファイルを手動削除
```

> ⚠ X-5a / X-5b 技術負債: 1GB 自動ローテーション + 時間分割は **未実装**。手動管理が必要。

---

## §4 中重症障害の復旧手順

### §4.1 Spresense 強制再起動

**症状**: シリアルコンソールも無応答 / FW フリーズ疑い

```
手順:
1. Spresense の RESET ボタンを押す (短押し)
   - ボード上の SW1 (RESET) ボタン
2. シリアルコンソールに NuttX 起動メッセージが流れることを確認
3. "[CAM] Camera thread started" が出るまで待機 (~10 秒)
4. PC viewer から接続再試行 (§3.2)
```

**復旧不能時**:
- 電源 OFF/ON (USB ケーブル抜き差し)
- それでもダメなら §6 (ハード障害疑い)

### §4.2 WiFi 再接続が成功しない

**症状**: シリアルログに "Failed to associate" 連発

**原因と対応**:

| 原因 | 確認方法 | 対応 |
|---|---|---|
| AP がオフ | ノート PC の AP 機能設定確認 | AP を ON にする |
| SSID/Pass 誤り | `wifi_config.h` と AP 設定を比較 | wifi_config.h を修正 → 再ビルド + 書込 |
| 電波弱化 | RSSI 確認、Spresense を AP 近くに移動 | 配置変更 |
| WiFi チャネル干渉 | 周辺の WiFi 数 (PC で `iwlist scan`) | AP のチャネルを変更 |
| GS2200M 故障 | 別 Spresense で同設定試行 | ハード交換 (§6) |

### §4.3 FAILED 状態からの復旧 (5 回再接続失敗後)

**症状**: TCP Health = FAILED、自動再接続が止まっている、シリアルログに "Max reconnect attempts reached"

**手順**:

1. **状況記録**:
   ```bash
   # シリアルログを 100 行コピーして保存
   # PC viewer の metrics CSV (もしあれば) を保存
   ```

2. **Spresense 再起動**: §4.1 の手順実施
3. **PC viewer 再起動**: §3.1
4. **接続復旧確認**: §3.2

**頻発する場合 (1 日 3 回以上)**:
- 構造的天井 #1 が支配的に発生 (Phase 8/9 実測値ベース)
- → §6 へエスカレーション (Tier 移行検討)

### §4.4 USB CDC-ACM 経由の代替経路 (Tier C 暫定切替)

WiFi 経路が慢性的に不安定な場合の緊急回避:

```
1. USB ケーブルで Spresense と PC を直結
2. /dev/ttyACM0 として認識されることを確認:
     ls /dev/ttyACM*
3. PC viewer の接続先を USB に切替
   (現状実装: tcp_connection vs serial の選択ロジック確認要)
4. USB 経路で稼働継続
```

> ⚠ Tier C は本番運用としては正式採用されていない (ADR-006 GATE-1 未実施)。**緊急避難用**。

---

## §5 環境問題への対応

### §5.1 PC viewer 側ストレージ容量不足

```bash
# 1. 録画ディレクトリの古い MP4 を削除
ls -lt ~/録画ディレクトリ/*.mp4 | tail
rm 古いファイル.mp4

# 2. 一時ファイル削除
df -h /tmp
```

### §5.2 PC 過負荷 (CPU 使用率高)

```bash
top  # security_camera_viewer の CPU% 確認
# 高い場合:
# - 動き検出感度を下げる (motion_detector_config 編集)
# - 同時実行している他プロセスを停止
```

### §5.3 LAN 環境の確認

```bash
# IP 衝突確認
arp -a | grep 192.168.1.42  # Spresense IP

# DHCP リース確認 (AP 側)
# AP 設定画面でクライアント一覧を確認
```

---

## §6 エスカレーション (Tier 移行判断)

以下のいずれかに該当する場合、現実装での運用継続を断念し Tier 移行を検討:

| 条件 | 対応 |
|---|---|
| FAILED 状態が **1 日 3 回以上** 発生 | Tier C (USB-only) 暫定切替 → 安定したら正式採用判断 |
| 構造的天井 #1 起因の遅延が **常時 500ms 超** | Tier 2 (ESP32-S3) 移行検討 |
| Full HD 解像度が要求される | Tier 3 (RPi CM5) 移行確定 (現実装では不可能) |
| ハード故障 (GS2200M / カメラ) | Tier 内修理 or 交換 → 慢性的なら Tier 移行 |

**判断材料**:
- ADR-006 GATE-1 ハードウェア評価
- `02_specifications/architecture/SPRESENSE_TCP_CONSTRAINTS.md` §13.5 (4 候補比較)
- `02_specifications/quality/risk_analysis/FMEA.md` (RPN 上位 + 対策状況)

**移行判断者**: 保守者 + 設置者 + (将来) ステークホルダー

---

## §7 事後対応 (Post-incident)

復旧後の必須アクション:

1. **症状記録**: いつ / どんな症状 / どの手順で復旧したか
2. **TECHNICAL_DEBT_REGISTER.md への登録**: 新しい failure mode を発見した場合
3. **FMEA.md への反映**: RPN 値の見直しが必要なら更新
4. **本ランブックの改訂**: 不足している手順を発見したら追記

### 記録テンプレート

```
## インシデント YYYY-MM-DD-HHMM

- 発見者:
- 症状:
- トリアージ結果: §_._
- 実施手順: §_._
- 根本原因: (推定)
- 復旧時間: __ 分
- 再発防止: (アクション)
- 関連 FMEA ID:
```

---

## §8 連絡先・エスカレーション体制

**現状**: 単一開発者 (ken11111) のため、保守者 = 開発者。

**本番運用移行時の TODO** (Phase 12+):

- [ ] エスカレーション体制を確立 (1 次対応 / 2 次対応 / 開発者の役割分担)
- [ ] 障害アラート手段の設定 (現状: 手動確認のみ)
- [ ] 24 時間対応か業務時間内対応かの方針決定

---

## §9 関連文書

### 上位文書
- ユースケース UC-5 (TCP 切断時の自動復旧): [`../02_specifications/use_cases/primary_use_cases.md`](../02_specifications/use_cases/primary_use_cases.md)
- ユースケース UC-7 (障害時の人手介入): 同上
- 異常系 ES-1〜5: [`../02_specifications/use_cases/exception_scenarios.md`](../02_specifications/use_cases/exception_scenarios.md)
- アクター (保守者): [`../02_specifications/use_cases/actors.md`](../02_specifications/use_cases/actors.md)

### 根拠文書
- 構造的天井 #1〜#5: [`../02_specifications/quality/GLOSSARY.md`](../02_specifications/quality/GLOSSARY.md) §2
- 失敗モード台帳 (FMEA): [`../02_specifications/quality/risk_analysis/FMEA.md`](../02_specifications/quality/risk_analysis/FMEA.md)
- 脅威モデル (LAN 隔離前提): [`../02_specifications/quality/risk_analysis/THREAT_MODEL.md`](../02_specifications/quality/risk_analysis/THREAT_MODEL.md)
- ADR-002 v1.1 (再接続の逆効果発見): [`../03_achievements/architecture_decisions/system_architecture/ADR_002_NETWORKING_TCP_HEALTH_MONITORING.md`](../03_achievements/architecture_decisions/system_architecture/ADR_002_NETWORKING_TCP_HEALTH_MONITORING.md)
- 健全性状態遷移: [`../02_specifications/architecture/gs2200m_health_state_machine.puml`](../02_specifications/architecture/gs2200m_health_state_machine.puml)
- TCP 構造的制約: [`../02_specifications/architecture/SPRESENSE_TCP_CONSTRAINTS.md`](../02_specifications/architecture/SPRESENSE_TCP_CONSTRAINTS.md)

### 次のアクション (本ランブックから派生)
- Phase 12+ で本書の手順を実運用で検証
- §8 エスカレーション体制の確立
- 自動アラート機構 (現状未実装)

---

## 改訂履歴

| バージョン | 日付 | 変更内容 |
|---|---|---|
| 1.0 | 2026-05-02 | 初版。X-4 タスク完了。8 症状 → トリアージ → §3 軽症 7 手順 / §4 中重症 4 手順 / §5 環境 3 手順 / §6 Tier 移行判断 / §7 事後対応 / §8 連絡先体制 |
