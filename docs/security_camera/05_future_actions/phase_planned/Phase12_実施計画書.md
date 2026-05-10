# Phase 12 実施計画書 — Tier 1 維持 + 家庭用 運用品質確立

**バージョン**: 1.0
**作成日**: 2026-05-05
**前提条件確定 (2026-05-05 ユーザー判断)**:
- ① **新規ハードウェア導入なし** → Tier 1 (Spresense + GS2200M) を維持
- ② **本番ターゲットは家庭用** (個人 LAN 内、24h 想定だが業務用 SLA は不要)

**位置付け**: Phase 11 (適応制御拡張 仕様策定) 完了後の運用品質確立フェーズ。本セッション (2026-05-01〜05) で整備した品質要求 / FMEA / 脅威モデル / 運用ランブック / RTM v5.0 を基に、**実機ベースの確認 + 残戦略判断 + 仕様恒久化** を行う。

> **本書の上位**:
> - [`MASTER_ROADMAP_2026.md`](../master_roadmap/MASTER_ROADMAP_2026.md) v2.0 (Phase 12 = Tier 移行 + セキュリティ判断 + 残課題対応 と定義)
> - [`../../02_specifications/quality/PENDING_NFR_WORK.md`](../../02_specifications/quality/PENDING_NFR_WORK.md) (引継 X タスク)
> - [`../../02_specifications/traceability/REQUIREMENTS_TRACEABILITY_MATRIX.md`](../../02_specifications/traceability/REQUIREMENTS_TRACEABILITY_MATRIX.md) v5.0 §C (Phase 12 引継事項)

---

## §1 Phase 12 の目標 (確定)

### 1.1 北極星

> **「Tier 1 (現ハード) のまま、家庭用個人運用 LAN として実用品質 (24h 稼働 / 長期メンテ容易) を確立する」**

### 1.2 Out of Scope (恒久確定)

ユーザー判断 ① ② の結果、以下は **WONT FIX** として恒久確定:

| 旧目標 (要求書 v0.1 暫定) | WONT FIX 理由 | 代替・受容 |
|---|---|---|
| Q1 Full HD 1920×1080 | 構造的天井 #1 で不可、Tier 移行も無し | VGA 640×480 で確定 |
| Q3 H.264 圧縮 (transport) | encoder_manager.c は dead code | MJPEG で確定 |
| Q5 RTSP プロトコル | 実装コスト大、家庭用で標準互換性不要 | カスタム MJPEG プロトコルで確定 |
| Q16 Want 100ms 遅延 | 構造的天井 #1 で物理的に不可 | 1s Must を最終目標 (Phase 8 実測 134ms ✅) |
| Q24 TLS 1.3 / JWT / mTLS | RAM 1.5MB 制約で困難 | LAN 隔離前提 + アプリ層簡易認証 (Option B) |

### 1.3 Phase 12 完了基準

以下の **(A)〜(E) すべて** を満たすこと:

- [A] **実測 3 件** (CPU / 24h 連続 / 行カバレッジ) 取得 + 公開
- [B] **自動再接続戦略** 改定 (ADR-002 v1.2) — Phase 9 実測「逆効果」への対応
- [C] **セキュリティ Option B 段階実装** (LAN 隔離 + アプリ層簡易認証 + ログ署名) → SECURITY_GAP_ANALYSIS.md 反映
- [D] **要求書 v1.1** 化 — Tier 1 + 家庭用前提の WONT FIX を明示、X-5 系の実装結果を反映
- [E] **本番移行チェックリスト** ([`../../07_operations/PRODUCTION_DEPLOYMENT_CHECKLIST.md`](../../07_operations/PRODUCTION_DEPLOYMENT_CHECKLIST.md)) 完成

---

## §2 サブ Phase 構成

ユーザー判断 ① (Tier 移行なし) により単線進行が可能。並列実行が可能な箇所のみ並走。

### Phase 12.1 — 実測実施 (約 2 週)

**目的**: Phase 12 内の戦略判断 (12.2〜12.4) に必要な実測値を取得。

