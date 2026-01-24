# Spresense プロジェクト - ドキュメント一覧

このディレクトリには、Spresense関連プロジェクトのすべてのドキュメントが含まれています。

---

## 📁 ドキュメント構成

```
docs/
├── README.md                    # このファイル
├── security_camera/             # セキュリティカメラプロジェクト
├── case_study/                  # Claude Code効率分析・事例研究
└── claude_queue/                # Claude自動実行キューシステム
```

### 🎥 security_camera/
Spresense ISX012 カメラボードを使用したセキュリティカメラシステムのドキュメント。
- 仕様書（要求仕様、機能仕様、プロトコル仕様）
- 実装計画とプロンプト集
- セットアップマニュアル
- テスト結果とトラブルシューティング
- プロジェクト管理ドキュメント

**現在のステータス**: ✅ Phase 1 完全成功 (MJPEG over USB CDC)

### 📊 case_study/
Claude Code を活用した開発効率の分析とプロジェクト事例。
- Claude Code 効率分析レポート
- セキュリティカメラプロジェクト詳細
- 仕様書作成方法論
- Phase 1/2 実装ガイド
- キャリアロードマップ

### 🤖 claude_queue/
Claude の自動実行とタスクキュー管理システム。
- キューマネージャースクリプト
- API/Web インターフェース
- 自動化オプション設定
- クォータ制御機能

---

## 🎉 Phase 1 完全成功! 🎉

**Phase 1A + Phase 1B 両方達成!**

Spresense カメラからの MJPEG ストリームを USB CDC 経由で PC に転送する機能が完全に動作しています。

詳細は [`security_camera/`](security_camera/) ディレクトリを参照してください。

---

## 📋 セキュリティカメラプロジェクト概要

Spresense ISX012 カメラボードを使用したセキュリティカメラシステム。リアルタイム MJPEG ストリームを USB CDC 経由で PC に送信。

### 現在のシステム構成 (Phase 1 完了)

```
┌─────────────────┐  USB CDC     ┌─────────────────┐
│  Spresense      │ ◄──────────► │  PC (WSL2)      │
│  + ISX012 Camera│  MJPEG       │  + 映像受信 ✅  │
│                 │  ~224 KB/s   │  + 保存 ✅      │
│  - JPEG取得 ✅  │              │                 │
│  - Protocol ✅  │              │  [Phase 2 予定] │
│  - USB送信 ✅   │              │  - デコード     │
│                 │              │  - GUI表示      │
│                 │              │  - 録画         │
└─────────────────┘              └─────────────────┘
```

### 実績データ

- **フレーム数**: 90/90 frames (100% success)
- **総送信**: 672,972 bytes (~657 KB)
- **プロトコル**: MJPEG (Sync + Seq + Size + JPEG + CRC16)
- **転送速度**: Full Speed USB (12 Mbps)

---

## 🚀 クイックスタート

**今すぐ試す**: [`security_camera/03_manuals/01_QUICK_START.md`](security_camera/03_manuals/01_QUICK_START.md)

**完全な手順**: [`security_camera/03_manuals/02_PHASE1_SUCCESS_GUIDE.md`](security_camera/03_manuals/02_PHASE1_SUCCESS_GUIDE.md)

---

## 📁 apps/examples/ アプリケーション構成

```
apps/examples/
├── security_camera/                  # セキュリティカメラ (MJPEG over USB CDC)
├── bmi160_orientation/               # BMI160 IMU姿勢推定 (Roll/Pitch/Yaw + AHRS)
├── camera/                           # 標準カメラサンプル (SDK提供)
├── asmpApp/                          # ASMP マルチコアサンプル
├── myasmp_worker/                    # ASMP ワーカーバイナリ
└── aps00_sandbox/                    # 学習用サンドボックス (複数サブプロジェクト)
    ├── aps_gpio_onoff/               # GPIO制御サンプル
    ├── test_asmp/                    # ASMPテスト
    ├── aps_template_asmp/            # ASMPテンプレート
    ├── aps_cxx_audio_detect/         # オーディオ検出 (C++)
    └── aps_cxx_audio_detect_worker/  # オーディオ検出ワーカー
```

## 📁 security_camera/ ドキュメント構成

