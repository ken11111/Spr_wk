# ドキュメント構造移行計画

**作成日**: 2026-01-22
**目的**: 現行構造から新インターフェース仕様分離型構造への段階的移行
**対象**: 92ファイル (65 Markdown + 27 PlantUML)

## 移行戦略

### Phase 1: 新ディレクトリ構造作成 ✅
### Phase 2: 重要ファイルの移行とリファクタリング
### Phase 3: インターフェース仕様の新規作成
### Phase 4: 要求トレーサビリティマトリクス整備
### Phase 5: 検証とクリーンアップ

---

## Phase 1: 新ディレクトリ構造作成

### 1.1 ディレクトリ作成コマンド

```bash
# 新構造のベースディレクトリ作成
cd /home/ken/Spr_ws/GH_wk_test/docs/security_camera

# 1. Requirements (要求管理)
mkdir -p 01_requirements_new

# 2. System Specifications (システム仕様)
mkdir -p 02_specifications_new/{functional,architecture,implementation,interface}
mkdir -p 02_specifications_new/implementation/{spresense,pc_side}
mkdir -p 02_specifications_new/interface/{protocol,transport,data_formats,interface_control}

# 3. Achievements (成果)
mkdir -p 03_achievements_new/{phase_deliverables,performance_results,interface_validation}

# 4. Issues & Challenges (課題)
mkdir -p 04_issues_challenges_new

# 5. Future Actions (今後の必要対応)
mkdir -p 05_future_actions_new

# 6. Evidence (エビデンス)
mkdir -p 06_evidence_new/{test_results,interface_tests,diagrams,raw_data}
mkdir -p 06_evidence_new/test_results/{performance_tests,phase_tests,error_analysis}
mkdir -p 06_evidence_new/interface_tests/{protocol_tests,transport_tests,interop_tests}
mkdir -p 06_evidence_new/diagrams/{system_diagrams,interface_diagrams,sequence_diagrams}
mkdir -p 06_evidence_new/raw_data/{csv_metrics,pcap_captures,logs}
```

### 1.2 ディレクトリ構造確認

```
security_camera/
├── 01_requirements_new/
├── 02_specifications_new/
│   ├── functional/
│   ├── architecture/
│   ├── implementation/
│   │   ├── spresense/
│   │   └── pc_side/
│   └── interface/                 ⭐ NEW (specifications配下)
│       ├── protocol/
│       ├── transport/
│       ├── data_formats/
│       └── interface_control/
├── 03_achievements_new/
│   ├── phase_deliverables/
│   ├── performance_results/
│   └── interface_validation/
├── 04_issues_challenges_new/
├── 05_future_actions_new/
└── 06_evidence_new/
    ├── test_results/
    │   ├── performance_tests/
    │   ├── phase_tests/
    │   └── error_analysis/
    ├── interface_tests/           ⭐ NEW
    │   ├── protocol_tests/
    │   ├── transport_tests/
    │   └── interop_tests/
    ├── diagrams/
    │   ├── system_diagrams/
    │   ├── interface_diagrams/    ⭐ NEW
    │   └── sequence_diagrams/
    └── raw_data/
        ├── csv_metrics/
        ├── pcap_captures/
        └── logs/
```

---

## Phase 2: 重要ファイルの移行とリファクタリング

### 2.1 優先順位別移行リスト

#### 🔴 Priority 1: インターフェース関連 (即座に移行)

| 現行ファイル | 移行先 | リファクタリング内容 |
|-------------|--------|---------------------|
| 01_specifications/04_MJPEG_PROTOCOL.md | 02_specifications_new/interface/protocol/MJPEG_PROTOCOL_SPEC.md | Phase 9.2対応を明記、TCP健全性監視追記 |
| 01_specifications/03_PROTOCOL_SPEC.md | 02_specifications_new/interface/protocol/COMMAND_PROTOCOL_SPEC.md | コマンドプロトコル部分を抽出 |
| 01_specifications/PHASE7.3.3_ERROR_HANDLING_SPEC.md | 02_specifications_new/interface/protocol/ERROR_HANDLING_SPEC.md | TCP健全性エラー(0x04xx)追加 |
| 03_manuals/03_USB_CDC_SETUP.md | 02_specifications_new/interface/transport/USB_CDC_ACM_SPEC.md | TTY Raw Mode要件を仕様化 |

#### 🟡 Priority 2: 要求・成果 (1週間以内)

