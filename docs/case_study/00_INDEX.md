# Security Camera Project - 総合インデックス

**最終更新**: 2025-12-16
**プロジェクト**: Spresense セキュリティカメラシステム
**状態**: Phase 1 完了 ✅ / Phase 2 完了 ✅ / Phase 3 準備完了

---

## 📚 ドキュメント構成

### Phase 1: Spresense側実装 (完了)

| # | ファイル | 内容 | 状態 |
|---|---------|------|------|
| 12 | [12_PHASE1_BUILD_SUCCESS.md](./12_PHASE1_BUILD_SUCCESS.md) | Phase 1 ビルド成功記録 | ✅ 完了 |
| 09 | [09_SECURITY_CAMERA_PROJECT.md](./09_SECURITY_CAMERA_PROJECT.md) | プロジェクト概要 | ✅ 完了 |
| 10 | [10_SPECIFICATION_METHODOLOGY.md](./10_SPECIFICATION_METHODOLOGY.md) | 仕様書作成手法 | ✅ 完了 |

### Phase 2: PC側Rust実装 (完了)

| # | ファイル | 内容 | 状態 |
|---|---------|------|------|
| 14 | [14_PHASE2_COMPLETE.md](./14_PHASE2_COMPLETE.md) | Phase 2 完了報告 | ✅ 完了 |
| 13 | [13_PHASE2_RUST_GUIDE.md](./13_PHASE2_RUST_GUIDE.md) | Rust実装ガイド (詳細) | ✅ 完了 |
| - | [/home/ken/Rust_ws/QUICKSTART.md](../../../Rust_ws/QUICKSTART.md) | クイックスタート | ✅ 完了 |

### 過去のケーススタディ

| # | ファイル | 内容 | プロジェクト |
|---|---------|------|------------|
| 01 | [01_INDEX.md](./01_INDEX.md) | 総合インデックス (旧) | BMI160 |
| 02 | [02_README.md](./02_README.md) | README | BMI160 |
| 03 | [03_SUCCESS_SUMMARY.md](./03_SUCCESS_SUMMARY.md) | 成功サマリ | BMI160 |
| 04 | [04_ISSUE_RESOLVED.md](./04_ISSUE_RESOLVED.md) | 問題解決記録 | BMI160 |
| 05 | [05_EXECUTION_OUTPUT.md](./05_EXECUTION_OUTPUT.md) | 実行出力 | BMI160 |
| 06 | [06_COMMANDS_SUMMARY.md](./06_COMMANDS_SUMMARY.md) | コマンド集 | BMI160 |
| 07 | [07_EFFICIENCY_ANALYSIS.md](./07_EFFICIENCY_ANALYSIS.md) | 効率分析 | BMI160 |
| 08 | [08_EFFICIENCY_SUMMARY.md](./08_EFFICIENCY_SUMMARY.md) | 効率サマリ | BMI160 |
| 11 | [11_CAREER_ROADMAP.md](./11_CAREER_ROADMAP.md) | キャリアロードマップ | 汎用 |

### リファレンス

| フォルダ/ファイル | 内容 |
|----------------|------|
| [prompts/](./prompts/) | プロンプト集・クイックリファレンス |
| [prompts/QUICK_REFERENCE.md](./prompts/QUICK_REFERENCE.md) | **重要**: 2重コンフィグ構造など |
| [prompts/02_build_system_integration.md](./prompts/02_build_system_integration.md) | ビルドシステム統合 |
| [prompts/03_application_development.md](./prompts/03_application_development.md) | アプリケーション開発 |

---

## 🎯 現在の状態

### ✅ Phase 1 完了 (2025-12-15)

**成果物**:
- Spresense ファームウェア: `nuttx.spk` (175KB)
- ソースコード: 13ファイル、約2,500行
- 場所: `/home/ken/Spr_ws/GH_wk_test/apps/examples/security_camera/`

**実装内容**:
- ✅ HD 1280x720 @ 30fps カメラキャプチャ
- ✅ H.264 Baseline エンコード (2Mbps)
- ✅ カスタムバイナリプロトコル (CRC16)
- ✅ USB CDC-ACM 転送

