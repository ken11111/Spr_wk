# FMEA — Failure Mode and Effects Analysis (失敗モード影響解析)

**バージョン**: 1.0
**作成日**: 2026-05-02
**準拠**: IEC 60812 (FMEA) + arc42 §11 (Risk and Technical Debt) を参考に簡略化
**目的**: ソフト/ハード/運用の失敗モードを系統的に抽出し、Severity × Occurrence × Detection で RPN 採点。高 RPN モードへの対策状況を可視化する
**位置付け**: P2-B タスク (PENDING_NFR_WORK.md)

> **位置付けの明確化**:
> - [`SECURITY_GAP_ANALYSIS.md`](SECURITY_GAP_ANALYSIS.md): セキュリティ脅威の設計-実装乖離 (悪意ある攻撃)
> - [`exception_scenarios.md`](../use_cases/exception_scenarios.md): 個別 UC レベルの異常系 (ES-1〜5)
> - **本書 (FMEA)**: 上記を網羅し、**システム全体の失敗モード**を採点 + 優先順位付け

---

## §1 採点基準 (Scoring Criteria)

### Severity (S, 1-10): 影響度

| S | 説明 |
|---|---|
| 1-2 | 機能継続、ユーザー無自覚 (内部メトリクスのみ) |
| 3-4 | 機能劣化、ユーザー自覚するが運用継続可 |
| 5-6 | 部分機能停止 (例: 録画のみ停止、表示は継続) |
| 7-8 | 全機能停止 + 自動 or 短時間で復旧可能 |
| 9-10 | 全機能停止 + 復旧に人手 / データ消失 / セキュリティ侵害 |

### Occurrence (O, 1-10): 発生頻度

| O | 説明 |
|---|---|
| 1-2 | 極めて稀 (年 1 回未満) |
| 3-4 | 稀 (月 1 回未満) |
| 5-6 | 時々 (週 1 回程度) |
| 7-8 | 頻繁 (日 1 回以上) |
| 9-10 | 常時 / 構造的に必発 |

### Detection (D, 1-10): 検知困難度 (10 = 検知不可)

| D | 説明 |
|---|---|
| 1-2 | 自動検知 + 即座にアラート (ユーザー通知) |
| 3-4 | 自動検知 + 一定遅延でログ出力 |
| 5-6 | ログ出力のみ、運用者が能動的に確認すれば気付く |
| 7-8 | 計測しているが解析が必要 (CSV / metrics packet 等) |
| 9-10 | 検知手段なし、外部から指摘されて初めて判明 |

### RPN = S × O × D (max 1000)

**閾値**:
- 🔴 RPN ≥ 200: 緊急対策推奨
- 🟡 RPN 100-199: 計画的対応
- 🟢 RPN < 100: 監視継続

---

## §2 FMEA テーブル (28 失敗モード)

> 以下 4 カテゴリで分類: **A. 構造的天井起点** / **B. ソフト失敗** / **C. ハード/環境** / **D. 運用/セキュリティ**

### A. 構造的天井起点 (5 件)