| 実測 | 出口 | 関連 |
|---|---|---|
| **X-6 CPU 利用率** (camera/usb/control) | `[CPU]` 行ログ ≥ 60 サンプル + 集計 CSV | [`CPU_MEASUREMENT_GUIDE.md`](../../07_operations/CPU_MEASUREMENT_GUIDE.md) |
| **X-5f ST-1** 屋内常温 24h 連続 | アップタイム / FPS / ドロップ率 / FAILED 回数 | [`STRESS_TEST_PLAN.md`](../../07_operations/STRESS_TEST_PLAN.md) |
| **X-8 Stage 1** 行カバレッジ計測 | cargo-llvm-cov 導入 + Rust 側 % 確定 | [`TEST_COVERAGE_BASELINE.md`](../../02_specifications/quality/TEST_COVERAGE_BASELINE.md) |

並列可能。**家庭用想定なので、X-5f ST-3/4 (温度試験) は対象外** (家庭の屋内温度範囲は ST-1 で十分カバー)。

### Phase 12.2 — 自動再接続戦略 見直し (約 1 週)

**目的**: ADR-002 v1.1 で判明した「再接続が逆効果」を運用上どう扱うか確定。

| 入力 | 判断ロジック | 出口 |
|---|---|---|
| Phase 12.1 X-5f ST-1 結果 (FAILED 状態への遷移頻度) | 1 日 N 回未満 → max=5 維持 / N 回以上 → max=3 短縮 / 連発 → Auto-Reconnect 完全無効化 + 即座 FAILED 表示 | **ADR-002 v1.2** + 実装変更 (tcp_server.c) |

家庭用は「24h 中の数回のサービス停止 + 復旧手順の明確さ」が許容される (業務用 SLA とは違う)。

### Phase 12.3 — セキュリティ Option B 段階実装 (約 3 週)

**Option B = LAN 隔離前提 + アプリ層簡易認証 + ログ署名** ([`SECURITY_GAP_ANALYSIS.md`](../../02_specifications/quality/SECURITY_GAP_ANALYSIS.md) §5)

| Step | 実装内容 | 緩和される脅威 |
|---|---|---|
| **B-1 LAN 隔離前提の運用文書化** | RUNBOOK §0 に LAN 隔離チェックリスト、PRODUCTION_DEPLOYMENT_CHECKLIST.md に明記 | 文書ベースの脅威認識 |
| **B-2 アプリ層 PSK 認証** | tcp_server.c で接続時に PSK を SHA-256 で交換、不一致なら drop | TS-1 (なりすまし) DREAD 46 |
| **B-3 接続元 IP allowlist** | wifi_config.h に許可 IP リスト、未許可は drop | TD-1 (DoS スパム) DREAD 42 |
| **B-4 syslog 署名** | metrics packet に CRC のみだが、ログファイルに HMAC-SHA256 を 1 日 1 回付与 | TR-1/TR-2 (否認) DREAD 33 |

