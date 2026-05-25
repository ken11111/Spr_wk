# 要求トレーサビリティマトリクス (Q1〜Q25 × STAMP M-1〜M-37 × Phase)

**バージョン**: 1.0
**作成日**: 2026-05-23
**目的**: [`../../01_requirements/FUNCTIONAL_REQUIREMENTS.md`](../../01_requirements/FUNCTIONAL_REQUIREMENTS.md) v1.1 の **Q1〜Q25** を、[`STAMP_STPA_ANALYSIS.md`](STAMP_STPA_ANALYSIS.md) v1.10.1 の対策 **M-1〜M-37**、関連 UCA/Hazard、実装 Phase、達成状態と対応付ける。
**位置付け**: arc42 §11 (Risk Trace), ISO/IEC 25010 補完。**「要求 vs 対策 vs 実装事実」の 3 軸を一画面で把握** することが目的。

---

## 📋 サマリ

### 要求達成度の俯瞰

| カテゴリ | 達成 | 部分達成 | 未達/WONT FIX | 計 |
|---|---|---|---|---|
| 機能要求 (Q1-Q15) | 9 | 4 | 2 | 15 |
| 非機能要求 (Q16-Q19) | 1 (Q16 Must) | 1 (Q19) | 2 (Q16 Want WONT FIX, Q17) | 4 |
| 制約・運用 (Q20-Q25) | 5 | 1 | 0 | 6 |
| **合計** | **15** | **6** | **4** | **25** |

### 主要観察

1. **WONT FIX 4 件は全て構造的天井起因** (Q1 Full HD, Q3 H.264, Q5 RTSP, Q16 Want 100ms): STAMP §10.6 と整合
2. **未対応の技術負債** (Q8/Q9/Q11/Q14): **新規対策 M-38/M-39/M-40 候補** として STAMP v1.11 へ反映
3. **Q17 ドロップ率 ≤30% 未達** (Phase C 62.3% / Phase 10 61.9%): **M-34 (画像サイズ上限制御) + M-36 (Sub-Core) で改善見込み**
4. STAMP/STPA M-1〜M-37 のうち **Q1-Q25 と直接紐付かない対策** (例: M-13 監査ログ, M-14 永続ログ, M-15 rate limit) は **STPA-Sec 起点** で要求書の Q24 セキュリティ Option 内に包含されていない論点

---

## §1 機能要求 (Q1〜Q15)