| ID | 失敗モード | 影響 | S | O | D | RPN | 現対策 | 関連 |
|---|---|---|---|---|---|---|---|---|
| **A1** | GS2200M `tx_buff[1]` 直列化による送信遅延 (構造的天井 #1) | TCP 平均 134 ms (Phase 8), 100ms Want 達成不可 | 5 | 10 | 3 | **150** 🟡 | TCP Health Monitor で計測 (緩和策はなし、Tier 移行のみ) | ADR-002 v1.1, QAS-2, ES-1 |
| **A2** | NuttX IOB プール (1568B) 枯渇 (構造的天井 #2) | TCP/UDP 並行で内部キューイング不可、高負荷時に send 失敗 | 6 | 4 | 7 | **168** 🟡 | IOB_THROTTLE=0 のためフロー制御なし。Tier 移行のみ | GLOSSARY §2 #2 |
| **A3** | RAM 1.5 MB 枯渇 (構造的天井 #4) | Phase 11 多変数制御 + TLS スタック実装不可 | 8 | 8 | 2 | **128** 🟡 | メモリ予算で 60% 利用済、設計時に予防 | SPRESENSE_TCP_CONSTRAINTS §2 |
| **A4** | GS2200M 内部バッファ非公開 (構造的天井 #5) | チューニング不可、ベンダー BB | 7 | 10 | 8 | **560** 🔴 | 改変不能。Tier 2/3 移行が唯一の解決策 | GLOSSARY §2 #5 |
| **A5** | Full HD 30 fps 帯域不足 (1830 KB/s vs SPI 500 KB/s) | Q1 暫定回答 A 物理的に達成不可 | 7 | 10 | 1 | **70** 🟢 | 要求書 v1.0 §3.3 で明示。Tier 2/3 で解決 | CPU_BANDWIDTH_BUDGET §3.4 |

### B. ソフト失敗 (8 件)

| ID | 失敗モード | 影響 | S | O | D | RPN | 現対策 | 関連 |
|---|---|---|---|---|---|---|---|---|
| **B1** | TCP 切断 (broken pipe) → 自動再接続 | UC-2 ストリーミング短時間停止 (~3 秒) | 5 | 7 | 2 | **70** 🟢 | Auto-Reconnect FSM (max=5, exp backoff) | UC-5, ADR-002 v1.1 |
| **B2** | 自動再接続 5 回失敗 → FAILED 固着 | サービス長時間停止、人手介入必要 | 9 | 4 | 3 | **108** 🟡 | TCP Health Monitor で検知、ただし運用ランブック未文書化 (X-4) | QAS-8, UC-7 |
| **B3** | 自動再接続による品質悪化 (ADR-002 v1.1) | PC FPS 6.74→2.77 (-59%), ドロップ 53.7%→74.2% | 6 | 10 | 5 | **300** 🔴 | 設計が逆効果と判明、Tier 移行 + 戦略再考必要 | ADR-002 v1.1, QAS-1 |
| **B4** | action_queue overflow (camera 生成 > usb 消費) | 古いフレームをドロップ | 4 | 9 | 4 | **144** 🟡 | 動的深度調整 5/7/9, ドロップ counter で観測 | QAS-10, UC-2 例外 |
| **B5** | JPEG パケットサイズ overflow (>60KB) | mjpeg_pack_frame 失敗、フレーム消失 | 4 | 2 | 2 | **16** 🟢 | MJPEG_MAX_JPEG_SIZE=61440 で予防、エラー検知 | mjpeg_protocol.h:31 |
| **B6** | CRC-16 検証失敗 (PC 側受信時) | パケットドロップ → 表示揺らぎ | 3 | 3 | 3 | **27** 🟢 | CRC-16-CCITT 計算実装済、検出時はパケット破棄 | mjpeg_protocol.c |
| **B7** | PC viewer プロセスクラッシュ (panic / OOM) | UC-2 表示停止、Spresense 側は再接続 wait | 7 | 3 | 2 | **42** 🟢 | bounded(3) で防御 (ADR-008)、再接続自動 | ES-4 |
| **B8** | Phase 11 多変数制御 (`.c` 未実装) を有効化試行 | リンクは通るが動作なし、ユーザーが「動いている」と誤解 | 5 | 5 | 9 | **225** 🔴 | L2.C 図と FUNCTIONAL_REQUIREMENTS で実装事実明示済、ただし誤解リスク残存 | L2.C 図注記 |

### C. ハード/環境 (7 件)

| ID | 失敗モード | 影響 | S | O | D | RPN | 現対策 | 関連 |
|---|---|---|---|---|---|---|---|---|
| **C1** | WiFi AP 接続失敗 (起動時, SSID/Pass 誤り) | UC-1 起動失敗 | 8 | 5 | 2 | **80** 🟢 | wifi_manager_connect エラーログ、設置者が `wifi_config.h` 修正 | ES-5 |
| **C2** | WiFi 切断 (運用中, AP 障害 or 電波弱化) | UC-2 全停止、自動再接続 | 7 | 4 | 2 | **56** 🟢 | 自動再 association 実装済 | ES-2 |
| **C3** | USB ケーブル抜去 (USB 路使用時) | USB 経由停止、TCP 路は影響なし | 5 | 6 | 1 | **30** 🟢 | EPIPE/EIO 検知、3 回リトライ + 再 open | ES-3 |
| **C4** | ISX012 カメラ初期化失敗 (ハード) | UC-1 起動失敗 | 9 | 1 | 2 | **18** 🟢 | ladder cleanup でリソース解放 | UC-1 例外 |
| **C5** | SD カード I/O エラー (現状未使用だが将来) | (現状影響なし) | 3 | 2 | 5 | **30** 🟢 | 現状機能なし、将来導入時に再評価 | (未使用) |
| **C6** | PC ストレージ容量不足 | 録画失敗、過去録画も上書きされる可能性 | 7 | 5 | 4 | **140** 🟡 | **1GB 自動ローテーション未実装** (技術負債) | UC-4, Q8 |
| **C7** | 屋外 / 高温環境での動作未検証 (Q25) | 不明 (未テスト) | 6 | 5 | 9 | **270** 🔴 | 屋内のみ検証済、Tier 2/3 移行と同時にストレステスト必要 | Q25, X-5f |

### D. 運用/セキュリティ (8 件)

| ID | 失敗モード | 影響 | S | O | D | RPN | 現対策 | 関連 |
|---|---|---|---|---|---|---|---|---|
| **D1** | WiFi 認証情報の git track (X-7) | リポジトリ公開時に資格情報漏洩 | 9 | 8 | 2 | **144** 🟡 | 発見済、X-7 で .gitignore + .example 化予定 (緊急度高) | X-7, CROSS_CUTTING_CONCERNS §3 |
| **D2** | 同一 LAN 内の MJPEG 盗聴 (Wireshark) | 映像漏洩 | 7 | 6 | 9 | **378** 🔴 | TLS 設計のみ、実装なし。物理 LAN 隔離前提 | SECURITY_GAP_ANALYSIS §3 |
| **D3** | 同一 LAN 内からの不正 TCP 接続 (port 8888) | 認証なし、誰でも映像受信可 | 7 | 4 | 8 | **224** 🔴 | 認証実装なし、LAN 隔離前提 | SECURITY_GAP_ANALYSIS §3 |
| **D4** | 中間者改ざん攻撃 (MJPEG 改竄) | 偽映像挿入 | 6 | 2 | 9 | **108** 🟡 | CRC-16 のみ (容易に再計算可能で防御不十分) | SECURITY_GAP_ANALYSIS §3 |
| **D5** | 運用者 5 回失敗時の介入手順不明 (X-4) | サービス停止延長、運用者がパニック対応 | 7 | 4 | 5 | **140** 🟡 | 運用ランブック未作成 (X-4) | UC-7, QAS-8 |
| **D6** | ログ解析困難 (3 系統混在: LOG_*/_err/syslog) | 障害解析時間増 | 4 | 6 | 5 | **120** 🟡 | CROSS_CUTTING_CONCERNS §1 で識別、統一は未実施 | CROSS_CUTTING_CONCERNS |
| **D7** | dead code (encoder/protocol_handler) によるビルド時間増 + 誤読 | 保守性低下、新参者の混乱 | 3 | 9 | 7 | **189** 🟡 | L2.A/B 図で dead code 注記、削除は未実施 | L2 図注記, P2-A |
| **D8** | Phase 番号定義の意味揺れ (MASTER_ROADMAP vs requirements) | 文書間の不整合、Phase 12 計画混乱 | 5 | 9 | 4 | **180** 🟡 | GLOSSARY §1 で「制御工学系を正規」と確定 (注記付き) | GLOSSARY §1, X-2 |

---

## §3 RPN 順位 (高い順, top 10)

| 順位 | ID | RPN | 失敗モード | カテゴリ |
|---|---|---|---|---|
| 1 | A4 | **560** | GS2200M 内部バッファ非公開 (構造的天井 #5) | 構造的 |
| 2 | D2 | **378** | 同一 LAN 内の MJPEG 盗聴 | セキュリティ |
| 3 | B3 | **300** | 自動再接続による品質悪化 | ソフト失敗 |
| 4 | C7 | **270** | 屋外 / 高温環境での動作未検証 | ハード/環境 |
| 5 | B8 | **225** | Phase 11 (.c 未実装) を有効化試行 | ソフト失敗 |
| 6 | D3 | **224** | 同一 LAN 内からの不正 TCP 接続 | セキュリティ |
| 7 | D7 | **189** | dead code によるビルド/誤読 | 運用 |
| 8 | D8 | **180** | Phase 番号定義の意味揺れ | 運用 |
| 9 | A2 | **168** | NuttX IOB プール枯渇 | 構造的 |
| 10 | A1 | **150** | tx_buff[1] 直列化による送信遅延 | 構造的 |

---

## §4 対策状況サマリ

### 既に対応済み (本セッションで開示・整理)

| ID | 対応 | 担当文書 |
|---|---|---|
| A1〜A5 | 構造的天井として GLOSSARY §2 + SPRESENSE_TCP_CONSTRAINTS で明示 | GLOSSARY, SPRESENSE_TCP_CONSTRAINTS |
| A5 (Full HD) | 要求書 v1.0 §3.3 で達成不可と確定 | FUNCTIONAL_REQUIREMENTS v1.0 |
| B3 (再接続逆効果) | ADR-002 v1.1 で全面改訂 | ADR-002 v1.1 |
| B8 (Phase 11 未実装) | L2.C 図で灰色破線 + ❌ で視覚化 | spresense_main_board_l2c_control.puml |
| D1 (WiFi 認証情報) | X-7 として登録、緊急度高 | PENDING_NFR_WORK X-7 |
| D2/D3/D4 (セキュリティ) | SECURITY_GAP_ANALYSIS で開示 | SECURITY_GAP_ANALYSIS |
| D5 (運用手順未文書化) | X-4 として登録 | PENDING_NFR_WORK X-4 |
| D6 (ログ混在) | CROSS_CUTTING_CONCERNS §1 で識別 | CROSS_CUTTING_CONCERNS |
| D7 (dead code) | L2.A/B 図で注記、QAS-7 で削除タスク化 | QAS-7 |
| D8 (Phase 番号揺れ) | GLOSSARY §1 で正規定義確定 | GLOSSARY §1 |

### 未対応 (Phase 12 以降)

| ID | 必要対策 | 緊急度 | 既存タスク |
|---|---|---|---|
| A4/A5 | Tier 2/3/C 移行判断 | 高 | ADR-006 GATE-1 |
| C6 | 1GB 自動ローテーション実装 | 中 | X-5a, TECHNICAL_DEBT_REGISTER |
| C7 | 屋外/温度ストレステスト | 中 | X-5f, Q25 |
| B2/D5 | 運用ランブック作成 | 中 | X-4 |
| D1 | WiFi 認証情報リポジトリ分離 | 高 | X-7 |
| D2/D3 | セキュリティ実装 (TLS/認証) | 中 | Phase 12 セキュリティ判断 (Option A〜D) |

---

## §5 観察と方針

### 観察 1: 構造的天井が RPN 上位を独占

A4 (560), A2 (168), A1 (150) が構造的天井起点。**RPN を下げる手段はハード移行 (Tier 2/3/C) のみ**で、ソフトチューニングでは解決不可。

### 観察 2: セキュリティ系が高 RPN

D2 (378), D3 (224) が高 RPN。**LAN 隔離前提**を運用上明示しないと現実装は脆弱。SECURITY_GAP_ANALYSIS で開示済だが、**運用文書での前提明記**が必要 (X-4 ランブックの一部に組込推奨)。

### 観察 3: 運用系の中位 RPN

D7 (189) dead code, D8 (180) Phase 番号揺れ, D6 (120) ログ混在。これらは**保守性・解析性**を侵食する低速な毒で、放置すると Phase 12 計画時に支払う。優先度中で順次解消が望ましい。

### 観察 4: 検知困難度 (D) が高いモード

D=9 (検知ほぼ不可): D2 (盗聴), D4 (改ざん), C7 (環境), B8 (Phase 11 偽動作)
- これらは**事前防御で予防するか、外部監査で発見するか**の二択
- 特に D2/D4 はセキュリティ判断 (Phase 12) で扱う

### 観察 5: 発生頻度 (O) が高いモード

O=10 (常時): A1 (tx_buff 律速), A4 (vendor BB), A5 (Full HD 帯域不足)
- **構造的に必発**のため、緩和策ではなく構造変更 (Tier 移行) が必要

---

## §6 結論と次のアクション

### 緊急対応 (RPN ≥ 200)

| ID | 推奨アクション |
|---|---|
| A4 (560) | Phase 12 で Tier 2/3 移行判断 (ADR-006 GATE-1) |
| D2 (378) | 運用文書に「LAN 隔離前提」を明記 + Option B (段階セキュリティ) を Phase 12 で判断 |
| B3 (300) | 自動再接続戦略の見直し (現状ロジック無効化 or 抑制を検討) |
| C7 (270) | 屋外/温度ストレステスト計画 (Tier 移行と同時) |
| B8 (225) | L2.C 図の警告強化を継続 (既に実施済)、Phase 11 .c 実装の判断 |
| D3 (224) | LAN 隔離前提を運用文書化 (D2 と同じアクション) |

### 計画的対応 (RPN 100-199)

D7/D8/D1/A2/D4/D5/D6/B2/C6/A1/B4 — Phase 12-13 で順次。

### 監視継続 (RPN < 100)

A3/A5/B1/B5/B6/B7/C1〜C5 — 現対策で十分、再発時に再評価。

---

## 関連文書

- 制約根拠: [`../architecture/SPRESENSE_TCP_CONSTRAINTS.md`](../architecture/SPRESENSE_TCP_CONSTRAINTS.md)
- 用語集: [`GLOSSARY.md`](GLOSSARY.md) (構造的天井 #1〜#5)
- 品質要求: [`QUALITY_REQUIREMENTS.md`](QUALITY_REQUIREMENTS.md)
- 品質シナリオ: [`QUALITY_ATTRIBUTE_SCENARIOS.md`](QUALITY_ATTRIBUTE_SCENARIOS.md) (QAS-1〜10)
- セキュリティ乖離: [`SECURITY_GAP_ANALYSIS.md`](SECURITY_GAP_ANALYSIS.md)
- 異常系: [`../use_cases/exception_scenarios.md`](../use_cases/exception_scenarios.md) (ES-1〜5)
- 主要 UC: [`../use_cases/primary_use_cases.md`](../use_cases/primary_use_cases.md)
- 残タスク: [`PENDING_NFR_WORK.md`](PENDING_NFR_WORK.md)
- ADR: [`../../03_achievements/architecture_decisions/`](../../03_achievements/architecture_decisions/) (特に ADR-002 v1.1, ADR-006)
- 既存技術負債台帳: [`../../05_future_actions/technical_debt/TECHNICAL_DEBT_REGISTER.md`](../../05_future_actions/technical_debt/TECHNICAL_DEBT_REGISTER.md) (12 件)

---

## 改訂履歴

| バージョン | 日付 | 変更内容 |
|---|---|---|
| 1.0 | 2026-05-02 | 初版。28 失敗モードを A〜D 4 カテゴリで採点 (S/O/D/RPN)、上位 10 件を抽出、対策状況を可視化、Phase 12 アクション提示 |