⚠ **実装しない**: TLS/mTLS (RAM 制約)、JWT (実装コスト大)、暗号化 (構造的天井 #4 で困難)。
これらは [`SECURITY_GAP_ANALYSIS.md`](../../02_specifications/quality/SECURITY_GAP_ANALYSIS.md) で「Tier 1 では不可、Tier 2 移行時の再判断対象」として明記。

### Phase 12.4 — Phase 11 .c 判断 (約 1 日)

**判断軸**: 家庭用に Phase 11 (多変数+予測適応制御) は必要か?

| 判断 | 理由 | 影響 |
|---|---|---|
| **❌ 撤回 (推奨)** | 家庭用は Phase 10 PID で十分、`enhanced_control.h` は dead API | enhanced_control.h を obsolete マーク + L2.C 図更新 + FMEA B8 解消 |
| 🟡 簡略実装 | adaptive PID のみ実装、predictive は撤回 | CPU 予算と効果のトレードオフ要計測 |
| ✅ フル実装 | 業務用 SLA を将来追加する可能性 | 工数大、家庭用なら過剰 |

**推奨**: ❌ 撤回 (Phase 12.1 X-6 実測で Phase 10 が安定動作を確認できれば十分)。

### Phase 12.5 — 仕様確定 (要求書 v1.1) (約 1 週)

| 文書 | 改訂内容 |
|---|---|
| [`FUNCTIONAL_REQUIREMENTS.md`](../../01_requirements/FUNCTIONAL_REQUIREMENTS.md) | v1.0 → v1.1。§1.0 確定方針に「Tier 1 + 家庭用」明記、§1.1 WONT FIX 一覧追加、Q1/Q3/Q5/Q16/Q24 を「実装事実」から「**確定値**」に格上げ |
| [`PRODUCTION_DEPLOYMENT_CHECKLIST.md`](../../07_operations/PRODUCTION_DEPLOYMENT_CHECKLIST.md) | **新規** — 家庭 LAN 配置手順 (WiFi 認証情報差替 / 24h 稼働確認 / 録画ローテーション動作確認 / セキュリティ Option B 検証) |
| [`SECURITY_GAP_ANALYSIS.md`](../../02_specifications/quality/SECURITY_GAP_ANALYSIS.md) | 12.3 実装結果を §6 に「Option B 実装済」として反映 |
| [`MASTER_ROADMAP_2026.md`](../master_roadmap/MASTER_ROADMAP_2026.md) | v2.0 → v2.1 — Phase 12 完了マーク + Phase 13+ の検討事項 (Tier 移行は将来候補で残す) |
| [`REQUIREMENTS_TRACEABILITY_MATRIX.md`](../../02_specifications/traceability/REQUIREMENTS_TRACEABILITY_MATRIX.md) | v5.0 → v5.1 — §B 達成サマリ更新 (実測値ベースで上書き) |

### Phase 12.6 — 統合検証 + クロージング (約 1 週)

- [`RUNBOOK.md`](../../07_operations/RUNBOOK.md) を実機で 1 周検証 (RUNBOOK §3 軽症 / §4 中重症 を 1 ケースずつ実演)
- 全 NFR 文書 cross-link 整合性チェック (PENDING_NFR_WORK.md ベース)
- Phase 12 完了レポート作成 ([`docs/security_camera/03_achievements/phase_deliverables/PHASE12_COMPLETION_REPORT.md`](../../03_achievements/phase_deliverables/) — 新規)

---

## §3 リソース・期間

**全期間**: 約 8 週 (並列実行で 6 週まで圧縮可能)

| サブ | 期間 | 並列可能 |
|---|---|---|
| 12.1 実測 | 2 週 | 12.6 ドキュメント整備と並列可 |
| 12.2 再接続戦略 | 1 週 | 12.1 実測完了が前提 |
| 12.3 セキュリティ B 段階 | 3 週 | 12.2 と並列可 |
| 12.4 Phase 11 判断 | 1 日 | 12.1 完了後即時 |
| 12.5 仕様確定 | 1 週 | 12.2/12.3/12.4 完了後 |
| 12.6 統合検証 | 1 週 | 12.5 完了後 |

**個人開発リソース想定**:
- 平日 1〜2h + 週末 4h ペース → 8 週で完走見込み
- 厳しければ Phase 12.3 セキュリティ Step B-4 (ログ署名) を Phase 13 に繰越可

---

## §4 リスクと対策 (本セッション識別済の継続事項)

| リスク | 対策 |
|---|---|
| 12.1 X-5f ST-1 で予期せぬ FAILED 連発 | 12.2 で Auto-Reconnect 完全無効化を選択肢に含む (家庭用なら Manual 復旧で許容) |
| 12.3 セキュリティ実装で構造的天井 #4 顕在化 (RAM 不足) | Step B-1〜B-3 はメモリ消費小、B-4 は SD カード活用で軽量化 |
| 12.5 要求書 v1.1 化で過去文書との整合性破綻 | 1 タスク = 1 commit + PENDING_NFR_WORK 同時更新を厳守 |
| 8 週後にも Phase 12 完了せず Phase 13 へずれ込み | 12.3 Step B-4 / 12.6 を Phase 13 繰越可、最低 12.1〜12.5 完了で Phase 12 終了宣言 |

---

## §5 完了後の Phase 13 候補 (参考)

Phase 12 完了 = Tier 1 + 家庭用 確定後、Phase 13 は以下の選択肢:

| 候補 | 想定条件 |
|---|---|
| Phase 13: Tier 移行 (旧 Phase 10) | 将来「業務用拡張」「Full HD 必要」の要望が顕在化したとき |
| Phase 13: 機能拡張 (複数カメラ / 通知 etc.) | Phase 12 安定運用後、家庭用機能を充実させる |
| Phase 13: 終了 / メンテモード | Phase 12 で実用性確立、新機能追加なし |

これは **Phase 12 完了後にユーザー判断**するもの。本書では選択肢を残すのみ。

---

## §6 関連文書

- 上位: [`../master_roadmap/MASTER_ROADMAP_2026.md`](../master_roadmap/MASTER_ROADMAP_2026.md) v2.0
- 引継ベース: [`../../02_specifications/quality/PENDING_NFR_WORK.md`](../../02_specifications/quality/PENDING_NFR_WORK.md)
- 実測ガイド:
  - [`../../07_operations/CPU_MEASUREMENT_GUIDE.md`](../../07_operations/CPU_MEASUREMENT_GUIDE.md) (12.1 X-6)
  - [`../../07_operations/STRESS_TEST_PLAN.md`](../../07_operations/STRESS_TEST_PLAN.md) (12.1 X-5f ST-1)
- 戦略判断根拠:
  - [`../../03_achievements/architecture_decisions/system_architecture/ADR_002_NETWORKING_TCP_HEALTH_MONITORING.md`](../../03_achievements/architecture_decisions/system_architecture/ADR_002_NETWORKING_TCP_HEALTH_MONITORING.md) v1.1 (12.2)
  - [`../../02_specifications/quality/SECURITY_GAP_ANALYSIS.md`](../../02_specifications/quality/SECURITY_GAP_ANALYSIS.md) (12.3)
  - [`../../02_specifications/quality/THREAT_MODEL.md`](../../02_specifications/quality/THREAT_MODEL.md) (12.3)
- 過去 Phase 計画書: [`Phase10_実施計画書.md`](Phase10_実施計画書.md), [`Phase11_実施計画書.md`](Phase11_実施計画書.md)

---

## §A ブランチ運用 (Phase 12 以降, 2026-05-10 確定)

Phase 12 の **Spresense ソース変更**は `phase12-firmware` ブランチで管理し、main は docs / 計画 / 整合性管理に専念する。既存の `phase10-control` `phase11-adaptive-control` パターンを継承。

### 役割分担

| ブランチ | 範囲 | コミット内容 |
|---|---|---|
| `main` | docs / 計画 / 整合性 | 仕様書, 監査結果, RUNBOOK, GLOSSARY 等 (`docs/`, README.md) |
| `phase12-firmware` | Spresense ソース実装 | `apps/examples/security_camera/*.{c,h}`, `spresense` submodule pointer, X-10 patches |

### 切り替え運用

- Spresense ソースを触る作業 (X-6 計装, X-10 patches, fw build 検証) は **必ず phase12-firmware に checkout してから実施**
- main を直接編集するのは docs / 計画系のみ
- 双方を同期したい場合は cherry-pick で意図的に運ぶ (自動 merge は禁止)

### main の整合性 (2026-05-10 切り出し時)

```
main HEAD = (Phase 12 cleanup 後の最新) — docs only
phase12-firmware HEAD = 49003fe (切り出し時の Spresense ソース込み snapshot)

submodule pointer:
  main             → spresense @ e9a4f170 (defconfig 関連 0 commits, クリーン)
  phase12-firmware → spresense @ 08434c2a (defconfig + NET_IPv4)
```

### バックアップタグ (緊急復旧用)

- `main-backup-2026-05-10` (main repo)
- `phase12-source-snapshot` (49003fe を別名で保存)
- `master-backup-2026-05-10` (spresense submodule)

### Phase 12 完了時の merge ポリシー

```bash
git checkout main
git merge --no-ff phase12-firmware -m "Merge Phase 12 firmware into main (X-6/X-9/X-10 完了)"
```

`merge-clean` skill で自動化可能。merge 前に CI build (X-9) + 実機検証 (Phase 12.1) 完了が前提。

### Phase 13 以降の継承

同パターンで `phase13-XXX` を切り出す。docs と source の二系統運用を継続。

---

## 改訂履歴

| バージョン | 日付 | 変更内容 |
|---|---|---|
| 1.0 | 2026-05-05 | 初版。ユーザー判断 (新規ハード導入なし + 家庭用) を反映、6 サブ Phase 構成、WONT FIX 5 件確定、完了基準 (A)〜(E)、リスクと対策、Phase 13 選択肢 |
| 1.1 | 2026-05-10 | §A ブランチ運用追記。Phase 12 Spresense ソース変更を `phase12-firmware` ブランチに分離、main は docs-only。バックアップタグ + merge ポリシー記載 |