```
security_camera/
├── 01_specifications/                # 📋 仕様書
│   ├── 01_REQUIREMENTS.md            # 要求仕様
│   ├── 02_FUNCTIONAL_SPEC.md         # 機能仕様
│   ├── 03_PROTOCOL_SPEC.md           # プロトコル仕様
│   ├── 04_MJPEG_PROTOCOL.md          # MJPEGプロトコル
│   ├── 05_SOFTWARE_SPEC_SPRESENSE.md # Spresense仕様
│   ├── 06_SOFTWARE_SPEC_PC_RUST.md   # PC (Rust) 仕様
│   ├── PERFORMANCE_LOGGING.md        # 性能ロギング仕様
│   └── PHASE1_5_UPDATE_PLAN.md       # Phase1.5更新計画
│
├── 02_implementation/                # 🔧 実装計画・ノート
│   ├── 01_IMPLEMENTATION_PLAN.md     # 実装計画
│   ├── 02_IMPLEMENTATION_NOTES.md    # 実装ノート
│   └── 03_IMPLEMENTATION_PROMPT.md   # 実装プロンプト
│
├── 03_manuals/                       # 📖 マニュアル・ガイド
│   ├── 01_QUICK_START.md             # クイックスタート
│   ├── 02_PHASE1_SUCCESS_GUIDE.md    # Phase1成功ガイド
│   ├── 03_USB_CDC_SETUP.md           # USB CDCセットアップ
│   └── 04_TROUBLESHOOTING.md         # トラブルシューティング
│
├── 04_test_results/                  # 🧪 テスト結果
│   ├── 01_PROTOCOL_TEST_RESULTS.md   # プロトコルテスト結果
│   ├── 02_MJPEG_SUCCESS.md           # MJPEG成功記録
│   ├── 03_ERROR_CODE_ANALYSIS.md     # エラーコード分析
│   ├── 04_TEST_PROCEDURE_FLOW.md     # テスト手順フロー
│   ├── 05~17_PHASE*.md               # Phase 1.5/2/3/7テスト結果
│   ├── MJPEG_INTEGRATION_TEST.md     # MJPEG統合テスト
│   ├── PHASE3.0_COMPLETION_REPORT.md # Phase3.0完了レポート
│   └── diagrams/                     # PlantUML図集約
│       └── *.puml                    # 全PlantUML図
│
├── 05_optimization_plans/            # 🚀 最適化提案・計画
│   ├── 01_パケット化処理最適化提案.md
│   ├── CRC最適化分析ファイル一覧.md
│   └── USB最適化クイックガイド.md
│
└── 06_project/                       # 📊 プロジェクト管理
    ├── 01_CURRENT_STATUS.md          # 現在のステータス
    ├── 02_CHANGES_SUMMARY.md         # 変更履歴
    ├── 03_LESSONS_LEARNED.md         # 学んだ教訓
    ├── PHASE15_STATUS.md             # Phase1.5ステータス
    └── PHASE3_PLAN.md                # Phase3計画
```

---

## 📚 セキュリティカメラ ドキュメント一覧

### ⭐ Phase 1 成功関連

1. **[01_QUICK_START.md](security_camera/03_manuals/01_QUICK_START.md)** - 最小限の手順で実行
2. **[02_PHASE1_SUCCESS_GUIDE.md](security_camera/03_manuals/02_PHASE1_SUCCESS_GUIDE.md)** - 完全な成功手順
3. **[01_CURRENT_STATUS.md](security_camera/06_project/01_CURRENT_STATUS.md)** - 現在の状態・進捗
4. **[01_PROTOCOL_TEST_RESULTS.md](security_camera/04_test_results/01_PROTOCOL_TEST_RESULTS.md)** - テスト結果
5. **[03_LESSONS_LEARNED.md](security_camera/06_project/03_LESSONS_LEARNED.md)** - 重要な発見と教訓

### 📖 設計ドキュメント

### 1. 要求仕様書 ([01_REQUIREMENTS.md](security_camera/01_specifications/01_REQUIREMENTS.md))

**内容**: ユーザー要求の明確化のための25の質問リスト

**用途**:
- 実装前にユーザーと仕様を確認
- 標準構成案を提示

