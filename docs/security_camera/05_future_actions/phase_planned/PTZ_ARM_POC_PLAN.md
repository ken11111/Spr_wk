# PTZ アーム統合 PoC 計画書 — Spresense + LeRobot SO-ARM101 Pro

**バージョン**: 0.1 (PoC 着手前 draft)
**作成日**: 2026-05-26
**ステータス**: 🟡 **PoC 計画段階** — 機材調達・体制確定待ち
**目的**: 既存 Spresense 防犯カメラに LeRobot SO-ARM101 Pro を統合し、PTZ (Pan/Tilt/Zoom) 型の追尾カメラを実現するための PoC を計画する。
**親 STAMP 文書**: [`../../02_specifications/quality/safety_analysis/STAMP_STPA_PTZ_ARM_REFERENCE.md`](../../02_specifications/quality/safety_analysis/STAMP_STPA_PTZ_ARM_REFERENCE.md) v0.1 (PoC 前 draft の安全分析)

---

## 📋 エグゼクティブサマリ

### PoC の最大の目的 3 つ

1. **物理可動部統合の実現性検証** — Spresense カメラを SO-ARM101 末端に搭載し、6 DoF 動作でも安定動作するか
2. **Visual Servoing の遅延・収束性測定** — E2E 遅延 < 200 ms / 発振収束 < 1 秒 を達成可能か
3. **本格統合判断のための定量データ収集** — STAMP_STPA_ANALYSIS.md v2.0 への統合可否を判断する基準データ取得

### PoC の規模感

| 項目 | 値 |
|---|---|
| 期間 | **8-11 週間** (Phase X.1〜X.5) |
| 総工数 | **60-80 人日** (1 名フルタイム or 2 名分担) |
| 機材費用 | **約 5-8 万円** (SO-ARM101 Pro + 既存資産流用前提) |
| 体制 | 開発 1-2 名 + 安全レビュー 1 名 (パートタイム) |

### 成功基準 (Go/No-Go for STAMP v2.0 統合)

- ✅ E2E 遅延 < 200 ms 達成 → 統合 OK
- ✅ Visual Servoing 収束時間 < 1 秒 達成 → 統合 OK
- ✅ 24 時間連続動作で配線・電源異常 0 → 統合 OK
- ❌ 上記いずれか NG → 構成見直し or 統合スコープ縮小

---

## §1 目的とスコープ

### 1.1 PoC の目的

本 PoC は **既存防犯カメラに「追尾機能」を加える** ことの技術的実現性と安全性を **机上検討から実機検証へ移行** するために実施。STAMP_STPA_PTZ_ARM_REFERENCE.md で抽出した新規 Loss / Hazard / UCA / 対策候補 (M-41〜M-44) を **実データで検証** することが目標。

### 1.2 スコープ

| 区分 | 含む | 含まない |
|---|---|---|
| **対象機能** | 動き検知 → 追尾 (pan/tilt 自動制御) | 完全自律巡回、AI 物体認識、複数ターゲット同時追尾 |
| **対象環境** | 屋内、固定設置 | 屋外、移動プラットフォーム |
| **対象運用** | デモ・実証実験 | 24/7 商用運用 |
| **既存機能** | 既存 motion_detector / 録画 / ストリーミング を流用 | 既存機能の置き換えはしない |
| **AI 模倣学習** | スコープ外 (LeRobot SDK の機能で参考のみ) | テレオペレーション / Diffusion Policy 等の研究機能 |

### 1.3 非ゴール

- **本 PoC では NuttX SMP 化 (M-37 候補) は試さない** (STAMP §10.6b で Phase 13+ 配置)
- **AI 推論 (顔認識等) は実装しない** (CXD5602 RAM 1.5 MB 制約)
- **複数アーム連携・複数カメラ対応** (Q13 要求と整合 = 単一カメラ前提)

---

## §2 システム概要

