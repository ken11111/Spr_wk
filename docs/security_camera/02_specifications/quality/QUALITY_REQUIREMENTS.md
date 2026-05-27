# Quality Requirements (品質要求集約) — arc42 §10

**バージョン**: 1.0
**作成日**: 2026-05-01
**規格**: ISO/IEC 25010:2011
**目的**: 散在 NFR を 8 品質属性体系で集約し、目標値・実測値・達成状況を一元化する

**前提**: 詳細シナリオは [`QUALITY_ATTRIBUTE_SCENARIOS.md`](QUALITY_ATTRIBUTE_SCENARIOS.md) (QAS-1〜10) を参照。
用語は [`GLOSSARY.md`](GLOSSARY.md) に従う。

---

## 達成状況の凡例

- ✅ **達成**: 実測値が目標を満たす
- 🟡 **部分達成**: 一部条件下で達成、他で未達
- 🔴 **未達**: 目標と実測に乖離あり (構造的制約 / 設計のみ実装無し / 設計乖離)
- ⚪ **未定義**: NFR 自体が定義されていない (ギャップ)

---

## 1. 機能適合性 (Functional Suitability) — 🟡

| サブ属性 | 要求 | 達成 | 出典 |
|---|---|---|---|
| 完全性 (Completeness) | 要求書 v1.0 で機能 19 項目を確定 | 🟡 v0.1 ドラフト → v1.0 確定化進行中 | [`../../01_requirements/FUNCTIONAL_REQUIREMENTS.md`](../../01_requirements/FUNCTIONAL_REQUIREMENTS.md) |
| 正確性 (Correctness) | MJPEG プロトコル 100% パケット整合 | ✅ CRC-16-CCITT 検証 | `mjpeg_protocol.c` |
| 適切性 (Appropriateness) | 防犯カメラ用途として QVGA 30 fps が妥当か | 🟡 解像度の妥当性は要検証 (Q1 確定後) | 要求書 §2.1 |

**ギャップ**: 要求書 v0.1 の Q1-Q19 が確定していなかったため、機能完全性の評価が困難。本タスク (Task B) で v1.0 へ確定化。

---

## 2. 性能効率性 (Performance Efficiency) — 🟡

### 2.1 時間効率性 (Time Behavior)