**主要質問**:
- 映像解像度・FPS
- 圧縮方式（JPEG/H.264）
- 通信方式（USB/WiFi）
- 録画トリガー（常時/動き検出）
- ストレージ管理

**標準構成**:
- 解像度: 1280x720 (HD)
- FPS: 30
- 圧縮: H.264, 2 Mbps
- 通信: USB CDC
- 録画: 常時、1時間ごと分割、7日間保存

---

### 2. 機能仕様書 ([02_FUNCTIONAL_SPEC.md](security_camera/01_specifications/02_FUNCTIONAL_SPEC.md))

**内容**: システム全体の機能要求定義

**含まれるPlantUML図**:
- システムアーキテクチャ図
- ユースケース図
- データフロー図
- システム起動シーケンス図
- ファイル分割シーケンス図
- エラー回復シーケンス図
- 状態遷移図
- 配置図

**主要機能**:
- F-001: カメラ初期化
- F-002: 映像キャプチャ (30fps)
- F-003: H.264エンコード
- F-004: USB CDC接続
- F-005: フレーム送信
- F-006: 映像受信
- F-007: 常時録画
- F-008: ファイル分割 (1時間ごと)
- F-009: ストレージ管理 (7日間保存)
- F-010: リアルタイム表示
- F-011: 録画ステータス表示

**データ仕様**:
- キャプチャ: YUV422, 1280x720, 30fps (~27.6 MB/s)
- エンコード: H.264, 2 Mbps (~0.25 MB/s)
- 保存: MP4, 約900 MB/hour

---

### 3. Spresense側ソフトウェア仕様書 ([05_SOFTWARE_SPEC_SPRESENSE.md](security_camera/01_specifications/05_SOFTWARE_SPEC_SPRESENSE.md))

**内容**: Spresense側（NuttX/C言語）の詳細設計

**含まれるPlantUML図**:
- レイヤー構成図
- コンポーネント図
- クラス図（データ構造）
- 初期化シーケンス図
- メインループシーケンス図
- エラーハンドリングシーケンス図
- 状態遷移図

**主要モジュール**:

| モジュール | ファイル | 責務 |
|----------|---------|------|
| Camera Manager | camera_manager.c/h | カメラ制御、YUVフレーム取得 |
| Encoder Manager | encoder_manager.c/h | H.264エンコード |
| Protocol Handler | protocol_handler.c/h | パケット化、CRC計算 |
| USB Transport | usb_transport.c/h | USB CDC送信 |
| Main Application | camera_app_main.c | メインループ制御 |

**主要データ構造**:
- `camera_config_t`: カメラ設定（1280x720, 30fps）
- `camera_frame_t`: YUVフレームデータ
- `encoder_config_t`: エンコーダ設定（2Mbps, GOP=30）
- `h264_nal_unit_t`: H.264 NAL Unit
- `packet_t`: 通信パケット（22バイトヘッダ + ペイロード）

**メモリ最適化**:
- ゼロコピー設計
- 静的バッファ配置
- 外部DRAM活用

**Kconfig設定**:
```kconfig
config SECURITY_CAMERA
    select VIDEO
    select VIDEO_ISX012
    select USBDEV
    select CDCACM
```

---

### 4. PC側ソフトウェア仕様書 ([06_SOFTWARE_SPEC_PC_RUST.md](security_camera/01_specifications/06_SOFTWARE_SPEC_PC_RUST.md))

**内容**: PC側（Rust）の詳細設計

**含まれるPlantUML図**:
- レイヤー構成図
- モジュール構成図

**主要モジュール**:

| モジュール | ファイル | 責務 | 主要クレート |
|----------|---------|------|------------|
| USB Transport | usb_transport.rs | USB CDC通信 | tokio-serial |
| Protocol Handler | protocol.rs | パケットパース | nom |
| H.264 Decoder | decoder.rs | デコード、YUV→RGB変換 | ffmpeg-next |
| MP4 Writer | mp4_writer.rs | MP4ファイル作成 | mp4 |
| Storage Manager | storage_manager.rs | ストレージ管理 | rusqlite |
| GUI | gui/app.rs | リアルタイム表示 | egui, eframe |