**解決した課題**:
1. プロジェクト配置ミス → 正しい場所に移動
2. CONFIG_VIDEO_STREAM 不足 → defconfig に追加
3. ログマクロ衝突 → undef + 数値リテラル
4. VIDEO_PROFILE 未定義 → 定数 0 に変更
5. Make.defs 欠落 → 作成

**重要な学び**:
- **2重コンフィグ構造**: SDK/.config (不使用) vs NuttX/.config (使用)
- defconfig が設定の大元
- Make.defs で CONFIGURED_APPS に追加必須

### 📝 Phase 2 準備完了

**開発場所**: `/home/ken/Spr_ws/GH_wk_test/security_camera_viewer/`

**実装予定**:
- ⬜ USB CDC-ACM シリアル通信
- ⬜ プロトコルパーサ (CRC16検証)
- ⬜ H.264 NALユニット再構築
- ⬜ ファイル保存 (MP4/MKV)
- ⬜ リアルタイム表示 (オプション)

**開始手順**:
```bash
cd /home/ken/Spr_ws/GH_wk_test
# security_camera_viewer はすでに作成済み
cd security_camera_viewer
cargo build
```

---

## 📂 ディレクトリ構造

```
/home/ken/Spr_ws/
├── GH_wk_test/                                  ← 新しいGitHub作業環境
│   ├── spresense/                               ← Spresense SDK (submodule)
│   │   ├── nuttx/
│   │   │   ├── .config                          ← NuttX設定 (使用)
│   │   │   └── nuttx.spk                        ← ファームウェア
│   │   └── sdk/
│   │       ├── configs/default/defconfig        ← 設定
│   │       └── tools/                           ← ビルドツール
│   ├── apps/                                    ← 外部アプリケーション
│   │   ├── examples/
│   │   │   ├── security_camera/                 ← セキュリティカメラ
│   │   │   ├── bmi160_orientation/              ← IMU姿勢推定
│   │   │   ├── camera/                          ← カメラサンプル
│   │   │   ├── asmpApp/                         ← ASMPマルチコア
│   │   │   ├── myasmp_worker/                   ← ASMPワーカー
│   │   │   └── aps00_sandbox/                   ← 学習用サンドボックス
│   │   ├── builtin/                             ← Builtin registry
│   │   ├── nshlib/                              ← NuttShell
│   │   └── system/                              ← システムアプリ
│   ├── security_camera_viewer/                  ← Phase 2 実装 (Rust)
│   │   ├── Cargo.toml
│   │   └── src/
│   │       ├── main.rs
│   │       ├── protocol.rs
│   │       └── serial.rs
│   ├── docs/                                    ← ドキュメント集約
│   │   ├── README.md                            ← 総合インデックス
│   │   ├── security_camera/                     ← プロジェクト仕様書
│   │   │   ├── 01_specifications/
│   │   │   │   ├── 02_FUNCTIONAL_SPEC.md
│   │   │   │   ├── 03_PROTOCOL_SPEC.md
│   │   │   │   ├── 05_SOFTWARE_SPEC_SPRESENSE.md
│   │   │   │   └── 06_SOFTWARE_SPEC_PC_RUST.md
│   │   │   ├── 02_implementation/
│   │   │   ├── 03_manuals/
│   │   │   ├── 04_test_results/
│   │   │   └── 05_project/
│   │   ├── case_study/                          ← 取り組み記録 (本フォルダ)
│   │   │   ├── 00_INDEX.md                      ← 本ファイル
│   │   │   ├── 12_PHASE1_BUILD_SUCCESS.md
│   │   │   ├── 13_PHASE2_RUST_GUIDE.md
│   │   │   └── prompts/
│   │   │       └── QUICK_REFERENCE.md           ← 2重コンフィグ構造
│   │   └── claude_queue/                        ← 自動化システム
│   │       ├── README.md
│   │       └── queue_manager_v2.sh
│   └── scripts/
│       └── build.sh                             ← ビルドスクリプト
└── Mypro/                                       ← 旧環境 (移行元)
    └── spresense/
```