詳細は [`STAMP_STPA_PTZ_ARM_REFERENCE.md §1`](../../02_specifications/quality/safety_analysis/STAMP_STPA_PTZ_ARM_REFERENCE.md#1-想定システム構成-案-a-pc-経由-ptz-型) 参照。

### 2.1 構成図 (再掲)

```
[ 撮影対象 ]
    │
    ▼ (動き)
[ Spresense + カメラ ] ──WiFi/8888──┐
  (SO-ARM101 末端搭載)              │
    ▲                                ▼
    │ サーボ指令              [ PC viewer + LeRobot SDK ]
    │                                │
[ SO-ARM101 Pro ] ◄──USB-RS485───────┘
```

### 2.2 既存防犯カメラとの統合点

- 既存 `motion_detector.rs` の bounding box 出力 → 新規追尾制御ループへ
- 既存 MJPEG ストリーム + metadata (pan/tilt 角度を追加)
- 既存 STAMP/STPA M-39 (OSD 重畳) と統合: pan/tilt 角度も焼き込み

---

## §3 必要機材

### 3.1 ハードウェア

| 項目 | 数 | 想定価格 (税込, 2026 年想定) | 備考 |
|---|---|---|---|
| **LeRobot SO-ARM101 Pro** (本体) | 1 | **¥30,000-50,000** | Hugging Face 公式 or 互換キット |
| 予備 Feetech STS3215 サーボ | 2 | ¥3,000 | 故障時備え |
| Spresense メイン + HDR カメラボード + WiFi 拡張 | 1 set | (既存資産流用) | 重量実測必須 |
| 末端搭載治具 (3D プリント or アダプタ) | 1 | ¥2,000 (試作) | 設計+印刷 |
| USB-RS485 アダプタ (PC ↔ サーボバス) | 1 | ¥1,500 | SO-ARM101 Pro 付属の場合あり |
| 配線材 (USB cable, 電源ケーブル, ケーブルガイド) | — | ¥3,000 | |
| 電源 (USB バスパワー or 5V/3A) | 1 | ¥2,000 | |
| **E-Stop ボタン + 配線** (安全機構) | 1 | ¥3,000 | M-41 対策 |
| (任意) リミットスイッチ × 2 | 2 | ¥1,000 | 越境検知 |
| **小計** | — | **¥45,500-65,500** | + 予備費 ¥10,000 |

### 3.2 ソフトウェア

| 項目 | ライセンス | 備考 |
|---|---|---|
| LeRobot SDK (Python) | Apache 2.0 | Hugging Face 公式 |
| Python 3.10+ (PC viewer 環境) | — | 既存 PC 環境 |
| 既存 PC viewer (Rust) — 変更必要 | — | 追尾制御ループ追加 |
| OpenCV (PC 側 bbox 算出) | BSD | bounding box trail visualization 用 |

### 3.3 計測機材

| 項目 | 用途 |
|---|---|
| 高速度カメラ or スマホスローモーション (240fps) | E2E 遅延の実測 |
| Power meter | 電源供給安定性確認 |
| WiFi アナライザ (アプリで可) | 配線動作中の WiFi 接続強度 |

---

## §4 マイルストーン (Phase X.1〜X.5)

| Phase | 内容 | 期間 | 工数 | Go/No-Go ゲート |
|---|---|---|---|---|
| **X.1** | SO-ARM101 単体動作確認 (LeRobot SDK 経由) | 1 週間 | 5 人日 | サンプルプログラムで全 6 軸が動作 |
| **X.2** | Spresense カメラを末端搭載 + 配線設計 | 1-2 週間 | 8-10 人日 | 6 DoF 全動作範囲で配線断線 0 |
| **X.3** | motion_detector 出力 → サーボ動作 (open-loop) | 2-3 週間 | 12-15 人日 | bbox center が画面中心に近づく方向に動く |
| **X.4** | Visual Servoing PID + LP filter + 安全機構 (M-41/M-42) | 3-4 週間 | 20-25 人日 | E2E 遅延 < 200 ms, 収束 < 1 秒 |
| **X.5** | STAMP 反映判断 + 24h 連続動作試験 | 1 週間 | 5-7 人日 | 配線・電源異常 0, 衝突 0 |
| **合計** | — | **8-11 週間** | **50-62 人日** | — |

予備工数 (調達・トラブル) 10-18 人日を加えると **総工数 60-80 人日**。

---

## §5 各 Phase の実施手順

### 5.1 Phase X.1: SO-ARM101 単体動作確認 (1 週間)

**目的**: LeRobot SDK + Feetech サーボバスの動作確認、開発環境構築。

**手順**:
1. SO-ARM101 Pro 開梱、組み立て (公式手順書)
2. PC に LeRobot SDK インストール (`pip install lerobot`)
3. USB-RS485 アダプタで PC ↔ サーボバス接続
4. LeRobot SDK サンプル (`teleoperate.py` 等) で 6 軸キャリブレーション
5. 各サーボの可動範囲 / 速度限界を実測
6. キャリブレーションデータを保存 (再現性確保)

**Go ゲート**: 全 6 サーボが指令通り動作し、トルク・速度・位置 metrics が取得できる。

### 5.2 Phase X.2: カメラ搭載 + 配線設計 (1-2 週間)

**目的**: Spresense + カメラ + 配線を末端に搭載、6 DoF 全動作で物理的問題が起きないこと。

**手順**:
1. 末端搭載治具の 3D 設計 (Fusion 360 / FreeCAD) + 試作プリント
2. Spresense + HDR カメラボード + WiFi 拡張 を治具に固定
3. 重量実測 (目標 < 100 g)
4. 配線レイアウト設計:
   - データ: WiFi-only (USB ケーブル捻れ回避, M-43 対応)
   - 電源: 末端の Spresense 用に USB バスパワー or 小型バッテリ
   - ケーブルガイド: ZipTie + ケーブルチェーン
5. 6 DoF 全動作範囲で配線断線・接触不良 0 を確認 (24 時間連続稼働試験)
6. WiFi 通信安定性 (アーム動作中の RSSI 変動) を計測

**Go ゲート**: 配線断線 0、WiFi RSSI 変動 ≤ 10 dBm、重量制限内。

### 5.3 Phase X.3: open-loop 追尾 (2-3 週間)

**目的**: 既存 motion_detector → bbox center → サーボ動作 までの基本パイプライン構築。

**手順**:
1. PC viewer (Rust) に **アーム指令送信機能** を追加 (LeRobot SDK Python と subprocess 連携 or HTTP)
2. motion_detector の bbox 中心 (cx, cy) と画像中心 (320, 240) の偏差を算出
3. 偏差 → サーボ角度オフセット の変換式 (キャリブレーション)
4. 偏差比例制御 (P 制御のみ) でサーボ動作
5. 30 秒間動的対象を追尾し、bbox が画面中心に近づく傾向を観測
6. E2E 遅延を高速度カメラで測定 (capture → サーボ動作完了)

**Go ゲート**: 偏差が時間とともに減少し、E2E 遅延 < 500 ms (Phase X.4 で改善余地あり)。

### 5.4 Phase X.4: Visual Servoing + 安全機構 (3-4 週間)

**目的**: PID + LP filter で発振抑制、安全機構実装 (M-41/M-42)。

**手順**:
1. P 制御 → PI → PID へ拡張、Kp/Ki/Kd チューニング
2. LP filter or Kalman filter で発振抑制 (Cutoff 周波数調整)
3. **Soft Limit** 実装: サーボ角度範囲を電子的に制限 (M-41)
4. **E-Stop** 実装: 物理ボタン + ソフト割込み、押下 → 全サーボ電源遮断 (M-41)
5. **追尾対象ロスト時の挙動** 実装: 10 秒ロスト → 中央位置に戻る (M-44)
6. E2E 遅延 (capture → 物理移動完了) の最終測定 (高速度カメラ 240fps)
7. 発振の有無、収束時間を測定

**Go ゲート**: E2E 遅延 < 200 ms, 収束時間 < 1 秒, E-Stop 応答 < 100 ms。

### 5.5 Phase X.5: STAMP 反映判断 + 24h 連続動作 (1 週間)

**目的**: PoC 結果を STAMP_STPA_ANALYSIS.md v2.0 統合判断にまとめる。

**手順**:
1. 24 時間連続動作試験 (実環境想定の動的対象)
2. 配線異常・電源異常・E-Stop 誤動作のカウント
3. 計測結果サマリ作成 (STAMP_STPA_PTZ_ARM_REFERENCE.md §6 チェックリスト全項目)
4. **Go/No-Go 判断会議** (開発 + 安全 + PdM):
   - Go → STAMP_STPA_ANALYSIS.md v2.0 統合作業へ
   - No-Go → 構成見直し or PoC 終了
5. レポート作成 (PoC 完了報告書 = 別文書)

**Go ゲート**: 24h で異常 0、Go/No-Go 判断会議の Go 判定。

---

## §6 成功基準 (KPI)

| KPI | 目標値 | 計測方法 |
|---|---|---|
| **E2E 遅延** (capture → 物理移動完了) | **< 200 ms** | 高速度カメラ 240fps で計測 |
| **発振収束時間** | **< 1 秒** | bbox center を画面中心 50px 以内に収める時間 |
| **追尾成功率** (移動対象) | **≥ 80%** | 歩行速度対象を 30 秒追尾し画面外に出ない回数 / 試行回数 |
| **E-Stop 応答時間** | **≤ 100 ms** | ボタン押下 → 全サーボ電源遮断 (オシロ計測) |
| **24h 連続動作異常** | **0** | 配線断線・電源変動・WiFi 切断 等の累計 |
| **既存機能維持** | 100% | motion_detector / 録画 / ストリーミング が PoC 後も動作 |

---

## §7 リスク評価

### 7.1 技術リスク

| リスク | 影響度 | 発生確率 | 緩和策 |
|---|---|---|---|
| **E2E 遅延 > 200 ms** | 🔴 高 | 中 | アルゴリズム最適化、PC 側で軽量化 (OpenCV のみ使用) |
| **配線断線 (6 DoF 動作で疲労)** | 🔴 高 | 高 | WiFi-only 構成、ケーブルチェーン採用 |
| **Visual Servoing 発振** | 🟡 中 | 中 | LP filter + ゲイン保守的設定、最終手段は Kalman |
| **SO-ARM101 サーボ過負荷** | 🟡 中 | 低 | Soft Limit 早期実装、トルク監視 |
| **既存 Spresense CPU 100% 超 (STAMP H-17) との干渉** | 🟡 中 | 中 | アーム制御は PC 側に置く (Spresense 影響無し) |

### 7.2 運用リスク

| リスク | 影響度 | 緩和策 |
|---|---|---|
| **物理衝突 (人/物)** | 🔴 致命 | E-Stop 物理ボタン常設、可動領域内に物置かない、安全レビュー必須 |
| **電源異常 (バッテリ枯渇等)** | 🟡 中 | UPS 推奨、バッテリ運用時は残量 metrics で警告 |
| **24h 連続動作中の温度上昇** | 🟢 低 | サーモグラフィで温度監視 |

### 7.3 プロジェクトリスク

| リスク | 影響度 | 緩和策 |
|---|---|---|
| **機材調達遅延** (LeRobot SO-ARM101 Pro) | 🟡 中 | 早期発注、互換キットも検討 |
| **担当者リソース不足** | 🟡 中 | 段階別工数を見直し、Phase X.4 を最重要視 |
| **既存防犯カメラ機能への影響** | 🟢 低 | 追尾は PC 側で完結、Spresense は機能拡張のみ |

---

## §8 工数とスケジュール

### 8.1 工数内訳

| Phase | 工数 | 期間 |
|---|---|---|
| X.1 単体動作 | 5 人日 | 1 週 |
| X.2 搭載+配線 | 8-10 人日 | 1-2 週 |
| X.3 open-loop | 12-15 人日 | 2-3 週 |
| X.4 Visual Servoing + 安全 | 20-25 人日 | 3-4 週 |
| X.5 STAMP 反映 + 24h 試験 | 5-7 人日 | 1 週 |
| **本タスク計** | **50-62 人日** | **8-11 週** |
| 予備 (調達/トラブル) | 10-18 人日 | — |
| **総計** | **60-80 人日** | **8-11 週 + 緩衝** |

### 8.2 想定スケジュール (例: 2026 年 6 月開始の場合)

| Phase | 開始予定 | 完了予定 |
|---|---|---|
| 機材調達 | 2026/06/01 | 2026/06/14 |
| X.1 単体動作 | 2026/06/15 | 2026/06/21 |
| X.2 搭載+配線 | 2026/06/22 | 2026/07/05 |
| X.3 open-loop | 2026/07/06 | 2026/07/26 |
| X.4 Visual Servoing | 2026/07/27 | 2026/08/23 |
| X.5 STAMP 反映 + 24h | 2026/08/24 | 2026/08/30 |

→ **2026 年 8 月末に Go/No-Go 判断、9 月以降に本格統合 (STAMP v2.0)** が想定スケジュール。既存 Phase 10 (2026/9-2027/3) と並走可能。

---

## §9 体制と意思決定

### 9.1 想定体制

| 役割 | 工数 | 担当範囲 |
|---|---|---|
| **PoC 開発リード** | 60-80 人日 | 全 Phase の実装 + 計測 |
| **安全レビュー** | 5 人日 (パートタイム) | M-41/E-Stop 設計レビュー, Phase X.4-X.5 安全試験立会 |
| **PdM** | 3 人日 (会議のみ) | Go/No-Go 判断会議 (X.5) |

### 9.2 リポジトリ・ワークスペース配置

PoC 実装は **既存 Spresense リポジトリ (`~/Spr_ws/GH_wk_test/`) や PC viewer (`~/Rust_ws/`) とは分離した新規ワークスペース** に配置する。

#### 配置先: `~/Robotics_ws/` (新規 — PoC 着手時 X.1 で作成)

選定理由:
- Spresense (C/NuttX) と LeRobot (Python/PyTorch) は **言語・依存系・容量特性が全く異なる** ため混在を避ける
- `Spr_ws/` (9.3 GB) は既に Spresense 専用と化しており、LeRobot 統合で更に巨大化するリスク
- 将来別ロボット (Unitree, ROS2 連携等) も加える場合に **拡張可能な命名**

#### 配下構造案

```
~/Robotics_ws/                     # ロボティクス全般の起点
├── README.md                      # Spresense / Rust_ws との連携メモ
├── .venv/                         # Python 環境 (uv venv)
├── .gitignore                     # .venv/, data/, __pycache__/ 等
├── lerobot/                       # LeRobot SDK (pip install or submodule)
├── so-arm101/
│   ├── config/                    # キャリブデータ
│   └── calibration_data.json
├── ptz_integration/               # Spresense 防犯カメラ連携
│   ├── tracking_controller.py     # Visual Servoing PID (M-42 対応)
│   ├── motion_to_servo.py         # bbox → サーボ角度変換
│   ├── e_stop_handler.py          # E-Stop ハード割込み (M-41 対応)
│   └── soft_limit.py              # 動作領域制限 (M-41 対応)
├── tests/                         # PoC テストスクリプト (Phase X.1〜X.5)
│   ├── x1_servo_unit_test.py
│   ├── x2_payload_test.py
│   ├── x3_open_loop.py
│   ├── x4_visual_servoing.py
│   └── x5_24h_endurance.py
├── docs/                          # Robotics_ws 固有 (運用ノート等)
│   └── (本格 STAMP/設計は Spresense リポジトリ側参照)
└── data/                          # 計測ログ / 試験データ (.gitignore 推奨)
    ├── e2e_latency_measurements.csv
    └── 24h_endurance_log.csv
```

#### 3 ワークスペース構成での役割分担

| ワークスペース | 言語 | 役割 | 本 PoC での扱い |
|---|---|---|---|
| `~/Spr_ws/GH_wk_test/` | C/NuttX | Spresense アプリ + **docs/ (設計の真実 source)** | STAMP/STPA・要求書・計画はここに集約。本 PoC でも参照のみ、変更しない |
| `~/Rust_ws/security_camera_viewer/` | Rust | PC viewer (GUI/motion_detector/録画) | アーム指令送信機能を追加 (HTTP/subprocess で Robotics_ws と連携) |
| **`~/Robotics_ws/`** ★PoC で作成 | Python | LeRobot SDK + 追尾制御 + アーム実装 | 本 PoC の主成果物配置先 |

#### ws 間連携プロトコル (案)

```
[ Rust_ws viewer (motion_detector) ]
        │ bbox + frame
        ▼ HTTP POST (localhost:8901) or stdin JSON
[ Robotics_ws ptz_integration/tracking_controller.py ]
        │ サーボ角度指令
        ▼ LeRobot SDK API
[ ~/Robotics_ws/lerobot/ ] ──USB-RS485──> [ SO-ARM101 Pro ]
```

#### 作成タイミング

- **今 (PoC 着手前)**: 配置先方針として本書および [`../../README.md`](../../README.md) に記録
- **Phase X.1 (機材到着+環境構築時)**: 実際に `mkdir ~/Robotics_ws/` で作成、Python venv セットアップ
- **Phase X.5 (PoC 完了時)**: コード成果物は Robotics_ws 配下に残し、設計判断のみ Spresense リポジトリ docs/ に統合

### 9.3 意思決定ゲート

| ゲート | 時期 | 判断者 | 判断材料 |
|---|---|---|---|
| **G1: PoC 着手** | Phase 開始時 | PdM | 機材予算、人員確保 |
| **G2: X.2 配線設計 OK** | X.2 終了時 | 安全 + 開発 | 配線断線 0、WiFi 安定 |
| **G3: X.4 Visual Servoing OK** | X.4 終了時 | 安全 + 開発 + PdM | E2E 遅延・収束時間 達成 |
| **G4: 本格統合 Go/No-Go** | X.5 終了時 | 全員 | KPI 全達成 → Go、未達 → No-Go (構成見直し) |

---

## §10 PoC 完了後の出力

### 10.1 ドキュメント

- **PoC 完了報告書** (本書とは別文書, X.5 で作成)
- STAMP_STPA_PTZ_ARM_REFERENCE.md → STAMP_STPA_ANALYSIS.md v2.0 への統合差分
- 計測データ (CSV) + 図表 (matplotlib)

### 10.2 コード成果物

- LeRobot SDK 統合用 Python スクリプト
- PC viewer (Rust) の追尾制御ループ追加
- E-Stop / Soft Limit 実装

### 10.3 ハードウェア成果物

- 末端搭載治具 (3D データ + 印刷物)
- 配線アセンブリ (ドキュメント化)

---

## §11 関連文書

- 親 STAMP draft: [`../../02_specifications/quality/safety_analysis/STAMP_STPA_PTZ_ARM_REFERENCE.md`](../../02_specifications/quality/safety_analysis/STAMP_STPA_PTZ_ARM_REFERENCE.md) v0.1
- 既存 STAMP 本書: [`../../02_specifications/quality/safety_analysis/STAMP_STPA_ANALYSIS.md`](../../02_specifications/quality/safety_analysis/STAMP_STPA_ANALYSIS.md) v1.11
- 既存 ROADMAP: [`PHASE_PLANNED_ROADMAP.md`](PHASE_PLANNED_ROADMAP.md) (本書 Phase 12 として追加予定)
- 要求書: [`../../01_requirements/FUNCTIONAL_REQUIREMENTS.md`](../../01_requirements/FUNCTIONAL_REQUIREMENTS.md) Q12 (動き検出), Q13 (単一カメラ前提)
- 外部: Hugging Face LeRobot 公式ドキュメント (https://github.com/huggingface/lerobot)

## 改訂履歴

| Version | Date | 変更内容 |
|---|---|---|
| 0.1 | 2026-05-26 | 初版 (PoC 前 draft) — 機材 / マイルストーン X.1-X.5 / KPI / リスク / 工数 60-80 人日 / スケジュール例 |