**主要データ構造**:
- `VideoFrame`: RGB24映像フレーム
- `NalUnit`: H.264 NAL Unit（enum型付き）
- `Packet`: プロトコルパケット
- `HandshakeInfo`: ハンドシェイク情報
- `RecordingFile`: 録画ファイルメタデータ

**非同期設計**:
- Tokioランタイム使用
- mpscチャネルでタスク間通信
- Arc<Mutex<>>でスレッド間共有

**データフロー**:
```
USB Transport → Packet → Receiver → NAL Unit → Decoder → VideoFrame → [Recorder, GUI]
```

---

### 5. 通信プロトコル仕様書 ([03_PROTOCOL_SPEC.md](security_camera/01_specifications/03_PROTOCOL_SPEC.md))

**内容**: Spresense-PC間の通信プロトコル詳細

**プロトコル名**: Security Camera Binary Protocol (SCBP)
**バージョン**: 0x01

**含まれるPlantUML図**:
- 通信モデル図
- データフロー図
- 接続確立シーケンス図
- ストリーミングシーケンス図
- ハートビートシーケンス図
- フラグメント再構成シーケンス図
- プロトコル状態遷移図

**パケット構造** (22バイトヘッダ):
```
+--------+--------+--------+--------+
| Magic  | Ver    | Type   | Seq    |
| 0x5350 | 0x01   | 0x10   | 0-N    |
+--------+--------+--------+--------+
| Timestamp (μs)            | 8B    |
+---------------------------+-------+
| Payload Size              | 4B    |
+---------------------------+-------+
| Checksum (CRC16)          | 2B    |
+---------------------------+-------+
| Payload (0-4096 bytes)            |
+-----------------------------------+
```

**パケットタイプ**:
- 0x01: HANDSHAKE - 接続確立、映像情報通知
- 0x10: VIDEO_SPS - H.264 SPS
- 0x11: VIDEO_PPS - H.264 PPS
- 0x12: VIDEO_IDR - H.264 I-frame
- 0x13: VIDEO_SLICE - H.264 P-frame
- 0x20: HEARTBEAT - 接続確認（5秒ごと）
- 0xFF: ERROR - エラー通知

**フラグメンテーション**:
- NAL Unitが4KB超の場合、複数パケットに分割
- フラグメントヘッダ（7バイト）追加
- フラグ: FRAGMENT_START, FRAGMENT_END

**帯域幅**:
- 映像: 2 Mbps
- オーバーヘッド: ~5%
- 実効: 2.1 Mbps
- USB CDC帯域使用率: 17.5% (12 Mbps中)

---

### 6. 実装プロンプト案 ([03_IMPLEMENTATION_PROMPT.md](security_camera/02_implementation/03_IMPLEMENTATION_PROMPT.md))

**内容**: Claude Codeで実装する際の段階的プロンプト集

**Phase 1: Spresense側実装**
- プロンプト 1-1: プロジェクト構造とKconfig設定
- プロンプト 1-2: カメラマネージャ実装
- プロンプト 1-3: エンコーダマネージャ実装
- プロンプト 1-4: プロトコルハンドラ・USB転送実装
- プロンプト 1-5: メインアプリケーション実装

**Phase 2: PC側実装**
- プロンプト 2-1: Cargoプロジェクト作成
- プロンプト 2-2: 共通型・エラー型実装
- プロンプト 2-3: USB Transport・Protocol実装
- プロンプト 2-4: H.264デコーダ実装
- プロンプト 2-5: MP4ライター実装
- プロンプト 2-6: ストレージマネージャ実装
- プロンプト 2-7: GUI実装
- プロンプト 2-8: メインアプリケーション実装

**Phase 3: 統合テスト**
- プロンプト 3-1: Spresenseビルド・フラッシュ
- プロンプト 3-2: PC側ビルド・実行

**重要な注意事項**:
- NuttX二重コンフィグ構造（Kconfig + defconfig）
- `./tools/config.py` での設定反映が必須
- FFmpegシステムライブラリの依存

**実装チェックリスト**:
- Spresense側: 10項目
- PC側: 11項目
- 統合テスト: 7項目

---

## 🎯 セキュリティカメラ 開発フロー

### 1. 仕様確認フェーズ
```
security_camera/01_specifications/01_REQUIREMENTS.md → ユーザー確認 → 仕様確定
```