---

## 🚀 Phase 2 を始める

### ステップ1: Rust環境確認
```bash
rustc --version
cargo --version
```

### ステップ2: プロジェクト確認
```bash
cd /home/ken/Spr_ws/GH_wk_test
# security_camera_viewer はすでに作成済み
cd security_camera_viewer
cargo build
```

### ステップ3: ガイド参照
- **詳細ガイド**: `/home/ken/Spr_ws/GH_wk_test/docs/case_study/13_PHASE2_RUST_GUIDE.md`

### ステップ4: 実装開始
1. Cargo.toml に依存関係追加
2. src/protocol.rs 作成
3. src/serial.rs 作成
4. src/main.rs 実装
5. テスト・デバッグ

---

## 📖 重要なリファレンス

### 2重コンフィグ構造 (最重要!)
- **場所**: `prompts/QUICK_REFERENCE.md` 165行目
- **内容**: SDK/.config vs NuttX/.config の違い
- **教訓**: 設定は NuttX側 .config に追加すること

### プロトコル仕様
- **場所**: `/home/ken/Spr_ws/GH_wk_test/docs/security_camera/01_specifications/03_PROTOCOL_SPEC.md`
- **パケット構造**: 22バイトヘッダー + 可変長ペイロード
- **CRC**: CRC-16-IBM-SDLC (Polynomial: 0x8408)

### ビルドシステム
- **場所**: `prompts/02_build_system_integration.md`
- **Kconfig**: examples/Kconfig に source 追加
- **Make.defs**: CONFIGURED_APPS に追加必須

---

## ✅ チェックリスト

### Phase 1 (Spresense)
- [x] プロジェクト構造作成
- [x] カメラマネージャ実装
- [x] エンコーダマネージャ実装
- [x] プロトコルハンドラ実装
- [x] USB転送実装
- [x] メインアプリケーション実装
- [x] Kconfig/Makefile設定
- [x] ビルド成功
- [x] 2重コンフィグ構造理解

### Phase 2 (PC側Rust)
- [ ] Rust環境セットアップ
- [ ] プロジェクト作成
- [ ] プロトコルパーサ実装
- [ ] シリアル通信実装
- [ ] ファイル保存実装
- [ ] 統合テスト
- [ ] 映像再生確認

### Phase 3 (統合テスト)
- [ ] エンドツーエンドテスト
- [ ] 長時間動作テスト
- [ ] エラーハンドリング確認
- [ ] パフォーマンス測定

---

## 🎓 学んだこと

### 技術的学び
1. **2重コンフィグ構造**: Spresenseのビルドシステム特性
2. **Make.defs の重要性**: CONFIGURED_APPS への追加が必須
3. **V4L2/H.264**: カメラとエンコーダのAPI
4. **USB CDC-ACM**: シリアル通信プロトコル
5. **CRC16**: データ整合性検証

### プロセス的学び
1. **段階的実装**: Option A (HD→Full HD) の有効性
2. **エラー解決**: 5つの主要課題を順次解決
3. **ドキュメント化**: 取り組みの記録が重要
4. **既存コード参照**: 他の examples を参考に

---

## 📞 次のアクション

### 即座に実行可能
```bash
# Phase 2 環境確認
cd /home/ken/Spr_ws/GH_wk_test
cd security_camera_viewer
cargo build

# ガイド表示
cat /home/ken/Spr_ws/GH_wk_test/docs/case_study/13_PHASE2_RUST_GUIDE.md
```

### Claude に依頼する場合
「Phase 2 のRust実装を始めたいです。/home/ken/Spr_ws/GH_wk_test/security_camera_viewer/でprotocol.rsから実装してください。」

---

**作成日**: 2025-12-15
**作成者**: Claude Code (Sonnet 4.5)
**プロジェクト状態**: Phase 1 完了 ✅ / Phase 2 準備完了 📝
