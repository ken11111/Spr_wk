# 本番運用デプロイ チェックリスト (家庭用 Tier 1)

**バージョン**: 1.0
**作成日**: 2026-05-08
**対象**: Tier 1 (Spresense + GS2200M) を家庭 LAN で本番運用に切替える際の手順
**位置付け**: Phase 12.3 Step B-1 (LAN 隔離前提の運用文書化), Phase 12.5 完了基準 (E) の主要成果物

> **本書の用途**:
> 1. 検証段階 → 本番運用へ切り替える時の **作業順序** を確実にする
> 2. 旧 WiFi 認証情報の漏洩リスクを管理する (X-7 ユーザー判断 Option C 連動)
> 3. セキュリティ Option B (LAN 隔離前提 + アプリ層認証 + IP allowlist + ログ署名) の運用化前提を明文化
> 4. 本書のチェックを通った時点で「Phase 12 完了基準 (E)」を満たす

---

## §1 適用条件 (Phase 12 確定方針)

本書は以下を**前提**とする:

- ✅ Tier 1 維持 (Spresense + GS2200M, 新規ハード導入なし)
- ✅ 家庭用個人 LAN (公開 NW / 業務 SLA 不要)
- ✅ ユーザー判断 X-7 Option C (history そのまま, 本番移行時に WiFi パスワード変更)
- ✅ セキュリティ Option B (LAN 隔離 + アプリ層 PSK + IP allowlist + ログ署名)

**適用しない場合**:
- 公開 NW で運用 → 本書では不十分、Tier 移行 + Option A (TLS) 検討必要
- 業務利用 → SLA 要件再考、別チェックリスト必要

---

## §2 事前準備 (Phase 12 中に並行実施推奨)

### §2.1 ハードウェア
- [ ] Spresense メインボード × 1 (動作確認済個体)
- [ ] GS2200M モジュール (動作確認済)
- [ ] USB CDC ケーブル (Tier C フォールバック用)
- [ ] WiFi AP (家庭ルーター or 専用 AP, 24h 稼働可)
- [ ] 電源 (UPS 推奨、長期稼働の電断対策)
- [ ] 設置場所の物理セキュリティ (筐体施錠 / 屋内, FMEA C7)

### §2.2 ネットワーク
- [ ] **本番 WiFi SSID/Pass を新規発行** (検証期で使った値は廃棄)
  - 理由: X-7 で git history に旧認証情報が残存 (公開時のリスク)
  - パスワード強度: WPA2-PSK with 12+ 文字、英数記号混在
- [ ] **LAN 隔離の確認**: Spresense + AP + PC viewer が**同一 LAN 内**で完結し、外部経由のアクセス経路がない
  - ルーターのポートフォワーディング設定: TCP 8888 を **公開しない**
  - DMZ 配置はしない
  - VPN 経由のリモート接続は本書スコープ外
- [ ] AP の MAC アドレスを記録 (Evil Twin 対策の参考)
- [ ] PC viewer の IP を allowlist 候補として記録 (B-3 で利用)

### §2.3 設定ファイル
- [ ] `apps/examples/security_camera/wifi_config.h` を本番 SSID/Pass で更新
  - **再ビルド**して fw に反映、git にはコミット**しない** (.gitignore 済)
- [ ] `apps/examples/security_camera/config.h` の必要設定:
  - `CONFIG_CAMERA_WIDTH/HEIGHT/FPS` (VGA 30fps が確定値)
  - `CONFIG_USB_DEVICE_PATH` (USB 主路の場合は `/dev/ttyACM0`)
  - `CONFIG_LOG_LEVEL = LOG_INFO` (デバッグ時は LOG_DEBUG)
- [ ] PC viewer 側 録画ディレクトリ確認:
  - 容量 > 1GB (RecordingPolicy.storage_quota_bytes)
  - 書込権限あり

---

## §3 本番切替時 必須チェック

### §3.1 Cold start テスト (最初の 1 時間)
- [ ] Spresense 電源投入 → シリアルコンソールで `[CAM] Camera thread started` 確認
- [ ] WiFi 接続成功: `Got IP: x.x.x.x` をシリアルで確認
- [ ] PC viewer 起動 → 接続成立、映像表示確認
- [ ] HUD 表示確認 (X-5d): タイムスタンプ / フレーム# / REC ドット (録画時)
- [ ] 録画開始 → 1 ファイル生成確認 (`./recordings/YYYYMMDD_HHMMSS.mp4`)
- [ ] 録画停止 → 外部プレーヤーで再生確認

### §3.2 Auto-Reconnect 動作確認
- [ ] 意図的に AP を一時 OFF (10 秒) → ON で復旧
- [ ] PC viewer に CRITICAL/RECONNECTING 状態が一時的に出ること
- [ ] 復旧後ストリーミング再開
- [ ] FAILED 状態固着なし
  - ⚠ FAILED 出る場合は **ADR-002 v1.2 (Phase 12.2 で改定予定)** で対処

### §3.3 録画ローテーション動作 (X-5a/b)
- [ ] 連続録画モード ON で 12 時間運用
- [ ] 録画ディレクトリのファイル数を 1 時間ごとに記録
- [ ] 1GB 上限到達後、**古いファイルから削除されること**を確認
- [ ] 最新ファイルが常に書き込み中 (停止しない)
- [ ] **フォルダ全体サイズが 1GB を超過しない**