| Q | 内容 | v1.1 確定値 | 関連 UCA | 関連 M-* | Phase | 状態 | 備考 |
|---|---|---|---|---|---|---|---|
| Q1 | 映像解像度/フレームレート | VGA 640×480 | (構造的天井 #1) | (HW 制約) | — | ✅ 確定 (WONT FIX Full HD) | Tier 2/3 で再評価 |
| Q2 | HDR | HDR 無効 | — | — | — | ✅ 確定 (要求 vs 実装 乖離) | 既知 |
| Q3 | 映像圧縮 | MJPEG | — | — | — | ✅ 確定 (WONT FIX H.264) | dead code 削除 X-9 連動 |
| Q4 | 通信 IF | TCP/WiFi + USB CDC-ACM (補助) | UCA-DRV-USB.1 | M-7, M-35a | 12.2 | ✅ 達成 (副経路は M-35c 統合候補) | |
| Q5 | 通信プロトコル | カスタム MJPEG | — | — | — | ✅ 確定 (WONT FIX RTSP) | |
| Q6 | 録画トリガー | 手動 + 動き検出 | UCA-MD.1〜4 | M-25, M-27 | 12.1 | 🟡 部分達成 (誤検知率未検証) | motion_corpus 整備が前提 |
| Q7 | ファイル保存形式 | MP4 (H.264 encoded) | — | (FFmpeg 依存) | — | ✅ 達成 | |
| **Q8** | **ストレージ管理 (容量上限・ローテーション)** | **1GB 上限 (固定), ローテ未実装** | **UCA-C1.2** (H-6 ストレージ枯渇) | **M-3** (1GB ローテーション) | **12.1** | 🟡 **未実装→M-3 で対応提案済** | TECHNICAL_DEBT_REGISTER |
| **Q9** | **ファイル分割 (時間/サイズ)** | **イベント分割のみ実装、時間分割未実装** | (新規) | **M-38 候補 (v1.11)** | (未配置) | 🔴 **未対応** | **B プランで新規追加** |
| Q10 | リアルタイム表示 | egui GUI | UCA-VIEW.* | M-20 (systemd) | 12.1 | 🟡 部分達成 (クラッシュ復旧 M-20 待ち) | |
| **Q11** | **再生機能** | **アプリ内未実装 (外部プレーヤー前提)** | — | **M-40 候補 (v1.11, スコープ判断要)** | (未配置) | 🔴 **未対応 (スコープ判断)** | 家庭用では外部プレーヤーで OK の判断もあり |
| Q12 | 動き検出 | 実装済 (motion_detector.rs) | UCA-MD.1〜4 | M-25 (FP/FN コーパス), M-27 (ランブック) | 12.1 | 🟡 部分達成 | |
| Q13 | 複数カメラ | 単一カメラ前提 | — | — | — | ✅ 達成 (要求と一致) | |
| **Q14** | **タイムスタンプ・OSD** | **メタデータのみ、映像重畳 OSD 未実装** | (新規) | **M-39 候補 (v1.11)** | (未配置) | 🔴 **未対応** | **B プランで新規追加** — 証拠性向上 |
| Q15 | 通知機能 | 不要 | — | — | — | ✅ 確定 (要求と一致) | |

---

## §2 非機能要求 (Q16〜Q19) — 実データ評価

| Q | 内容 | v1.1 確定値 | 実測達成度 (v1.0 評価) | 関連 UCA | 関連 M-* | Phase | 状態 |
|---|---|---|---|---|---|---|---|
| **Q16** | **許容遅延** | Must <1s ✅ / Want <100ms WONT FIX | **Must 達成率 99.98%** (Phase C 1 サンプル超過のみ) / Want **0%** (構造的天井 #1) | UCA-B1.3 | M-2 (動的閾値) | 12.3 | ✅ Must / 🔴 Want WONT FIX |
| **Q17** | **録画ドロップ率** | 目標 ≤30% | **未達**: Phase C **62.3%**, Phase 10 **61.9%** (目標から大幅乖離) | UCA-A1.2, UCA-A1.5, UCA-FBM.1 | M-1, M-24, **M-34** (画像 cap), **M-36** (Sub-Core) | 12.3 | 🔴 **未達** |
| Q18 | 起動モード | Spresense 自動 / PC 手動 | 要求と一致 | — | — | — | ✅ 達成 |
| **Q19** | **エラー回復** | 自動再接続 max=5 + エラーログ | ADR-002 v1.1 で「逆効果」判明、FPS 6.74→2.77 悪化 | UCA-B1.1〜.4 | M-2 (動的閾値), M-6 (ランブック), M-28〜32 (OS/PWR) | 12.1-12.3 | 🟡 部分達成 (M-2 で改善見込み) |

詳細評価: [`../../06_evidence/metrics_analysis/performance_trends.md`](../../06_evidence/metrics_analysis/performance_trends.md) §Requirements Achievement (本文後続セクションで追記予定) + `figures/ds_13_requirement_achievement.png`

---

## §3 制約・運用要求 (Q20〜Q25)

| Q | 内容 | v1.1 確定値 | 関連 UCA / Hazard | 関連 M-* | Phase | 状態 |
|---|---|---|---|---|---|---|
| Q20 | Spresense 拡張ボード | カメラ + WiFi 確定 | (構造的天井 #5 GS2200M) | (Tier 移行で改善) | — | ✅ 確定 |
| Q21 | PC 環境 | Rust 主体 + Linux/Windows | UCA-VIEW.1, .2 | M-20 (systemd), M-35b | 12.1, 12.2 | ✅ 達成 |
| Q22 | Phase 分け | Phase A→B→C→10→12 → 12.1/2/3 → 13+ | — | (本書 §10.6 連動) | — | ✅ 整理済 |
| Q23 | Rust クレート選定 | tokio, egui, image, ffmpeg-next 等 | UCA-MEMSAFE.1 | (Rust 言語特性で達成済) | — | ✅ 達成 |
| **Q24** | **セキュリティ要件** | **LAN 隔離前提 + Option B (アプリ層簡易認証)** | UCA-AUTH.1, RL.1, RL.2, INT.1, CRYPTO.1〜3, AUDIT.1, .2 | **M-5, M-8, M-10〜M-17** (STPA-Sec 系) | 12.1 (M-5), 12.3 (M-8, M-15), 13+ (M-12, M-14, M-16) | 🟡 Option A→B 移行計画中 |
| Q25 | テスト環境 | (未確定詳細) | UCA-VIEW.* | M-25 (motion FP/FN コーパス) | 12.1 | 🟡 部分達成 |

---

## §4 STAMP/STPA M-* 逆引きインデックス

| M-* | 対応する Q | カバー範囲 |
|---|---|---|
| **M-1** PID 多変数化 (代理指標) | Q17 (ドロップ率), Q19 (回復) | 制御強化 |
| **M-2** Auto-Reconnect 動的閾値 | Q16 (遅延), Q19 (回復) | 一時遅延 vs 真切断判別 |
| **M-3** MP4 ローテーション | **Q8 (ストレージ管理)** | 1GB 上限 |
| M-4 disk 残量 feedback | Q8 | M-3 と一体 |
| **M-5** pre-commit hook | Q24 (cred 漏洩防止) | X-7 連動 |
| M-6 運用ランブック | Q19 (人手介入), Q11 (再生) | 運用 |
| **M-7** USB 副経路ヘルス | Q4 (通信), Q19 (障害可視化) | M-35c へ統合 |
| **M-8** TLS-PSK / アプリ層認証 | Q24 (セキュリティ Option B) | Phase 12 判断 |
| M-9 環境センサ | (要求書外) | 屋外環境 (Q25 関連) |
| M-10〜M-17 (STPA-Sec 群) | Q24 (各防御 Controller) | THREAT_MODEL 14 件 |
| M-18 action_queue 上限制御 | Q17 (ドロップ理由通知) | UCA-FBM.1 |
| M-19 OVERSIZE フラグ | Q17 (大型 frame 検知) | M-35a へ統合 |
| M-20 viewer systemd | Q10 (リアルタイム表示) | UCA-VIEW.2 |
| M-21 起動失敗 LED | Q19 (起動失敗時の通知) | |
| M-22 USB→TCP 自動切替 | Q4 (通信冗長性) | |
| M-23 IOB プール metrics | (要求書外, 内部品質) | M-35a へ統合 |
| **M-24** Scene complexity feedforward | Q17 (ドロップ率改善) | M-1 と統合 |
| M-25 motion FP/FN コーパス | Q6, Q12 (動き検出) | |
| M-26 ISX012 AE Safe Range | Q1 (画質) | Tier 1 限界 |
| M-27 sensitivity ランブック | Q12 (動き検出運用) | M-6 統合可 |
| M-28〜M-33 (OS/PWR 群) | Q19 (回復), Q18 (起動) | watchdog 等 |
| **M-34** 画像サイズ上限制御 | Q17 (ドロップ率), Q1 (帯域天井) | v1.8 追加 |
| **M-35a/b/c** Observability | (要求書外, 計装系) | v1.9 追加, M-7/19/23/31 統合 |
| **M-36** Sub-Core オフロード | (要求書外, CPU 100% 超対策) | v1.10 追加, M-34 実装の前提 |

---

## §5 ギャップ分析 (要求 vs 対策)

### A. 要求にあるが対策が不足

| Q | ギャップ | 提案対策 |
|---|---|---|
| **Q9** ファイル時間分割 | 全く対応無し | **M-38 (v1.11): mp4 30 分/1 時間自動分割** |
| **Q14** OSD 重畳 | 全く対応無し | **M-39 (v1.11): タイムスタンプ等を映像に焼き込み** |
| **Q11** アプリ内再生 | 全く対応無し (スコープ判断要) | **M-40 (v1.11): viewer 再生 UI 追加** — 家庭用では「外部プレーヤー前提」も可 |
| **Q17** ドロップ率 ≤30% | M-1+M-24+M-34+M-36 で改善見込みだが目標達成は不明 | 既存対策で十分か、Tier 移行必要かは M-35a 計測後判断 |

### B. 対策にあるが要求と直接紐付かない (STAMP 起点)

| M-* | 動機 |
|---|---|
| M-13, M-14 (監査ログ) | STPA-Sec で TR-1/TR-2 から発見 — 要求書 Q24 には記載なし |
| M-35a/b/c (Observability) | 実データから「下流ボトルネック特定不可」を発見 |
| M-36 (Sub-Core) | CPU 100% 超推定から発見 |

→ これらは **要求書に明示されていない「品質要求 (QAS)」** で、本書では STAMP 起点で追加価値を提供している。

### C. 要求書側に追記すべき項目候補 (Phase 12 確定方針との整合)

| 候補 | 理由 |
|---|---|
| Q24 への M-13 (操作監査ログ) 明示 | 法的監査要件と整合 |
| Q14 OSD を要求として確定 (or WONT FIX) | 現状「未実装」と書いてあるだけ |
| Q9 時間分割の判断 | 「未実装」のままで放置するか、M-38 提案を取り込むか |

---

## §6 関連文書

- 要求書: [`../../01_requirements/FUNCTIONAL_REQUIREMENTS.md`](../../01_requirements/FUNCTIONAL_REQUIREMENTS.md) v1.1
- 対策原本: [`STAMP_STPA_ANALYSIS.md`](STAMP_STPA_ANALYSIS.md) v1.10.1
- 実データ評価: [`../../06_evidence/metrics_analysis/performance_trends.md`](../../06_evidence/metrics_analysis/performance_trends.md) v1.4
- 達成度可視化: [`../../06_evidence/metrics_analysis/figures/ds_13_requirement_achievement.png`](../../06_evidence/metrics_analysis/figures/ds_13_requirement_achievement.png)
- 関連: [`QUALITY_ATTRIBUTE_SCENARIOS.md`](QUALITY_ATTRIBUTE_SCENARIOS.md) (QAS-1〜10)
- 関連: [`PENDING_NFR_WORK.md`](PENDING_NFR_WORK.md) (X-* タスク台帳)

## 改訂履歴

| Version | Date | 変更内容 |
|---|---|---|
| 1.0 | 2026-05-23 | 初版 — Q1〜Q25 × M-1〜M-37 のトレーサビリティ確立、ギャップ分析で M-38/M-39/M-40 新規候補抽出 |