### 2. 設計フェーズ
```
02_FUNCTIONAL_SPEC.md (全体設計)
├─ 05_SOFTWARE_SPEC_SPRESENSE.md (Spresense側詳細設計)
├─ 06_SOFTWARE_SPEC_PC_RUST.md (PC側詳細設計)
└─ 03_PROTOCOL_SPEC.md (通信プロトコル設計)
```

### 3. 実装フェーズ
```
security_camera/02_implementation/03_IMPLEMENTATION_PROMPT.md を使用してClaude Codeで段階的実装
```

### 4. テストフェーズ
```
Phase 3プロンプトで統合テスト
```

---

## 🚀 セキュリティカメラ 実装ガイド

### Claude Codeで実装する場合

1. **Phase 1開始**:
   ```
   security_camera/02_implementation/03_IMPLEMENTATION_PROMPT.md の「プロンプト 1-1」をコピー
   → Claude Codeに送信
   → 自動実装
   ```

2. **順次実装**:
   - プロンプト 1-2 → 1-3 → 1-4 → 1-5
   - プロンプト 2-1 → 2-2 → ... → 2-8
   - プロンプト 3-1 → 3-2

3. **テスト実行**:
   ```bash
   # Spresense側
   cd /home/ken/Spr_ws/GH_wk_test/spresense/nuttx
   make
   sudo -E PATH=$HOME/spresenseenv/usr/bin:$PATH ../sdk/tools/flash.sh -c /dev/ttyUSB0 nuttx.spk

   # PC側
   cd /home/ken/Spr_ws/GH_wk_test/security_camera_viewer
   cargo run --release
   ```

---

## 📊 技術スタック

### Spresense側
- **OS**: NuttX RTOS
- **言語**: C
- **ハードウェア**: CXD5602 (ARM Cortex-M4F × 6)
- **カメラ**: ISX012 (HDRカメラ)
- **エンコーダ**: ハードウェアH.264エンコーダ
- **通信**: USB CDC-ACM

### PC側
- **言語**: Rust (Edition 2021, MSRV 1.70)
- **ランタイム**: Tokio (非同期)
- **通信**: tokio-serial (USB CDC)
- **デコード**: ffmpeg-next (H.264)
- **GUI**: egui + eframe
- **DB**: rusqlite (SQLite)
- **パース**: nom (コンビネータパーサ)

---

## 📈 システム性能

### 映像仕様
- **解像度**: 1280x720 (HD)
- **フレームレート**: 30 fps
- **コーデック**: H.264 Baseline Profile
- **ビットレート**: 2 Mbps
- **GOP**: 30フレーム (1秒ごとにIフレーム)

### ストレージ
- **1時間**: 約900 MB
- **1日**: 約21.6 GB
- **7日間**: 約151 GB
- **推奨容量**: 200 GB以上

### 遅延
- **エンコード遅延**: < 100 ms
- **転送遅延**: < 50 ms
- **デコード遅延**: < 100 ms
- **総遅延**: < 500 ms

---

## 🔧 トラブルシューティング

### Spresense側
- **ビルドエラー**: `make clean` → `./tools/config.py default` → `make`
- **USB認識しない**: CONFIG_CDCACM=y を確認
- **カメラエラー**: VIDEO, VIDEO_ISX012 を確認

### PC側
- **ビルドエラー**: FFmpegライブラリインストール確認
- **USB接続エラー**: `/dev/ttyACM*` の存在確認、権限確認
- **デコードエラー**: SPS/PPS受信確認、ffmpegログ確認

---

## 📝 次のステップ (Phase 2機能)

Phase 1完成後、以下の機能を追加予定:

1. **動き検出**
   - PC側で画像差分検出
   - 閾値超えで録画開始

2. **複数カメラ対応**
   - カメラIDをプロトコルに追加
   - GUI分割表示

3. **Web UI**
   - axum + WebSocketでブラウザ表示

4. **通知機能**
   - 動き検出時にメール送信
   - デスクトップ通知

---

## 📄 ライセンス

TBD

---

## 👥 貢献者

TBD

---

**ドキュメントバージョン**: 2.0
**最終更新**: 2025-12-21
**ステータス**: ✅ Phase 1 完全成功 (カメラ + プロトコル + USB CDC)
