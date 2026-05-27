# Pending NFR / Quality Work — 未着手タスク台帳

**作成日**: 2026-05-01
**目的**: 本セッション (P0 ドキュメント整備計画) の中で**判明したが未着手**の NFR 関連タスクを durable に記録し、context 圧縮や session 跨ぎでの記憶喪失を防ぐ
**位置付け**: P0 完了後の P1 / P2 / 計画外タスクの全集合

> **既存台帳との棲み分け**:
> - 個別の技術負債は [`../../05_future_actions/technical_debt/TECHNICAL_DEBT_REGISTER.md`](../../05_future_actions/technical_debt/TECHNICAL_DEBT_REGISTER.md) で管理 (12 件登録済)
> - 本台帳は **品質要求 (NFR) 視点** で本セッションで判明したものを集約。重複は cross-reference で繋ぐ

---

## P1 タスク (アーキテクト推奨, 高優先)

### ✅ P1-A. CPU/帯域予算表の追記 — **完了 (2026-05-01)**

**成果物**: [`CPU_BANDWIDTH_BUDGET.md`](performance/CPU_BANDWIDTH_BUDGET.md) 新設
**実施内容の要点**:
- CPU モデルの正確化: CONFIG_SMP=n のため**単一コア動作** (旧記述「6 コア」を訂正)
- 5 アプリスレッドの CPU 共有を理論的に分析、推定 CPU% を提示
- 帯域予算: SPI 4 MHz 実効 67% (337 KB/s) / USB 70% (1.06 MB/s) / WiFi は律速されるため不要
- レイテンシ予算: Phase 8 実測値で End-to-End ~200 ms と推定、TCP send が 67% を占有
- 計測手段の確立方針: CONFIG_SCHED_CPULOAD=y 有効化を X-6 に格上げ

**残課題**: CPU 利用率の実測 (X-6 で別途実施)

---

### P1-A (旧 — 完了済み記述, 履歴用)

**目的**: SPRESENSE_TCP_CONSTRAINTS.md は **メモリ予算**は完備だが、**CPU 利用率予算** と **帯域利用率** が欠落している。性能議論の精度向上のため。

**作業内容**:
- Cortex-M4F × 6 コアの利用状況 (どのスレッドが何コアで何 % か)
  - main task / camera_thread / usb_thread / control_thread / gs2200m driver task の CPU 利用率実測
  - 計測手法: NuttX `top` 相当機能 or perf_logger 経由でサンプリング
- SPI 4 MHz 利用率 (理論 500 KB/s に対する実効 % )
- WiFi 帯域実効値 (802.11n 理論 vs GS2200M 実効)
- USB CDC-ACM 帯域 (12 Mbps 上限に対する実効 KB/s)
- レイテンシ予算分解 (ISX012 capture → V4L2 dequeue → MJPEG pack → tx_buff → SPI → WiFi → PC decode → display)

**追記先**: `SPRESENSE_TCP_CONSTRAINTS.md` §2.X (新設) または §16

**関連**: QAS-2, QAS-10, QUALITY_REQUIREMENTS.md §2.2 で「⚪ 未文書化」と記載した箇所

---

### ✅ P1-B. Cross-cutting Concerns 集約文書 (arc42 §8) — **完了 (2026-05-01)**

**成果物**: [`CROSS_CUTTING_CONCERNS.md`](CROSS_CUTTING_CONCERNS.md) 新設
**実施内容の要点**:
- ロギング: `LOG_*` (アプリ層) / `_err` 系 (NuttX 慣例) / `syslog()` 直接の **3 系統混在**を識別、CONFIG_LOG_LEVEL は実装と乖離 (gate 機能なし)
- エラー処理: ERR_* コード (config.h:185-202 で 16 個定義) と `-errno` 直接返却の混在、半数以上の ERR_* が未使用
- 設定管理: 3 ファイル分散 (config.h ~50 / wifi_config.h ~10 / mjpeg_protocol.h ~15)
- 🔴 **重大発見**: WiFi 認証情報 (`WIFI_SSID="DESKTOP-GPU979R"` + `WIFI_PASSWORD="B54p3530"`) が **`wifi_config.h` にハードコードされ git track 対象**。リポジトリ公開時にセキュリティリスク
- 緊急度別の改善案 (高/中/低) を §6 で整理

**派生タスク**: WiFi 認証情報のリポジトリ分離 (.gitignore + .example 化) → 緊急度高として X-7 を追加

---

### P1-B (旧 — 完了済み記述, 履歴用)

**目的**: ロギング / エラー処理 / 設定管理の横断方針が散在している (config.h / wifi_config.h / mjpeg_protocol.h に分散, syslog 利用方針がアプリ全体で統一されていない)。

**作業内容**:
- ロギング方針: syslog レベル使い分け, ログ集約, ローテーション
- エラー処理方針: errno 取り扱い, retry 戦略, 失敗時のリソース解放
- 設定管理方針: config.h / wifi_config.h / mjpeg_protocol.h の集約候補
- 国際化方針: 現状日本語ログのみ → 英語化検討

**新規作成先**: `02_specifications/quality/CROSS_CUTTING_CONCERNS.md` (arc42 §8 専用)

**関連**: QUALITY_REQUIREMENTS.md §7.4 (修正性), §1 (設定の集約度)

---

### ⏭️ P1-C. セキュリティ仕様の「設計済」と「実装済」分離 — **スキップ (ユーザー判断 2026-05-02)**

**判断理由**: SECURITY_GAP_ANALYSIS.md (138 行で乖離 9 項目を開示) と SECURITY_ARCHITECTURE.md / SECURITY_SPEC.md 冒頭の警告バナーで透明性は確保済。ファイル分割や renaming は cross-link 修正コストの方が大きく、効果に見合わないと判断。