| 指標 | 目標 | 実測 | 達成 | 関連 QAS |
|---|---|---|---|---|
| End-to-End 遅延 (Must) | < 1 秒 | ~134 ms 平均 (Phase 8) / ~227 ms 平均 (Phase 9) | ✅ | QAS-2 |
| End-to-End 遅延 (Want) | < 100 ms | 134 ms 以上 (構造的天井 #1 で物理的に達成不可) | 🔴 | QAS-2 |
| TCP 送信時間 最大 | < 1000 ms | 2,713 ms (Phase 8) / 5,228 ms (Phase 9 ピーク) | 🔴 | QAS-1 |
| PID 制御周期 | 100 ms (10 Hz) | 100 ms (`fps_controller.h:61`) | ✅ | QAS-3 |
| TCP Health 監視粒度 | 3 秒粒度 | 3 秒 (移動平均 8 サンプル) | ✅ | QAS-4 |

### 2.2 資源効率性 (Resource Utilization)

| 指標 | 目標 | 実測 | 達成 |
|---|---|---|---|
| RAM 使用量 | < 1.5 MB (構造的天井 #4) | ~60% 使用 (V4L2 192KB + queue 360KB + batch 122KB + SO_SNDBUF 256KB) | ✅ |
| CPU 利用率 (per core) | TBD | **未文書化** | ⚪ |
| SPI 帯域 | < 4 MHz 上限 | 200-300 KB/s 実効 (理論 500 KB/s の 60%) | 🟡 |
| WiFi 帯域 (実効) | TBD | TBD | ⚪ |
| USB CDC-ACM 帯域 | < 12 Mbps (1.5 MB/s) | 未計測 | ⚪ |

### 2.3 容量 (Capacity)

| 指標 | 目標 | 実測 | 達成 |
|---|---|---|---|
| 同時接続クライアント数 | 1 | 1 (要求書 Q13) | ✅ |
| 1 フレーム最大サイズ | < 60 KB | 50-60 KB (QVGA 平均) | ✅ |
| MJPEG batch packet | < 122 KB peak | 122 KB | ✅ |
| action_queue 深度 | 5 / 7 / 9 (動的) | 5/7/9 切替実装 | ✅ |
| Full HD 30 fps 目標 | 200KB+/frame × 30 = 6MB/s | **構造的天井 #1 で達成不可** | 🔴 (Tier 移行必要) |

**ギャップ**:
- CPU 利用率の予算化が未文書化 (Cortex-M4F × 6 のうち実使用コアと %)
- WiFi 帯域実効値の計測なし
- 起動時間予算 (電源 ON → first frame) 未定義

---

## 3. 互換性 (Compatibility) — 🟡

### 3.1 共存性 (Co-existence)

| 指標 | 状態 |
|---|---|
| Spresense ファームウェア複数版の同時運用 | 単一版のみ想定 |
| プロトコル versioning | sync_word による種別識別あり (`0xCAFEBABE/BABF/BEEF`) |

### 3.2 相互運用性 (Interoperability)

| 指標 | 目標 | 実測 | 達成 | 関連 QAS |
|---|---|---|---|---|
| MJPEG batch size 動的切替 | 1/2/3 切替可 (Phase 7.2a) | 2 で固定 (`MJPEG_BATCHING_ENABLED=0`) | 🟡 | QAS-9 |
| metrics packet サイズ拡張 | Phase 9.2 で 50→58B | ✅ 後方互換 (sync_word で識別) | ✅ | QAS-9 |
| Spresense / PC 版数管理 | バージョンネゴ | 未実装 (PC 側が固定 layout 想定) | ⚪ | - |

**ギャップ**: バージョンネゴシエーション機構の体系化なし。Phase A/B/C は CSV 列数で識別する程度。

---

## 4. 使用性 (Usability) — 🔴

PC viewer (`security_camera_viewer`) の UX 仕様は**ほぼ未定義**。

| 指標 | 状態 |
|---|---|
| GUI 操作仕様書 | ⚪ 未定義 |
| エラー表示方針 | ⚪ TCP health score の表示はあるが体系的なエラー分類なし |
| 国際化 (i18n) | ⚪ 未定義 (現状日本語ログのみ) |
| アクセシビリティ | ⚪ 未定義 |
| ヘルプ / オンボーディング | ⚪ 未定義 |

**ギャップ**: 防犯カメラとしての運用 UX (誰が・いつ・どこで viewer を見るか) のシナリオ定義なし。

---

## 5. 信頼性 (Reliability) — 🔴

### 5.1 成熟度 (Maturity)

| 指標 | 目標 | 実測 | 達成 | 関連 QAS |
|---|---|---|---|---|
| フレームドロップ率 | < 5% (要求書 Q17 「多少許容」) | 53.7% (Phase 8) → 74.2-75.5% (Phase 9 悪化) | 🔴 | QAS-2 |
| MTBF (連続稼働時間) | TBD | 未計測 | ⚪ | - |

### 5.2 可用性 (Availability)

| 指標 | 目標 | 実測 | 達成 |
|---|---|---|---|
| Uptime SLA | TBD | 未定義 | ⚪ |
| 計画停止時間 | TBD | 未定義 | ⚪ |

### 5.3 障害耐性 (Fault Tolerance)

| 指標 | 目標 | 実測 | 達成 | 関連 QAS |
|---|---|---|---|---|
| TCP 切断時の自動再接続 | max=5 回, exponential backoff | ✅ Phase 9 実装 | ✅ | QAS-1, QAS-8 |
| 健全性 6 状態モデル | HEALTHY → ... → FAILED | ✅ 設計済 (実装は driver 側で観測) | ✅ | QAS-4 |
| スパイク検知 | avg×3 or >1000ms 連続 2 回 | ✅ Phase 9.2 実装 | ✅ | QAS-4 |

### 5.4 回復性 (Recoverability)

| 指標 | 目標 (ADR-002 v1.0) | 実測 (ADR-002 v1.1) | 達成 | 関連 QAS |
|---|---|---|---|---|
| 切断からの復旧時間 | 2.8 秒 | **未達: 4 回目以降 RST 拒否で失敗** | 🔴 | QAS-1, QAS-8 |
| 再接続回数 | max=5 | 5 回後 FAILED 状態固着 | 🟡 | QAS-8 |
| 副作用なし | 再接続中も他処理継続 | **悪化: PC FPS 6.74 → 2.77 (-59%)** | 🔴 | QAS-1 |

**重要発見 (ADR-002 v1.1)**: 自動再接続戦略は**逆効果**であり、ハードウェア改善 (Tier 2/3/C 移行) なしでは構造的天井 #1 を回避できない。

**ギャップ**:
- MTBF / Uptime SLA の数値化なし
- 5 回失敗後の人手介入手順未定義
- Phase 9 自動再接続の「逆効果」が要求書 §3.2 に未反映 (本タスク Task B で対応)

---

## 6. セキュリティ (Security) — 🔴

> **⚠ 重要警告**: `SECURITY_ARCHITECTURE.md` (Phase 9.2 設計仕様) は TLS 1.3 + AES-256-GCM + JWT 認証の多層防御を詳述しているが、**実装は完全な無保護**である。詳細は [`SECURITY_GAP_ANALYSIS.md`](risk_analysis/SECURITY_GAP_ANALYSIS.md) 参照。

| サブ属性 | 設計 (Phase 9.2) | 実装 | 達成 | 関連 QAS |
|---|---|---|---|---|
| 機密性 (Confidentiality) | TLS 1.3 + AES-256-GCM | ❌ クリアテキスト MJPEG/TCP 直送 | 🔴 | QAS-5 (設計のみ) |
| 完全性 (Integrity) | GCM 認証タグ + JWT 署名 | ❌ CRC-16 のみ (改ざん検知なし) | 🔴 | QAS-5 |
| 否認防止 (Non-repudiation) | JWT クレーム + ログ署名 | ❌ 未実装 | 🔴 | QAS-5 |
| 説明責任 (Accountability) | 監査ログ + フォレンジック対応 | ❌ ログ署名なし | 🔴 | - |
| 真正性 (Authenticity) | 相互認証 (mTLS + デバイス証明書) | ❌ 認証なし | 🔴 | QAS-5 |

**ギャップ**: 設計-実装乖離が体系的に開示されていなかったため、本タスクで [`SECURITY_GAP_ANALYSIS.md`](risk_analysis/SECURITY_GAP_ANALYSIS.md) を新設し透明化する。

---

## 7. 保守性 (Maintainability) — 🟡

### 7.1 モジュール性 (Modularity)

| 指標 | 状態 |
|---|---|
| アプリ層 7 building block | ✅ L1 図で可視化済 |
| dead code 識別 | ✅ 本セッションで `encoder_manager.c` (H.264 path) と `protocol_handler.c` を dead code 認定 |
| 関心の分離 (SoC) | ✅ Capture / Transport / Control 分離 |

### 7.2 再利用性 (Reusability)

| 指標 | 状態 |
|---|---|
| Capture Pipeline の分離度 | ✅ V4L2 抽象化により他カメラ転用可 |
| Transport Manager の抽象化 | 🟡 TCP / USB の切替は app 側で if 分岐 |

### 7.3 解析性 (Analysability)

| 指標 | 状態 |
|---|---|
| ログレベル制御 | 🟡 syslog ベース、レベル切替実装 |
| metrics packet 拡張性 | ✅ Phase 4.1 → Phase 9.2 で 4 fields 追加実績 |
| 関連 QAS | QAS-7 |

### 7.4 修正性 (Modifiability)

| 指標 | 状態 |
|---|---|
| 設定の集約度 | 🟡 `config.h`, `wifi_config.h`, `mjpeg_protocol.h` に分散 |
| ビルド時間 | TBD (未計測) |
| 技術負債項目数 | Critical 3 / High 4 / Medium 3 (TECHNICAL_DEBT_REGISTER.md) |

### 7.5 試験性 (Testability)

| 指標 | 状態 |
|---|---|
| 単体テスト カバレッジ | ⚪ 未計測 |
| 統合テスト戦略 | 🟡 `02_specifications/functional/TEST_COVERAGE_ENHANCEMENT_SPEC.md` あるが体系化途上 |
| 受け入れ基準 | ⚪ 未定義 |

**ギャップ**:
- テストカバレッジの体系化が不完全
- 設定ファイル 3 分散 (config.h / wifi_config.h / mjpeg_protocol.h) の集約余地

---

## 8. 移植性 (Portability) — 🟢

| Tier | 想定ハード | 移植コスト | 期待効果 | 出典 |
|---|---|---|---|---|
| Tier 1 (現状) | Spresense + GS2200M | 0 | (基準) | `spresense_deployment_current.puml` |
| Tier 2 | ESP32-S3 (内蔵 WiFi) | 大 (+14 週: HW-1〜HW-4) | 構造的天井 #1 #2 解消 | `spresense_deployment_candidate_a_esp32s3.puml` |
| Tier 3 | RPi CM5 (Linux + HW JPEG/H.264) | 最大 (Spresense 資産放棄) | Full HD 30 fps 対応可 | `spresense_deployment_candidate_b_rpi_cm5.puml` |
| Tier C | Spresense + USB CDC-ACM のみ (GS2200M 論理切断) | 小 (+2-4 週: PC viewer のみ) | SPI 約 5 倍帯域 | `spresense_deployment_candidate_c_usb.puml` |

**評価**: 移植先の選択肢が定量化済み (要求性能と移植コストでマトリクス化)。移植性の体系化は本プロジェクトで最も整っている領域。

| 指標 | 状態 | 関連 QAS |
|---|---|---|
| 適応性 (Adaptability) | ✅ 4 Tier 候補で評価済 | QAS-6 |
| 設置性 (Installability) | ⚪ 工場出荷手順未定義 | - |
| 置換性 (Replaceability) | 🟡 ADR-006 GATE-1 で評価ゲートあり | QAS-6 |

---

## 9. 全体達成サマリ

| 品質属性 | 状態 | 主因 |
|---|---|---|
| 1. 機能適合性 | 🟡 | 要求書 v1.0 確定中 |
| 2. 性能効率性 | 🟡 | 1s Must は達成、100ms Want は構造的天井で物理不可 |
| 3. 互換性 | 🟡 | versioning 体系化途上 |
| 4. 使用性 | 🔴 | PC viewer UX 未定義 |
| 5. 信頼性 | 🔴 | Phase 9 再接続が逆効果 (ADR-002 v1.1) |
| 6. セキュリティ | 🔴 | 設計-実装乖離 (TLS/認証なし) |
| 7. 保守性 | 🟡 | dead code 識別済、テスト体系途上 |
| 8. 移植性 | 🟢 | Tier 1〜3/C で定量化 |

**結論**:
- **構造的天井**が信頼性・性能の上限を決定。Phase 12 以降は Tier 2/3/C 移行判断が中核タスク。
- セキュリティの設計-実装乖離は最大の透明性リスク。本タスクで `SECURITY_GAP_ANALYSIS.md` 新設し開示。
- 使用性 (PC viewer UX) は次に着手すべき NFR ギャップ。

---

## 改訂履歴

| バージョン | 日付 | 変更内容 |
|---|---|---|
| 1.0 | 2026-05-01 | 初版。ISO/IEC 25010 8 属性で散在 NFR を集約。各属性で目標/実測/達成状況を表形式で整理 |