| 現行ファイル | 移行先 | リファクタリング内容 |
|-------------|--------|---------------------|
| 01_specifications/01_REQUIREMENTS.md | 01_requirements_new/FUNCTIONAL_REQUIREMENTS.md | 機能要求と非機能要求を分離 |
| 01_specifications/02_FUNCTIONAL_SPEC.md | 01_requirements_new/FUNCTIONAL_REQUIREMENTS.md | 要求仕様として統合 |
| 06_project/01_CURRENT_STATUS.md | 03_achievements_new/phase_deliverables/PHASE1.5_VGA_OPTIMIZATION.md | 成果として再構成 |
| 04_test_results/05_PHASE15_VGA_性能テスト結果.md | 03_achievements_new/performance_results/FPS_IMPROVEMENTS.md | 性能成果として整理 |

#### 🟢 Priority 3: 課題・教訓 (2週間以内)

| 現行ファイル | 移行先 | リファクタリング内容 |
|-------------|--------|---------------------|
| 06_project/03_LESSONS_LEARNED.md | 04_issues_challenges_new/LESSONS_LEARNED.md | TTY Raw Mode教訓等を体系化 |
| 04_test_results/27_PHASE9_RECONNECT_FAILURE_ANALYSIS.md | 04_issues_challenges_new/CRITICAL_ISSUES.md | V4L2制約、USB帯域制約と統合 |
| 04_test_results/25_GS2200M_TCP_STACK_ANALYSIS.md | 04_issues_challenges_new/CRITICAL_ISSUES.md | TCP課題として統合 |

#### 🔵 Priority 4: エビデンス移行 (3週間以内)