**派生派遣**: 「実装済セキュリティ要素」(WPA2-PSK / CRC-16-CCITT 等) の正の事実は今後 SECURITY_GAP_ANALYSIS.md §7 として追補する余地あり (緊急度低)

---

### P1-C (旧 — スキップされた記述, 履歴用)

**目的**: SECURITY_GAP_ANALYSIS.md で乖離を開示済だが、より積極的な対応として SECURITY_ARCHITECTURE.md / SECURITY_SPEC.md 自体を「設計提案」と「実装済」に**ファイル分割**することで誤読を防ぐ。

**作業内容**:
- `SECURITY_ARCHITECTURE.md` → `SECURITY_DESIGN_PROPOSAL.md` にリネーム検討 (本書は 100% 提案)
- 実装済セキュリティ要素 (CRC-16, syslog 監査) は `SECURITY_IMPLEMENTED.md` (新設) に分離記述
- functional/SECURITY_SPEC.md も同様に整理

**判断**: SECURITY_GAP_ANALYSIS.md で透明性が確保できているなら不要かもしれない。Phase 12 のセキュリティ判断 (Option A〜D) と連動して検討。

**関連**: SECURITY_GAP_ANALYSIS.md §5 Option A〜D

---

## P2 タスク (中優先)

### ✅ P2-A. Use Case / Scenarios 文書 (4+1 view §5) — **完了 (2026-05-02)**