### §3.4 セキュリティ Option B 動作確認 (Phase 12.3 完了後)
- [ ] **B-2 アプリ層 PSK**:
  - 不正 PSK で接続試行 → drop されること (`nc spresense_ip 8888` < /dev/null で確認)
  - 正 PSK で接続成立すること
- [ ] **B-3 IP allowlist**:
  - 別 PC (allowlist 外) から接続試行 → drop
  - allowlist 内 PC から接続成立
- [ ] **B-4 ログ署名**:
  - syslog 1 日 1 回 HMAC 付与の確認
  - 改ざん検知のシミュレーション
- [ ] (Phase 12.3 未着手なら本節スキップ可、その代わり LAN 隔離を強く運用で守る)

---

## §4 24h 稼働 ベリフィケーション (X-5f ST-1 連動)

### §4.1 計測前準備
- [ ] X-6 計装が有効 (`[CPU]` 行が syslog に出ることを確認)
- [ ] 録画ローテーション動作確認 (§3.3 完了)
- [ ] シリアルログ取得手段:
  - `screen -L /dev/ttyUSB0 115200` (screenlog.0 に保存)
  - or `minicom -C measurement.log`

### §4.2 計測中 (24h)
- [ ] 1 時間ごとに観察:
  - `[CPU] camera/usb/control` 行が継続的に出力されているか
  - `tcp_health_moving_avg_ms` が異常値ではないか
  - 録画ファイル数の遷移 (1GB ローテーション動作)
- [ ] 異常時の手順: [`RUNBOOK.md`](RUNBOOK.md) に従う

### §4.3 計測後解析
- [ ] `python3 scripts/cpu_measurement/parse_cpu_log.py screenlog.0` で集計
- [ ] アップタイム / FPS / ドロップ率 / FAILED 回数を記録
- [ ] [`STRESS_TEST_PLAN.md`](STRESS_TEST_PLAN.md) §6 結果テンプレートに記入
- [ ] **判定基準** (Phase 12 完了 (A)):
  - 強制再起動 0 回 (Spresense / PC とも)
  - 平均 FPS ≥ 5.0 (Phase 8 ベース 75% 以上)
  - ドロップ率 ≤ 80%
  - FAILED 状態固着 0 回 → 1 回以上発生時は Phase 12.2 で再接続戦略再検討必須

---

## §5 緊急時の介入手順

24h 稼働中の異常は **保守者** ([`../02_specifications/use_cases/actors.md`](../02_specifications/use_cases/actors.md)) が以下で対応:

| 症状 | 一次対応 | 参照 |
|---|---|---|
| 映像が止まる | RUNBOOK §3.3 / §3.4 | [`RUNBOOK.md`](RUNBOOK.md) |
| FAILED 状態固着 | Spresense 強制再起動 (§4.1) | RUNBOOK §4.3 |
| Spresense 無応答 | 電源 OFF/ON | RUNBOOK §4.1 |
| 録画停止 | ストレージ容量確認 (§3.7) | RUNBOOK §3.7 |
| WiFi 切断 連発 | AP 再起動 + RSSI 確認 | RUNBOOK §4.2 |

緊急時連絡先: 単一開発者 (現状). 将来チーム化時に追記。

---

## §6 本番移行 完了宣言 (Sign-off)

以下すべてにチェックが入った時点で「本番運用開始」とする:

- [ ] §2 事前準備 全項目
- [ ] §3 本番切替時 必須チェック 全項目
- [ ] §4 24h 稼働ベリフィケーション 全項目
- [ ] (Phase 12.3 完了後) §3.4 セキュリティ Option B 動作確認
- [ ] §5 緊急時介入手順を保守者が把握済

**Sign-off**:
- 日付: ____________
- 保守者: ____________
- 観測初期値:
  - 平均 FPS: ____________
  - 平均 TCP send 時間: ____________
  - 録画ファイル容量管理: ____________

---

## §7 関連文書

- 上位: [`../05_future_actions/phase_planned/Phase12_実施計画書.md`](../05_future_actions/phase_planned/Phase12_実施計画書.md) §Phase 12.5
- セキュリティ前提: [`../02_specifications/quality/risk_analysis/SECURITY_GAP_ANALYSIS.md`](../02_specifications/quality/risk_analysis/SECURITY_GAP_ANALYSIS.md) §5 Option B
- 脅威モデル: [`../02_specifications/quality/risk_analysis/THREAT_MODEL.md`](../02_specifications/quality/risk_analysis/THREAT_MODEL.md)
- 失敗モード: [`../02_specifications/quality/risk_analysis/FMEA.md`](../02_specifications/quality/risk_analysis/FMEA.md) C6 (ストレージ) / C7 (環境)
- ストレステスト: [`STRESS_TEST_PLAN.md`](STRESS_TEST_PLAN.md) ST-1
- 運用ランブック: [`RUNBOOK.md`](RUNBOOK.md)
- CPU 計測: [`CPU_MEASUREMENT_GUIDE.md`](CPU_MEASUREMENT_GUIDE.md)
- 要求書: [`../01_requirements/FUNCTIONAL_REQUIREMENTS.md`](../01_requirements/FUNCTIONAL_REQUIREMENTS.md) v1.1 (WONT FIX 5 件)

---

## 改訂履歴

| バージョン | 日付 | 変更内容 |
|---|---|---|
| 1.0 | 2026-05-08 | 初版。Phase 12.3 Step B-1 + 12.5 完了基準 (E) として家庭用 Tier 1 のための本番移行手順を整備。事前準備 / 必須チェック / 24h ベリフィケーション / 緊急介入 / Sign-off の 7 章構成 |