| 現行ファイル | 移行先 | リファクタリング内容 |
|-------------|--------|---------------------|
| 04_test_results/diagrams/*.puml (27ファイル) | 06_evidence_new/diagrams/sequence_diagrams/ | ファイル名規則統一 |
| 04_test_results/metrics_*.csv (8ファイル) | 06_evidence_new/raw_data/csv_metrics/ | Phase別フォルダ分類 |
| 04_test_results/*.pcap (3ファイル) | 06_evidence_new/raw_data/pcap_captures/ | ネットワーク解析用 |

### 2.2 移行スクリプト例

```bash
#!/bin/bash
# migration_phase2.sh

echo "Phase 2: 重要ファイル移行開始"

# Priority 1: インターフェース関連
echo "Priority 1: インターフェース仕様移行"
cp 01_specifications/04_MJPEG_PROTOCOL.md 02_specifications_new/interface/protocol/MJPEG_PROTOCOL_SPEC.md
cp 01_specifications/03_PROTOCOL_SPEC.md 02_specifications_new/interface/protocol/COMMAND_PROTOCOL_SPEC.md
cp 01_specifications/PHASE7.3.3_ERROR_HANDLING_SPEC.md 02_specifications_new/interface/protocol/ERROR_HANDLING_SPEC.md
cp 03_manuals/03_USB_CDC_SETUP.md 02_specifications_new/interface/transport/USB_CDC_ACM_SPEC.md

# Priority 2: 要求・成果
echo "Priority 2: 要求・成果移行"
cp 01_specifications/01_REQUIREMENTS.md 01_requirements_new/FUNCTIONAL_REQUIREMENTS.md
cp 06_project/01_CURRENT_STATUS.md 03_achievements_new/phase_deliverables/PHASE1.5_VGA_OPTIMIZATION.md
cp 04_test_results/05_PHASE15_VGA_性能テスト結果.md 03_achievements_new/performance_results/FPS_IMPROVEMENTS.md

# Priority 3: 課題・教訓
echo "Priority 3: 課題・教訓移行"
cp 06_project/03_LESSONS_LEARNED.md 04_issues_challenges_new/LESSONS_LEARNED.md
cp 04_test_results/27_PHASE9_RECONNECT_FAILURE_ANALYSIS.md 04_issues_challenges_new/CRITICAL_ISSUES.md

# Priority 4: エビデンス
echo "Priority 4: エビデンス移行"
cp 04_test_results/diagrams/*.puml 06_evidence_new/diagrams/sequence_diagrams/
cp 04_test_results/metrics_*.csv 06_evidence_new/raw_data/csv_metrics/
cp 04_test_results/*.pcap 06_evidence_new/raw_data/pcap_captures/

echo "Phase 2移行完了"
```

---

## Phase 3: インターフェース仕様の新規作成

### 3.1 新規作成ファイルリスト

#### 3.1.1 プロトコル層 (protocol/)

```bash
# 新規作成: METRICS_PACKET_SPEC.md (Phase 9.2対応)
cat > 02_specifications_new/interface/protocol/METRICS_PACKET_SPEC.md << 'EOF'
# メトリクスパケット仕様

**Phase対応**: 9.2 TCP健全性監視
**Sync Word**: 0xCAFEBEEF
**パケットサイズ**: 58 bytes (50→58拡張)

## Phase 9.2拡張内容

新フィールド (+8 bytes):
- tcp_health_moving_avg_ms (4 bytes): TCP送信時間移動平均
- tcp_health_total_spikes (4 bytes): スパイク検出総数

スパイク検出ロジック:
- 条件: send_time > (moving_avg * 3) OR > 1000ms
- ウィンドウサイズ: 8サンプル
- 連続スパイク≥2: degradation_alert = true

用途: GS2200M資源枯渇予測、予防的再接続
EOF
```

#### 3.1.2 トランスポート層 (transport/)

```bash
# 新規作成: WIFI_TCP_SPEC.md
cat > 02_specifications_new/interface/transport/WIFI_TCP_SPEC.md << 'EOF'
# WiFi TCP仕様

**モジュール**: GS2200M
**Phase対応**: 7.0 (基本), 9.2 (健全性監視)

## TCP健全性監視 (Phase 9.2)

監視項目:
- TCP send()時間測定
- 移動平均計算 (8サンプル)
- スパイク検出・カウント

リソース枯渇パターン:
- 正常: 350ms以下
- 枯渇前: 2289ms急増
- 枯渇: RST送信

予防的再接続:
- 連続スパイク≥2: 再接続実行
- ダウンタイム: 30秒→数秒に短縮
EOF

# 新規作成: TRANSPORT_COMPARISON.md
cat > 02_specifications_new/interface/transport/TRANSPORT_COMPARISON.md << 'EOF'
# トランスポート比較

| 項目 | USB CDC-ACM | WiFi TCP |
|------|-------------|----------|
| 帯域 | 12 Mbps | ~1 Mbps |
| レイテンシ | ~30ms | ~234ms |
| 信頼性 | 100% | 99.85% |
| 健全性監視 | 無し | Phase 9.2対応 |

使い分け:
- 開発: USB (低レイテンシ)
- 運用: WiFi (予防的再接続)
EOF
```

#### 3.1.3 データフォーマット層 (data_formats/)

```bash
# 新規作成: CRC_VALIDATION_SPEC.md
cat > 02_specifications_new/interface/data_formats/CRC_VALIDATION_SPEC.md << 'EOF'
# CRC検証仕様

**アルゴリズム**: CRC-16-CCITT
**多項式**: 0x1021

## Phase 9.2対応

メトリクスパケットCRC範囲:
- 対象: 56 bytes (SYNC除く52+2+2)
- 新フィールド含む: tcp_health_moving_avg_ms, tcp_health_total_spikes

最適化履歴:
- Phase 1.0: 38.4ms (ビット計算)
- Phase 1.5: 8.7ms (ルックアップ) -77%改善
EOF
```

#### 3.1.4 インターフェース制御層 (interface_control/)

```bash
# 新規作成: RECONNECTION_SPEC.md (Phase 9対応)
cat > 02_specifications_new/interface/interface_control/RECONNECTION_SPEC.md << 'EOF'
# 再接続仕様 (Phase 9)

## Phase 9.2 予防的再接続

従来 (Phase 9.0):
- 切断後再接続 (事後対応)
- RST検出→再接続
- ダウンタイム: ~30秒

Phase 9.2改善:
- 切断前再接続 (予防対応)
- 健全性劣化検出→予防的再接続
- ダウンタイム: 数秒

健全性監視:
typedef struct {
    uint32_t send_times_ms[8];
    uint8_t  consecutive_spikes;
    bool     degradation_alert;
} tcp_health_monitor_t;

予防的再接続トリガー:
1. consecutive_spikes >= 2
2. degradation_alert = true
3. フレーム送信前チェック
4. 再接続実行→健全性リセット

期待効果: 95%ダウンタイム削減
EOF
```

### 3.2 PlantUMLダイアグラム新規作成

```bash
# システムアーキテクチャ図作成
cat > 06_evidence_new/diagrams/system_diagrams/security_camera_system_overview.puml << 'EOF'
@startuml security_camera_system_overview
!theme plain
title "Spresense Security Camera System Overview"

package "Spresense" {
    [ISX012 Camera] as camera
    [MJPEG Encoder] as encoder
    [TCP Health Monitor] as health <<Phase 9.2>>
    [Protocol Handler] as proto
    [USB/WiFi Transport] as transport
}

package "PC Side" {
    [Transport Layer] as pc_transport
    [Protocol Parser] as parser
    [MJPEG Viewer] as viewer
    [Recording Manager] as recorder
}

camera -down-> encoder : V4L2
encoder -down-> proto : JPEG frames
health -right-> proto : Health metrics
proto -down-> transport : Packets
transport -right-> pc_transport : USB/WiFi
pc_transport -down-> parser : Raw packets
parser -down-> viewer : Decoded frames
parser -down-> recorder : Recording

note right of health
Phase 9.2: TCP Health Monitoring
• Moving average (8 samples)
• Spike detection (>3x avg)
• Preventive reconnection
end note
@enduml
EOF

# インターフェース層詳細図
cat > 06_evidence_new/diagrams/interface_diagrams/interface_layer_detail.puml << 'EOF'
@startuml interface_layer_detail
!theme plain
title "Interface Layer Detail (Phase 9.2)"

package "Interface Specifications" {
    package "Protocol Layer" {
        [MJPEG Protocol\n0xCAFEBABE] as mjpeg
        [Metrics Protocol\n0xCAFEBEEF\n58 bytes] as metrics <<Phase 9.2>>
    }

    package "Transport Layer" {
        [USB CDC-ACM\n12 Mbps] as usb
        [WiFi TCP\nHealth Monitoring] as tcp <<Phase 9.2>>
    }

    package "Data Formats" {
        [JPEG Format\nVGA 36-65KB] as jpeg
        [CRC-16-CCITT\n56 bytes range] as crc <<Phase 9.2>>
    }

    package "Interface Control" {
        [Connection Mgmt] as conn
        [Preventive Reconnection\nSpike Detection] as reconn <<Phase 9.2>>
    }
}

mjpeg -down-> usb
mjpeg -down-> tcp
metrics -down-> tcp
jpeg -down-> crc
tcp -down-> reconn

note bottom of metrics
Phase 9.2 Extension:
+ tcp_health_moving_avg_ms (4 bytes)
+ tcp_health_total_spikes (4 bytes)
CRC range: 56 bytes
end note
@enduml
EOF
```

---

## Phase 4: 要求トレーサビリティマトリクス整備

### 4.1 マトリクス設計

```bash
# 要求トレーサビリティマトリクス作成
cat > 01_requirements_new/REQUIREMENTS_TRACE_MATRIX.md << 'EOF'
# 要求トレーサビリティマトリクス

## 機能要求トレーサビリティ

| 要求ID | 要求名 | 仕様書 | 実装 | テスト | 成果 |
|-------|--------|--------|------|-------|------|
| FR-001 | VGA MJPEG配信 | MJPEG_PROTOCOL_SPEC.md | camera_threads.c | PHASE15_VGA_TEST.md | FPS_IMPROVEMENTS.md |
| FR-002 | USB転送 | USB_CDC_ACM_SPEC.md | tcp_server.c | USB_TEST.md | PHASE1_USB_FOUNDATION.md |
| FR-003 | WiFi転送 | WIFI_TCP_SPEC.md | tcp_server.c | WIFI_TEST.md | PHASE7_WIFI_TCP.md |
| FR-004 | 性能監視 | METRICS_PACKET_SPEC.md | mjpeg_protocol.c | METRICS_TEST.md | PERFORMANCE_ANALYSIS.md |
| FR-005 | 自動再接続 | RECONNECTION_SPEC.md | camera_threads.c | RECONNECT_TEST.md | PHASE9_AUTO_RECONNECT.md |

## 非機能要求トレーサビリティ

| 要求ID | 要求名 | 目標値 | 実測値 | ステータス | エビデンス |
|-------|--------|---------|---------|-----------|-----------|
| NFR-001 | フレームレート | 30 fps | 11 fps | ⚠️ 制約有り | USB帯域制限 |
| NFR-002 | レイテンシ | <50ms | 44-48ms | ✅ 達成 | VGA_PERFORMANCE.md |
| NFR-003 | 信頼性 | 100% | 100% | ✅ 達成 | フレーム成功率 |
| NFR-004 | TCP健全性 | 予防的再接続 | Phase 9.2実装 | ✅ 達成 | TCP_HEALTH_TEST.md |
| NFR-005 | メモリ使用量 | <512KB | 192KB | ✅ 達成 | 87%削減 |

## インターフェース要求トレーサビリティ

| 要求ID | 要求名 | インターフェース仕様 | 実装 | 検証 |
|-------|--------|---------------------|------|------|
| IFR-001 | MJPEGプロトコル | MJPEG_PROTOCOL_SPEC.md | mjpeg_protocol.c | PROTOCOL_VALIDATION.md |
| IFR-002 | メトリクス拡張 | METRICS_PACKET_SPEC.md | mjpeg_protocol.c | METRICS_VALIDATION.md |
| IFR-003 | TCP健全性 | RECONNECTION_SPEC.md | tcp_server.c | RECONNECT_VALIDATION.md |
| IFR-004 | USB Raw Mode | USB_CDC_ACM_SPEC.md | 設定手順 | USB_VALIDATION.md |
| IFR-005 | CRC最適化 | CRC_VALIDATION_SPEC.md | crc_calc.c | CRC_PERFORMANCE.md |
EOF
```

---

## Phase 5: 検証とクリーンアップ

### 5.1 移行検証チェックリスト

```markdown
## 移行完了チェックリスト

### インターフェース仕様 ✅
- [ ] MJPEG_PROTOCOL_SPEC.md (Phase 9.2対応)
- [ ] METRICS_PACKET_SPEC.md (新規作成)
- [ ] USB_CDC_ACM_SPEC.md (TTY Raw Mode)
- [ ] WIFI_TCP_SPEC.md (健全性監視)
- [ ] RECONNECTION_SPEC.md (予防的再接続)

### 要求管理 ✅
- [ ] FUNCTIONAL_REQUIREMENTS.md
- [ ] NON_FUNCTIONAL_REQUIREMENTS.md
- [ ] INTERFACE_REQUIREMENTS.md
- [ ] REQUIREMENTS_TRACE_MATRIX.md

### 成果管理 ✅
- [ ] PHASE1_USB_FOUNDATION.md
- [ ] PHASE1.5_VGA_OPTIMIZATION.md
- [ ] PHASE7_WIFI_TCP.md
- [ ] PHASE9_AUTO_RECONNECT.md
- [ ] FPS_IMPROVEMENTS.md

### 課題管理 ✅
- [ ] CRITICAL_ISSUES.md (V4L2制約、USB制約)
- [ ] INTERFACE_ISSUES.md (TCP切断等)
- [ ] LESSONS_LEARNED.md (TTY Raw Mode等)

### エビデンス整理 ✅
- [ ] PlantUML図分類 (27ファイル)
- [ ] CSV分類 (8ファイル)
- [ ] PCAP分類 (3ファイル)
```

### 5.2 旧構造削除計画

```bash
# 移行完了後の旧構造削除 (慎重に実行)
echo "移行検証完了後に実行:"
echo "mv 01_specifications 01_specifications_OLD"
echo "mv 02_implementation 02_implementation_OLD"
echo "mv 03_manuals 03_manuals_OLD"
echo "mv 04_test_results 04_test_results_OLD"
echo "mv 05_optimization_plans 05_optimization_plans_OLD"
echo "mv 06_project 06_project_OLD"

echo "新構造リネーム:"
echo "mv 01_requirements_new 01_requirements"
echo "mv 02_specifications_new 02_specifications"
echo "mv 03_achievements_new 03_achievements"
echo "mv 04_issues_challenges_new 04_issues_challenges"
echo "mv 05_future_actions_new 05_future_actions"
echo "mv 06_evidence_new 06_evidence"
```

---

## 移行スケジュール

| Phase | 期間 | 担当者 | 成果物 |
|-------|------|--------|-------|
| Phase 1 | 1日 | システム担当 | 新ディレクトリ構造 |
| Phase 2 | 1週間 | 技術担当 | 重要ファイル移行 |
| Phase 3 | 2週間 | インターフェース担当 | 新仕様書作成 |
| Phase 4 | 1週間 | 品質担当 | トレーサビリティ整備 |
| Phase 5 | 3日 | 全担当者 | 検証・クリーンアップ |

**総期間**: 約1ヶ月

## 期待効果

1. **インターフェース仕様の明確化** ⭐
   - Spresense-PC境界の明確化
   - Phase 9.2健全性監視仕様の体系化

2. **要求トレーサビリティ向上**
   - 要求→仕様→実装→テスト→成果の追跡
   - 品質保証の向上

3. **ドキュメント可読性向上**
   - 92ファイルの体系的整理
   - 目的別アクセス改善

4. **今後の拡張性**
   - 新Phase対応の標準化
   - インターフェース変更管理の効率化

次のステップ: 要求トレーサビリティマトリクス設計詳細化