**成果物**: [`../use_cases/`](../use_cases/) ディレクトリ新設 (5 ファイル)
- `README.md` — 索引 + 設計原則
- `actors.md` — 6 アクター (運用者 / 設置者 / 保守者 / 不正侵入者 / Spresense / PC viewer)
- `use_case_overview.puml` (+ .png) — UML use case diagram
- `primary_use_cases.md` — UC-1〜7 (起動/ストリーミング/動き検出録画/ファイル管理/切断復旧/設定変更/人手介入)
- `exception_scenarios.md` — ES-1〜5 (構造的天井 #1 / WiFi切断 / USB切断 / PC クラッシュ / 設定誤り)

各 UC は Cockburn 形式 (主アクター/前提/主シナリオ/例外/関連 Q+ADR+QAS+実装) で記述。
要求書 v1.0 §1.1 から use_cases/ への cross-link 追加済。

---

### P2-A (旧 — 完了済み記述, 履歴用)

**目的**: 「誰が・何のために・どう使うか」が文書化されていない。4+1 view の Scenarios が完全欠落。

**作業内容**:
- アクター定義 (運用者 / 設置者 / 不正侵入者 / 監視者)
- 主要ユースケース 5-7 件 (起動 / 録画 / 動き検出時録画 / 切断復旧 / 録画再生 / 設定変更 / 障害時対応)
- 各 UC をシナリオ形式で記述

**新規作成先**: `02_specifications/use_cases/` ディレクトリ新設 + 個別 UC ファイル

**関連**: 要求書 v1.0 Q6 (録画トリガー), Q15 (通知), Q19 (エラー回復)

---

### ✅ P2-B. FMEA テーブル (失敗モード影響解析) — **完了 (2026-05-02)**

**成果物**: [`FMEA.md`](risk_analysis/FMEA.md) 新設 (~280 行)
**実施内容の要点**:
- 28 失敗モードを 4 カテゴリで抽出 (A 構造的天井 5 件 / B ソフト失敗 8 件 / C ハード環境 7 件 / D 運用セキュリティ 8 件)
- 各モードを Severity × Occurrence × Detection (1-10) で採点 → RPN 計算
- 高 RPN 上位 10 件を抽出: A4 (560) / D2 (378) / B3 (300) / C7 (270) / B8 (225) / D3 (224) / D7 (189) / D8 (180) / A2 (168) / A1 (150)
- 既に対応済み (本セッションで開示・整理した) 10 項目を整理
- 緊急対応 (RPN≥200) / 計画的対応 (RPN 100-199) / 監視継続 (RPN<100) でアクション分類
- 観察: 構造的天井が RPN 上位を独占、セキュリティ系が高 RPN、運用系の中位 RPN は保守性侵食の毒

**派生発見**: B3 (自動再接続による品質悪化, RPN=300) は ADR-002 v1.1 で発見済だが、**戦略の根本見直し** (再接続ロジック無効化 or 抑制を検討) の必要性を再認識

---

### P2-B (旧 — 完了済み記述, 履歴用)

**目的**: 個別障害は ADR-002 / health state machine 等に断片的に存在するが、**系統的な FMEA** がない。信頼性 NFR の根拠化。

**作業内容**:
- 障害モード 20-30 件抽出 (構造的天井 #1〜#5 + ソフト失敗 + ハード失敗 + 運用失敗)
- 各モードについて: 影響 (Severity) × 発生確率 (Occurrence) × 検知容易性 (Detection) = RPN 計算
- 高 RPN モードへの対策と現状実装の対応

**新規作成先**: `02_specifications/quality/risk_analysis/FMEA.md`

**関連**: QAS-1, QAS-8, ADR-002 v1.1, gs2200m_health_state_machine.puml

---

### ✅ P2-C. 詳細 Threat Model (STRIDE / DREAD) — **完了 (2026-05-02)**

**成果物**: [`THREAT_MODEL.md`](risk_analysis/THREAT_MODEL.md) 新設 (~340 行)
**実施内容の要点**:
- STRIDE 6 カテゴリ × 14 脅威シナリオを DREAD (D+R+E+A+D, max 50) で採点
- 上位 5 件 (DREAD ≥ 35):
  1. TI-1 (48) MJPEG 盗聴 (Wireshark)
  2. TS-1 (46) PC viewer なりすまし
  3. TI-2 (45) WiFi 認証情報のリポジトリ漏洩 → X-7 と直結
  4. TD-1 (42) TCP 8888 接続スパム DoS
  5. TD-2 (35) tx_buff 枯渇誘発 DoS
- 観察: 「認証なし」起点で TS-1 → TI-1/TI-3/TD-1/TD-2 が連鎖的に脆弱 → **TLS-PSK 1 機構で複数脅威を緩和可**
- Phase 12 緊急対策 3 件:
  1. X-7 WiFi 認証情報リポジトリ分離
  2. LAN 隔離前提の運用文書化 (X-4 ランブックの一部)
  3. TLS-PSK or アプリ層認証 (Phase 12 セキュリティ Option B)
- §1 Scope で「LAN 内が信頼境界」前提を明示、対象外 (公開 NW / WiFi 突破 / 物理改竄) も記述

**派生発見**: TI-2 (WiFi 認証情報リポジトリ漏洩, DREAD 45) は緩和策コストが極小 (X-7 で .gitignore + .example 化のみ) → Phase 12 序盤の最優先タスクに位置付け

---

### P2-C (旧 — 完了済み記述, 履歴用)

**目的**: SECURITY_GAP_ANALYSIS.md に簡易 STRIDE があるが、各脅威について DREAD 等の数値評価が未実施。

**作業内容**:
- 各 STRIDE カテゴリで脅威シナリオ 3-5 件詳述
- DREAD (Damage / Reproducibility / Exploitability / Affected users / Discoverability) で 1-10 採点
- 軽減策の優先順位付け

**新規作成先**: `02_specifications/quality/risk_analysis/THREAT_MODEL.md` または SECURITY_GAP_ANALYSIS.md §3 を拡充

**関連**: SECURITY_GAP_ANALYSIS.md, Phase 12 セキュリティ判断

---

## 計画外タスク (本セッション中に派生, 記録のみ)

### ✅ X-1. RTM v5.0 改訂 — **完了 (2026-05-02)**

**成果物**: `02_specifications/traceability/REQUIREMENTS_TRACEABILITY_MATRIX.md` v5.0
**実施内容**:
- 冒頭に v5.0 改訂方針 + v4.0 の問題点を明示 (架空 KPI / 要求書 Q番号 不参照 等)
- §A Q1-Q25 主軸の正規 trace を新設:
  * A.1 機能要求 trace (Q1-Q15)
  * A.2 非機能要求 trace (Q16-Q19)
  * A.3 ハードウェア/スコープ (Q20-Q25)
  * A.4 構造的天井 #1〜#5 trace
  * A.5 Use Case → 機能仕様 → 実装 trace
  * A.6 ADR → 要求 / Phase trace
- §B 達成/乖離/未達成サマリ:
  * ✅ 完全達成 12 件
  * 🟡 乖離あり 4 件 (Q1/Q3/Q5/Q22)
  * 🟡 部分達成 5 件 (技術負債: X-5a-e)
  * 🔴 未達成/構造的不可 5 件 (Q16Want/Q17/Q19/Q24/Q25)
- §C Phase 12 引継ぎ事項 (緊急/計画的/監視継続/完了済)
- §D 以降は v4.0 内容を legacy / 参考資料として保持 (架空 KPI 含むことを警告付きで)

---

### X-1 (旧 — 完了済み記述, 履歴用)

**理由**: 要求書 v0.1 → v1.0 確定により、RTM (要求トレーサビリティマトリクス) v4.0 の前提が変わった。各 Q の確定値と仕様/実装/テストの紐付けを再構築する必要。

**対象**: `02_specifications/traceability/REQUIREMENTS_TRACEABILITY_MATRIX.md` (現 v4.0, 800 行)

**規模**: 中〜大 (要求 25 件 × 仕様 × 実装 × テストのマトリクス更新)

---

### ✅ X-2. MASTER_ROADMAP_2026 v2.0 改訂 — **完了 (2026-05-03)**

**成果物**: `05_future_actions/master_roadmap/MASTER_ROADMAP_2026.md` v2.0
**実施内容**:
- 冒頭に v2.0 改訂方針 + Phase 番号再割り当てマップ追記
- 旧 Phase 10 → Phase 13 (AI統合・マルチカメラ) 再割り当て
- 旧 Phase 11 → Phase 14 (プラットフォーム化) 再割り当て
- 旧 Phase 12 → Phase 15 (商用化・事業拡大) 再割り当て
- 新規 Phase 12 = Tier 移行 + セキュリティ判断 + 残課題対応 を追加
- エグゼクティブサマリーを v2.0 内容に更新
- 進捗表を Phase 10 (✅完了) / Phase 11 (🟡仕様策定) / Phase 12 (📋計画策定中) / Phase 13-15 (💭構想中) に再構成
- §3.2 / §4.1 / §4.2 のセクション見出しに「旧Ph10」等の注記追加
- mermaid gantt chart の Phase 番号修正
- リスク管理 / リソース計画 / KPI 表内の Phase 番号修正

連動更新:
- GLOSSARY.md §1: 旧定義の解消ステータスに更新、Phase 12 と Phase 13/14/15 の項目追加

---

### X-2 (旧 — 完了済み記述, 履歴用)

**理由**: 本セッションで Phase 10/11 定義の競合を「制御工学系を正規」と確定した (GLOSSARY §1)。`MASTER_ROADMAP_2026.md` の Phase 10 (AI統合・マルチカメラ) / Phase 11 (プラットフォーム化) は **Phase 番号再割り当て**が必要。

**対象**: `05_future_actions/master_roadmap/MASTER_ROADMAP_2026.md`

**規模**: 大 (ロードマップ全体の再構成)

**注意**: GLOSSARY.md §1 で「将来 v2.0 改訂で再割り当て予定」と既に予告済

---

### ✅ X-3. 個別 functional/ 仕様書 7 件の整合性チェック — **完了 (2026-05-03)**

**成果物**: [`FUNCTIONAL_SPEC_AUDIT.md`](FUNCTIONAL_SPEC_AUDIT.md) (~150 行) + 各 SPEC への警告バナー追加

**監査結果サマリ**:
- 🔴 重大乖離 2 件: ADAPTIVE_CONTROL_SPEC, SECURITY_SPEC, TEST_COVERAGE_ENHANCEMENT_SPEC
- 🟡 部分乖離 4 件: CAMERA_CAPTURE, CONTROL_ENGINEERING (Phase 番号誤参照), RECORDING (未実装機能), STREAMING (Phase 11 機能誤主張)

**実施したアクション**:
- 7 SPEC 全件に警告バナーを冒頭に追加 (FUNCTIONAL_SPEC_AUDIT.md / SECURITY_GAP_ANALYSIS / GLOSSARY 等への cross-link)
- 各 SPEC の本文は変更せず (内容自体は設計意図として価値があるため)
- 派生候補タスク識別:
  - Phase 11 .c 実装 or 仕様削除判断 (FMEA B8)
  - RECORDING SPEC を要求書 v1.0 整合化 (X-5a-d 連動)
  - セキュリティ Option A〜D 判断 (Phase 12)
  - **テストカバレッジ実測 + ベースライン確立** (新規 X-8 候補)

---

### X-3 (旧 — 完了済み記述, 履歴用)

**理由**: 要求書 v1.0 確定により、`functional/` 配下 6 件 (CAMERA_CAPTURE / STREAMING / RECORDING / ADAPTIVE_CONTROL / CONTROL_ENGINEERING / SECURITY) との整合性が変化している可能性。

**対象**: `02_specifications/functional/*.md` 6 件

**規模**: 中 (各仕様書を要求書 v1.0 に対して読み合わせ)

---

### ✅ X-4. 運用ランブック (Operations Runbook) — **完了 (2026-05-02)**

**成果物**:
- `docs/security_camera/07_operations/` ディレクトリ新設
- [`07_operations/README.md`](../../07_operations/README.md): ディレクトリ目的 + 索引
- [`07_operations/RUNBOOK.md`](../../07_operations/RUNBOOK.md) (~340 行): 9 セクション構成

**RUNBOOK.md の構成**:
- §0 事前確認 (LAN 隔離前提など 4 項目)
- §1 症状チェックリスト (S1〜S8)
- §2 トリアージ (4 質問のフロー)
- §3 軽症障害復旧 (7 手順): PC viewer 起動 / 接続確立 / TCP Health 観察 / RECONNECTING / クラッシュ / シリアルログ解析 / 録画問題
- §4 中重症復旧 (4 手順): Spresense 強制再起動 / WiFi 再接続 / FAILED 状態 / Tier C 暫定切替
- §5 環境問題 (3 手順): ストレージ / CPU 過負荷 / LAN 環境
- §6 エスカレーション (Tier 移行判断条件: 1日3回以上 FAILED / 常時 500ms 超 / Full HD 要求 / ハード故障)
- §7 事後対応 (記録テンプレート)
- §8 連絡先・エスカレーション体制 (現状: 単一開発者、本番運用時 TODO)
- §9 関連文書

**関連** クロスリンク追加:
- UC-7 (primary_use_cases.md) 関連実装欄から RUNBOOK へ
- exception_scenarios.md §検知手段サマリから RUNBOOK へ

---

### X-4 (旧 — 完了済み記述, 履歴用)

**理由**: ADR-002 v1.1 の発見 (再接続 5 回失敗で FAILED 固着) に対応する**人手介入手順**が未文書化。QAS-8 で「未定義」と明記済。

**作業内容**:
- Spresense 再起動手順
- GS2200M リセット手順
- Tier C (USB-only) 切替手順
- 連絡先・エスカレーション体制

**新規作成先**: `07_operations/RUNBOOK.md` (新設ディレクトリ)

**関連**: QAS-8, ADR-002 v1.1, 構造的天井 #1

---

### X-5. 機能仕様の未実装項目への対応

要求書 v1.0 §9 で「⚪ 未達成 / 未検証」と確定したもの:

| ID | 項目 | 関連 Q | 規模 | 状態 |
|---|---|---|---|---|
| ✅ X-5a | ストレージ ローテーション (1GB 上限到達時の古いファイル削除) | Q8 | 中 | **実装済 (Rust_ws c737f3b)** |
| ✅ X-5b | 録画ファイルの時間分割 (1 時間ごとなど) | Q9 | 小〜中 | **実装済 (Rust_ws c737f3b)** |
| ✅ X-5c | アプリ内再生 UI (egui で MP4 再生) | Q11 | 中 | **実装済 (Rust_ws): RecordingBrowser + 外部プレーヤー起動** |
| ✅ X-5d | OSD 重畳 (映像にタイムスタンプ書き込み) | Q14 | 中 | **実装済 (Rust_ws c737f3b)** |
| ✅ X-5e | Windows ネイティブビルド検証 | Q21 | 小 | **CI 準備完了 (windows-latest job 追加)** |
| ✅ X-5f | 屋外 / 温度範囲 / 24h 連続稼働ストレステスト | Q25 | 大 | **計画完了** ([STRESS_TEST_PLAN.md](../../07_operations/STRESS_TEST_PLAN.md)) |

**X-5c/e/f 追加メモ (2026-05-04)**:
- **X-5c**: `recording_browser.rs` 新設 (~280 行) — egui SidePanel で録画一覧 / 外部プレーヤー起動 / 削除。クロスプラットフォーム launch (xdg-open / open / start)。3 unit tests passed。`gui_main.rs` でトグル + 統合
- **X-5e**: `.github/workflows/ci.yml` に `windows-build` job 追加 (windows-latest, GUI feature 含む build + test). 既存 `WINDOWS_BUILD.md` (327 行) と組合わせ運用
- **X-5f**: `07_operations/STRESS_TEST_PLAN.md` 新設 (~250 行) — 8 試験 (ST-1〜8) の手順 + 計測指標 + 結果テンプレート + 機材リスト + リスク管理

**X-5a/b/d 実装メモ (2026-05-04)**:
- Rust_ws リポジトリ commit `c737f3b` で実装
  * `ui_tokens.rs` 新設 (Design System トークン + paint_hud + ConnState)
  * `mp4_recorder.rs` 拡張: `RecordingPolicy` + `RecordingManager`
    (X-5a 容量上限ローテーション + X-5b 時間/バイト分割)
  * `gui_main.rs` 統合: apply_visuals + paint_hud オーバーレイ
  * `ring_buffer.rs::iter_frames()` 追加 (pre-existing build break 修正)
- 並行して外部ハンドオフ "Spresense Security Camera Design System.zip"
  のダークテーマ + §文言 (信号なし · NO SIGNAL 等) を適用
- `cargo check --features gui --bin security_camera_gui` 通過 (44 warnings)
- 実 GUI 動作テストは Phase 12 で実機検証

**関連**: TECHNICAL_DEBT_REGISTER.md と一部重複の可能性 → 統合管理候補

---

### ✅ X-10. Spresense apps repo + NuttX upstream API drift 解消 — **完了 (2026-05-14, 方針 A 採用)**

**実施結果 (phase12-firmware ブランチ)**:
- ✅ 3 件の API drift を patch ファイル化 (`tools/spresense_patches/0001-Patch-upstream-API-drift-for-newer-NuttX-compatibili.patch`)
- ✅ defconfig も含めた一括適用スクリプト整備 (`tools/spresense_patches/apply.sh`, 冪等性あり)
- ✅ `nuttx.spk` (258 KB) ビルド成功、`nm` で perf_thread_cpu_* 3 シンボル (init/log/sample) 確認済
- ✅ spresense submodule は **upstream クリーンミラー** (master = `e9a4f170`) を維持

**改訂方針 (2026-05-14 ユーザー判断)**:
spresense submodule は Sony 公式 fork のため、原則 **改変しない**。Phase 12 ビルドに必要な custom defconfig + apps repo patches は **本リポジトリの `tools/spresense_patches/` で管理** し、`apply.sh` で submodule の working tree にコピー/適用する (commit はしない)。

**修正内容 (patch)**:
1. **`struct file_struct.fs_fd` 不在** (NuttX 側で削除された)
   - 修正箇所: `spresense/sdk/apps/system/readline/readline.c`, `spresense/sdk/apps/system/cle/cle.c`
   - 修正: `instream->fs_fd` → `fileno(instream)` (POSIX 標準)
2. **`IFF_DOWN` マクロ未定義** (nuttx に `IFF_UP` のみ定義)
   - 修正箇所: `spresense/sdk/apps/netutils/netlib/netlib_setifstatus.c`
   - 修正: `req.ifr_flags |= IFF_DOWN;` → `req.ifr_flags &= ~IFF_UP;`
3. **SPI5 DMAC 未有効化** (cxd56_gs2200m.c が要求)
   - 追加: `CXD56_SPI5=y` / `CXD56_DMAC_SPI5_TX=y` / `CXD56_DMAC_SPI5_RX=y` を defconfig に明記

**ビルド手順 (phase12-firmware ブランチ)**:
```bash
git checkout phase12-firmware
./tools/spresense_patches/apply.sh   # defconfig コピー + patch 適用 (冪等)
cd spresense/sdk
./tools/config.py examples/security_camera
make                                  # nuttx.spk が生成される
```

**ブロック解消したタスク**:
- ✅ Phase 12.1 X-6 (CPU 実測) — perf_thread_cpu_* シンボル組込済 nuttx.spk が用意できた
- 🔵 Phase 12.1 X-5f ST-1 24h 連続稼働 — ビルド前提条件は解消、実機書込待ち
- 🔵 Phase 12.3 Step B-2 (PSK 認証) — 同上

**関連**:
- `tools/spresense_patches/README.md` (phase12-firmware ブランチ): 詳細手順 + 検証コマンド
- 旧記述 (方針 A/B/C 比較) は git log で参照可: `git show <prev-commit>:docs/.../PENDING_NFR_WORK.md`

---

### ✅ X-9. Spresense ファームウェア リビルド経路の整備 — **defconfig 整備完了 (2026-05-09)**

**実施内容 (本セッション)**:
- `spresense/sdk/configs/examples/security_camera/defconfig` 新規作成 (40 エントリ)
  * 標準 camera 例 (LCD 系) からの差分: -EXAMPLES_CAMERA / -LCD_* 等を除外
  * 必須追加: CDCACM (USB CDC-ACM) + EXAMPLES_SECURITY_CAMERA + VIDEO_ISX012
  * WiFi 系: WIRELESS_GS2200M / WL_GS2200M / NET_TCP_NO_STACK / NET_USRSOCK_TCP / NETUTILS_DHCPC
  * 構造的天井 #2 設定: IOB_NBUFFERS=8 / IOB_BUFSIZE=196 (現状値を defconfig に明示化)
  * SPI クロック: WL_GS2200M_SPI_FREQUENCY=4000000 (4 MHz, 構造的天井 #1)
- `./tools/config.py --list` で `examples/security_camera` が認識されることを確認 ✅
- `./tools/config.py examples/security_camera` で defconfig 読込成功 ✅
- README.md 更新: 推奨手順 (defconfig 経由) + 代替 (menuconfig) を併記

**調査結果**:
過去に nuttx.spk が生成できていた経緯は **README §1 の代替手順 (menuconfig 経由)** で `.config` を手動構築していた可能性が高い。spresense submodule 上に security_camera defconfig がコミットされた痕跡なし → 「使ってなかったから無かった」が結論 (2026-05-09 ユーザー確認)。

**残課題**:
- ✅ `kconfig-frontends` 導入: 本セッションで .deb 展開 → `$HOME/.local` 配置 → 動作確認済 (sudo 不要回避策)
- ✅ defconfig 適用: `./tools/config.py examples/security_camera` 通過確認、`NET_IPv4` / `NET_USRSOCK_UDP` 追加で gs2200m driver も通過
- ✅ X-6 perf_logger.c の `syslog(LOG_INFO,...)` バグ修正 (LOG_INFO マクロとの衝突解消)
- 🔴 **完全 make は未到達**: spresense submodule の apps repo と NuttX upstream の API drift で 3 件以上のエラー
  → **X-10 として別タスク化** (上記参照)
- CI build-only ジョブの追加 (Phase 12 後半 or Phase 13)

---

### X-9 (旧 — 完了済み記述, 履歴用)

**理由**: 2026-05-09 の起動準備確認時に判明 — `tools/config.py examples/security_camera` 実行で `RuntimeError: Config "examples/security_camera" not found` が発生。本リポジトリには `apps/examples/security_camera/Kconfig` は存在するが、Spresense SDK の標準 defconfig 登録 (`tools/config.py --list` の出力対象) には**含まれていない**。

**再現条件**:
1. `cd spresense/sdk && make distclean` で `nuttx.spk` が消失
2. `./tools/config.py examples/security_camera` でリコンフィグしようとしても上記エラー
3. 結果として X-6 計装 (perf_thread_cpu_*) を反映した nuttx.spk のリビルドが不可

**現状回避策 (2026-05-09 適用済)**:
- `/home/ken/Spr_ws/spresense_v3.0.0_backup/sdk/nuttx.spk` (Spresense v3.0.0 当時の生成物) を `spresense/sdk/nuttx.spk` に手動コピー
- これは **X-6 計装なしの旧版**。基本動作テストには使えるが、CPU 利用率実測 (Phase 12.1) には使えない

**作業内容**:
1. **defconfig 探索/作成**: `spresense/sdk/configs/examples/security_camera/defconfig` を新設、`tools/config.py examples/security_camera` で読み込めるようにする
2. **再現可能なビルド手順**:
   ```
   cd spresense/sdk
   ./tools/config.py examples/security_camera   # ← これが通るように整備
   make
   # 結果: nuttx.spk が生成され、X-6 計装 (perf_thread_cpu_*) が含まれる
   ```
3. **CI 上の build-only ジョブ追加** (実機書込なしで Spresense 側ビルド検証)
4. **README / WINDOWS_BUILD.md の更新** で再現手順を文書化

**規模**: 中 (defconfig 作成 + Makefile 整備 + CI ジョブ + 文書化)

**ブロックする他タスク**:
- Phase 12.1 X-6 (CPU 実測): nuttx.spk の X-6 計装版が無いと実測不可
- Phase 12.1 X-5f ST-1: 同 fw を必要とする
- Phase 12.3 Step B-2 (PSK 認証実装): 実装後の動作確認に新 fw が必要

**関連**:
- アプリ本体: `apps/examples/security_camera/`
- バックアップ参照: `/home/ken/Spr_ws/spresense_v3.0.0_backup/`
- Spresense SDK ツール: `spresense/sdk/tools/config.py`
- 計装 commit: `ac7dfa7` (X-6 perf_thread_cpu_* 追加)

---

### ✅ X-7. WiFi 認証情報のリポジトリ分離 — **完了 (2026-05-02)**

**実施内容**:
- `wifi_config.h.example` 新設 (placeholder 値: YOUR_SSID_HERE / YOUR_PASSWORD_HERE) + セキュリティ注意書き
- `.gitignore` に `apps/examples/security_camera/wifi_config.h` 追加
- `git rm --cached wifi_config.h` で track 外し (ローカル実体は保持)
- `apps/examples/security_camera/README.md` に §0 セットアップ手順追記 (cp + 編集) + 警告
- THREAT_MODEL.md TI-2 (DREAD 45) と相互参照

**git history サニタイズ判断 (2026-05-02 ユーザー確定)**:
✅ **Option C: 何もしない (history は現状維持)**

**判断根拠**:
- 現状の WiFi AP は **ノート PC の AP 機能** で、検証中のみ有効化される運用
- 本番運用開始前なので、漏洩 history があっても実害は限定的
- 今後の本番運用は `.gitignore` 化された新運用フローで開始するため、漏洩リスクは未来に持ち込まれない

**本番運用移行時の TODO** (Phase 12+ で再評価):
- 本番 AP 切替時に WIFI_PASSWORD を必ず新規発行 (history 漏洩済の旧 password を再使用しない)
- 公開リポジトリ化前に再度 X-7 残課題として履歴サニタイズを再検討

---

### X-7 (旧 — 完了済み記述, 履歴用)

**理由**: P1-B 調査で判明 — `apps/examples/security_camera/wifi_config.h` に **WiFi SSID / Password がハードコード** され、git track されている。リポジトリが公開されるとセキュリティリスク。

**作業内容**:
- `wifi_config.h` を `wifi_config.h.example` にリネーム (テンプレート化)
- 実体 `wifi_config.h` を `.gitignore` 追加
- 既にコミット済の git history から認証情報を除去 (`git filter-repo` 等, 慎重に判断)
- README に setup 手順追記 (`cp wifi_config.h.example wifi_config.h` + 編集)

**規模**: 小〜中 (history サニタイズの判断が伴う)

**関連**: SECURITY_GAP_ANALYSIS.md (本件は実装乖離ではなく「設定管理の問題」), CROSS_CUTTING_CONCERNS.md §3

---

### ✅ X-8. テストカバレッジ実測 + ベースライン確立 — **Phase 12.1 Stage 1 完了 (2026-05-08)**

**Stage 1 実測結果 (2026-05-08)**:
- cargo-llvm-cov v0.8.5 + llvm-tools-preview インストール
- `cargo llvm-cov --features gui --summary-only` 実行成功
- **総合行カバレッジ: 16.75%** (3510 行中 2922 未カバー)
- 健全: motion_detector 95.71% / ring_buffer 89.12%
- 0% 帯: gui_main 1151 行 / mp4_recorder 225 行 / ui_tokens 155 行
- → TEST_COVERAGE_ENHANCEMENT_SPEC v1.0「92%」主張は実測 75pt 乖離と確定

**Stage 1 副次成果**:
- CI workflow: 旧 cargo-tarpaulin job を **cargo-llvm-cov** に切替 (Codecov v4)
- カバレッジ後退検出 (baseline 16.0% threshold) を CI に追加
- TEST_COVERAGE_BASELINE.md v1.1 で Stage 4 数値目標を実測ベースに再設定 (1ヶ月 25% / 3ヶ月 40%)

**Phase 12.1 Stage 1 後続改善 (2026-05-08)**:
- ui_tokens.rs に 11 単体テスト追加 (ConnState dot_color/label, 4px scale, 色階層 等)
- 総合カバレッジ **16.75% → 19.41%** (+2.66pt)、ui_tokens **0% → 44.14%**
- 1ヶ月目標 25% への進捗 33%

**残り Stage 2-3** (Phase 13 候補):
- Stage 2 PC viewer モジュール分割 (gui_main 1151 行 → 個別 ui::* モジュール)
- Stage 3 Spresense Pure-Logic 抽出 + host build (mjpeg_protocol / fps_controller / frame_statistics)

---

### ✅ X-8. テストカバレッジ実測 + ベースライン確立 (旧 — 2026-05-05 ベースライン確立, 2026-05-08 Stage 1 実測完了で更新)

**成果物**: [`TEST_COVERAGE_BASELINE.md`](performance/TEST_COVERAGE_BASELINE.md) (~250 行)

**実測ベースライン (2026-05-05)**:
- PC viewer (Rust): **31 test 関数** (35 pass / 0 fail / 3 ignored)
  * cargo test --features gui で全件成功
  * テスト密度: protocol/motion/ring 系は高 (1.0+)、gui_main 1815 行は test 0
- Spresense アプリ (C): **0 test 関数**
  * NuttX 内蔵 unit test framework なし、ハード依存で host build 困難

**改善計画 (Phase 12 以降)**:
- Stage 1: cargo-llvm-cov 導入 → CI で行カバレッジ自動測定
- Stage 2: gui_main.rs 分割 (1815 行 → 5+ モジュール)
- Stage 3: Spresense Pure-Logic 抽出 (mjpeg_protocol / fps_controller / frame_statistics)

**新数値目標**: 1 ヶ月で PC 30% / 3 ヶ月で 50% (旧 92% は撤回)

**関連 X-3 派生**: TEST_COVERAGE_ENHANCEMENT_SPEC.md v1.0 の「92%」を本書ベースに改訂が必要 (Phase 12)

---

### X-8 (旧 — 完了済み記述, 履歴用)

**理由**: X-3 監査で判明 — TEST_COVERAGE_ENHANCEMENT_SPEC.md の「カバレッジ 92%」が未計測値であり、QUALITY_REQUIREMENTS §7.5 でも ⚪ 未計測と判定済。

**作業内容**:
- 単体テストの存在確認 (現状 Spresense 側はテスト無し疑い)
- gcov / llvm-cov 等でカバレッジ計測機構を導入
- ベースライン確立 → 改善目標を再設定
- TEST_COVERAGE_ENHANCEMENT_SPEC.md を実測値ベースで改訂

**規模**: 中〜大 (テストフレームワーク導入が必要なら大)

---

### ✅ X-6. CPU 予算の実測手段の確立 — **実装準備完了 (2026-05-03)**

**成果物**:
- コード: `apps/examples/security_camera/perf_logger.{h,c}` に per-thread CPU 計測 API 追加
  * `perf_thread_cpu_init/sample/log` 3 関数
  * `perf_thread_cpu_t` 構造体 (cpu_percent / avg / max / sample_count)
  * 採用方式: `clock_gettime(CLOCK_THREAD_CPUTIME_ID)` × `CLOCK_MONOTONIC` の比率
- 計装: `apps/examples/security_camera/camera_threads.c`
  * camera_thread (30 frames 周期 = ~1 sec)
  * usb_thread (30 frames 周期)
  * control_thread (10 cycles 周期 = ~1 sec @ 10Hz)
- ガイド: [`../../07_operations/CPU_MEASUREMENT_GUIDE.md`](../../07_operations/CPU_MEASUREMENT_GUIDE.md) (~200 行)
  * §1 計測アーキテクチャ (clock_gettime ベース採用理由)
  * §2 ビルド・書込手順
  * §3 計測実施シナリオ (A/B/C)
  * §4 ログ集計 (parse_cpu_log.py 連動)
  * §5 計測結果の文書反映フロー
  * §6 トラブルシューティング
- 集計スクリプト: `scripts/cpu_measurement/parse_cpu_log.py`
  * syslog の `[CPU]` 行を正規表現抽出
  * 平均/最大/合計を集計、CSV 出力対応
  * smoke test 通過済

**残課題 (ハードウェア実行が必要)**:
- 実機での実測実施 (Phase 12 序盤推奨)
- 結果を CPU_BANDWIDTH_BUDGET §2 / QUALITY_REQUIREMENTS §2.2 に反映
- gs2200m driver task (kernel) の CPU 利用率は本実装の対象外 (要別手段, 例: CONFIG_SCHED_CPULOAD=y + `top`)

---

### X-6 (旧 — 完了済み記述, 履歴用)

**理由**: P1-A の前提として、Cortex-M4F × 6 の CPU 利用率を実測する手段が未確立。

**作業内容**:
- NuttX の `top` 相当機能の有効化 (`CONFIG_FS_PROCFS` + `CONFIG_FS_PROCFS_REGISTER`)
- perf_logger 拡張で per-thread CPU time 計測
- 計測結果を SPRESENSE_TCP_CONSTRAINTS に追記

**規模**: 中 (実装 + 計測ループ)

---

## 🚀 Phase 12 確定方針 (2026-05-05 ユーザー判断)

**Phase 12 = Tier 1 維持 + 家庭用 運用品質確立**

新規ハード導入なし + 本番ターゲット家庭用 が確定したことにより、本台帳の各タスクは以下のように再分類される:

| 分類 | タスク | 取扱 |
|---|---|---|
| **Phase 12 序盤 必須** | X-6 実機計測, X-5f ST-1, X-8 Stage 1 | 実測 3 件, Phase 12.1 で実施 |
| **Phase 12.2 戦略判断** | ADR-002 v1.2 (Auto-Reconnect 見直し) | 12.1 実測ベース |
| **Phase 12.3 段階セキュリティ** | Option B 実装 (PSK / IP allowlist / ログ署名) | LAN 隔離前提を運用文書化 |
| **Phase 12.4 撤回 (推奨)** | Phase 11 .c 実装 (FMEA B8) | 家庭用に不要、obsolete マーク |
| **Phase 12.5 仕様確定** | 要求書 v1.1 (本日 commit), PRODUCTION_DEPLOYMENT_CHECKLIST 新規 | WONT FIX 恒久化 |
| **Phase 13 以降** | X-5f ST-3〜8 (温度/Jamming等), X-8 Stage 2-3, Tier 移行検討 | 家庭用想定では Phase 12 では不要 |

詳細: [`../../05_future_actions/phase_planned/Phase12_実施計画書.md`](../../05_future_actions/phase_planned/Phase12_実施計画書.md)

---

## 取り組み優先度の提案 (Phase 12 確定前の旧表 — 履歴用)

### 即時着手候補 (Phase 12 序盤)
- **P1-A** CPU/帯域予算 — 数値根拠が定量化されると Tier 移行判断の精度向上
- **X-4** 運用ランブック — 現状の運用ギャップを最小コストで埋める
- **X-1** RTM v5.0 — 要求書 v1.0 確定の論理的続編

### 中期 (Phase 12〜13)
- **P1-B** Cross-cutting Concerns
- **P2-A** Use Case / Scenarios
- **X-3** functional/ 仕様書整合性チェック

### Phase 12 セキュリティ判断と連動
- **P1-C** セキュリティ仕様分離
- **P2-C** 詳細 Threat Model (STRIDE/DREAD)

### Phase 13 以降または Tier 移行と同時
- **P2-B** FMEA
- **X-2** MASTER_ROADMAP_2026 v2.0
- **X-5** 個別未実装項目 (a〜f)
- **X-6** CPU 予算実測手段

---

## 関連ドキュメント

- 完了済: [`README.md`](README.md), [`GLOSSARY.md`](GLOSSARY.md), [`QUALITY_REQUIREMENTS.md`](QUALITY_REQUIREMENTS.md), [`QUALITY_ATTRIBUTE_SCENARIOS.md`](QUALITY_ATTRIBUTE_SCENARIOS.md), [`SECURITY_GAP_ANALYSIS.md`](risk_analysis/SECURITY_GAP_ANALYSIS.md)
- 既存台帳: [`../../05_future_actions/technical_debt/TECHNICAL_DEBT_REGISTER.md`](../../05_future_actions/technical_debt/TECHNICAL_DEBT_REGISTER.md) (12 件登録済)
- 要求書: [`../../01_requirements/FUNCTIONAL_REQUIREMENTS.md`](../../01_requirements/FUNCTIONAL_REQUIREMENTS.md) v1.0 §10 「次のステップ」

---

## 改訂履歴

| バージョン | 日付 | 変更内容 |
|---|---|---|
| 1.0 | 2026-05-01 | 初版。本セッション (P0 ドキュメント整備) で判明した P1 (3 件) / P2 (3 件) / 計画外 (6 件) を集約 |
