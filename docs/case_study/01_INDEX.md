# Spresense開発プロジェクト ドキュメントインデックス

**対象プロジェクト**:
- **Project 1**: Spresense SDK v3.4.5 アップデート & BMI160姿勢推定アプリケーション開発
- **Project 2**: Spresense HDRカメラ防犯カメラシステム 要求・仕様書作成

**期間**: 2025-12-13 〜 2025-12-15
**ステータス**: ✅ **両プロジェクト成功**

---

## 📚 ドキュメント一覧

### プロジェクト1: BMI160姿勢推定アプリケーション (12-13〜12-14)

### 1. メインドキュメント

#### 📄 [02_README.md](02_README.md)
**概要**: プロジェクト全体の包括的なドキュメント

**内容**:
- プロジェクト概要
- SDK環境のアップデート手順
- BMI160アプリケーション開発の詳細
- ビルドシステムへの統合方法
- 遭遇した問題と解決策
- 最終的な成功状況
- 技術的な学び

**対象読者**: プロジェクト全体を理解したい方、Spresense開発の初心者

**推奨読了時間**: 20分

---

### 2. プロジェクト完了報告

#### 🎉 [03_SUCCESS_SUMMARY.md](03_SUCCESS_SUMMARY.md)
**概要**: プロジェクトの完全な成功報告書

**内容**:
- エグゼクティブサマリー
- 主要な成果一覧
- 各フェーズの詳細（SDK更新、アプリ開発、統合、テスト）
- 技術的詳細（Madgwick AHRS、位置推定）
- 遭遇した課題と解決プロセス
- 学んだ重要な知見
- 今後の展開案

**対象読者**: プロジェクトマネージャー、技術リーダー、完成版を知りたい方

**推奨読了時間**: 15分

**ハイライト**:
```
✅ SDK v3.0.0 → v3.4.5 完全アップデート
✅ BMI160姿勢推定アプリ開発完了
✅ NuttXビルトイン登録成功
✅ 実機動作確認（1000秒以上）
```

---

### 3. 問題解決の記録

#### 🔧 [04_ISSUE_RESOLVED.md](04_ISSUE_RESOLVED.md)
**概要**: 遭遇した主要問題の詳細な解決記録

**内容**:
- 問題の概要（アプリが登録されない問題）
- 根本原因の特定プロセス
- 4つの主要原因の詳細分析
- ステップバイステップの解決手順
- 解決前後の比較表
- 技術的な深堀り
- builtin登録メカニズムの解明

**対象読者**: 同様の問題に遭遇した開発者、デバッグ手法を学びたい方

**推奨読了時間**: 10分

**重要な発見**:
```
1. NuttX側.configとSDK側.configの二重構造
2. CONFIG_EXAMPLES_* 命名規則の厳格さ
3. Make.defsの正確なパス指定の必要性
4. MODULE変数の不要性
```

---

### 4. 実行結果の完全ログ

#### 📊 [05_EXECUTION_OUTPUT.md](05_EXECUTION_OUTPUT.md)
**概要**: アプリケーションの実際の実行結果の完全な記録

**内容**:
- 実行コマンド
- センサー初期化メッセージ
- 実際のセンサーデータ出力（1000秒以上）
- データ分析（Roll/Pitch/Yaw範囲、位置推定特性）
- サンプリング性能の評価
- 技術的観察（AHRS性能、ドリフト特性）

**対象読者**: 実際の動作を確認したい方、性能を評価したい方

**推奨読了時間**: 15分（データログ含む）

**データハイライト**:
```
Roll角度: -24.91° 〜 97.71°
Pitch角度: -65.18° 〜 42.44°
Yaw角度: 360°回転追跡
サンプリング: 100Hz安定動作
```

---

### 5. コマンドリファレンス

#### 💻 [06_COMMANDS_SUMMARY.md](06_COMMANDS_SUMMARY.md)
**概要**: プロジェクトで使用した全コマンドの一覧

**内容**:
- SDK環境構築コマンド
- ビルド関連コマンド
- フラッシュ・書き込みコマンド
- デバッグ・確認コマンド
- トラブルシューティングコマンド

**対象読者**: 実際に手を動かしたい開発者、コマンドリファレンスが必要な方

**推奨読了時間**: 5分

---

### 6. AI開発効率化分析

#### 📈 [07_EFFICIENCY_ANALYSIS.md](07_EFFICIENCY_ANALYSIS.md)
**概要**: 実際使用プロンプトと最適化プロンプトの詳細比較分析

**内容**:
- フェーズ別の効率化分析（Phase 1, 2, 3）
- 往復回数、トークン使用量、解決時間の比較
- プロンプト品質スコアリング
- ROI（投資対効果）分析
- 効率化要因の詳細分析

**対象読者**: AI開発効率化に興味がある方、プロンプトエンジニアリングを学びたい方

**推奨読了時間**: 15分

**ハイライト**:
```
往復回数: 65%削減
トークン使用量: 65%削減
解決時間: 60%削減
情報精度: 2倍向上
```

---

#### 📊 [08_EFFICIENCY_SUMMARY.md](08_EFFICIENCY_SUMMARY.md)
**概要**: 効率化分析のエグゼクティブサマリー

**内容**:
- 効率化効果の総合まとめ
- 最も効果的だったプロンプトTOP3
- コスト削減効果（トークン、時間、金銭）
- 推奨使用戦略
- ベンチマーク比較

**対象読者**: 効率化の結論だけを知りたい方、マネージャー層

**推奨読了時間**: 5分

**主要な発見**:
```
実際使用: 6.5-9時間
提案版: 2.5-4時間（60%削減）
従来手法比: 7倍高速化
```

---

### 7. 設定ファイルサンプル

#### ⚙️ 設定ファイル群

##### [bmi160_Kconfig.txt](bmi160_Kconfig.txt)
- Kconfig設定の実例
- CONFIG変数の定義方法

##### [bmi160_Makefile.txt](bmi160_Makefile.txt)
- Makefileの実例
- ビルド設定の書き方

##### [config_reference.txt](config_reference.txt)
- .config設定のリファレンス
- 必須CONFIG変数一覧

**対象読者**: 独自アプリケーションを開発する方

---

### プロジェクト2: Spresense HDRカメラ防犯カメラシステム (12-15)

#### 📐 [09_SECURITY_CAMERA_PROJECT.md](09_SECURITY_CAMERA_PROJECT.md)
**概要**: 防犯カメラシステム仕様書作成プロジェクトの完全記録

**内容**:
- エグゼクティブサマリー
- 作成した6つの仕様書の詳細
  - 要求仕様書（25質問）
  - 機能仕様書（8 PlantUML図）
  - Spresense側ソフト仕様書（7 PlantUML図）
  - PC/Rust側ソフト仕様書（2 PlantUML図）
  - プロトコル仕様書（7 PlantUML図）
  - 実装プロンプト（15ステップ）
- 設計上の重要な決定（USB CDC, H.264, Rust, カスタムプロトコル）
- BMI160プロジェクトから継承した知見
- プロジェクト統計（30+ PlantUML図、67,000文字、135ページ相当）

**対象読者**: 仕様書作成手法を学びたい方、同様のシステムを設計する方

**推奨読了時間**: 25分

**ハイライト**:
```
✅ 6つの完全な仕様書（REQUIREMENTS, FUNCTIONAL, SOFTWARE x2, PROTOCOL, IMPLEMENTATION）
✅ 30以上のPlantUML図（設計可視化）
✅ 15ステップの実装プロンプト（Claude Code対応）
✅ NuttX二重コンフィグ構造への対応を明記
```

---

#### 📘 [10_SPECIFICATION_METHODOLOGY.md](10_SPECIFICATION_METHODOLOGY.md)
**概要**: ソフトウェア仕様書作成手法の実践ガイド

**内容**:
- 仕様書の階層構造（5+1階層）
- 各仕様書の作成方法とテンプレート
- PlantUML図の活用法（8種類の図）
- 実装プロンプトの設計方法
- ベストプラクティス集
- 完全なテンプレート集

**対象読者**: 仕様書作成の手法を体系的に学びたい方、組み込みシステム開発者

**推奨読了時間**: 30分

**主要トピック**:
```
1. 要求仕様書作成（25質問、優先度付け、標準構成案）
2. 機能仕様書作成（8種PlantUML図、機能要求定義）
3. ソフトウェア仕様書作成（モジュール設計、API仕様）
4. プロトコル仕様書作成（バイトレベル仕様、シーケンス図）
5. 実装プロンプト作成（15ステップ、環境固有注意）
```

**期待される効果**:
- 実装スピード向上（プロンプトに従うだけ）
- 品質向上（設計の一貫性）
- 保守性向上（最新ドキュメント）
- 知識共有（オンボーディング高速化）

---

## 🗺️ 読む順序の推奨

### プロジェクト1（BMI160）の読む順序

### 初めての方
1. **02_README.md** - 全体像を把握
2. **03_SUCCESS_SUMMARY.md** - 成果を理解
3. **05_EXECUTION_OUTPUT.md** - 実際の動作を確認

### 問題解決を知りたい方
1. **04_ISSUE_RESOLVED.md** - 問題と解決策
2. **02_README.md** - 背景情報
3. **06_COMMANDS_SUMMARY.md** - 実行コマンド

### 自分でアプリ開発する方
1. **02_README.md** - 全体フロー
2. **06_COMMANDS_SUMMARY.md** - 必要なコマンド
3. 設定ファイル群 - テンプレート
4. **04_ISSUE_RESOLVED.md** - トラブル対策

### 技術的詳細を知りたい方
1. **03_SUCCESS_SUMMARY.md** - Phase 2, 3の詳細
2. **05_EXECUTION_OUTPUT.md** - データ分析
3. **04_ISSUE_RESOLVED.md** - ビルドシステム解説

### AI開発効率化を知りたい方
1. **08_EFFICIENCY_SUMMARY.md** - 効率化の結論
2. **07_EFFICIENCY_ANALYSIS.md** - 詳細分析
3. **prompts/** フォルダ - 実際のプロンプト集

---

### プロジェクト2（防犯カメラ）の読む順序

### 仕様書作成手法を学びたい方
1. **10_SPECIFICATION_METHODOLOGY.md** - 手法の全体像
2. **09_SECURITY_CAMERA_PROJECT.md** - 実践例
3. `/home/ken/Spr_ws/spresense/security_camera/` - 実際の成果物

### 防犯カメラシステムを実装したい方
1. **09_SECURITY_CAMERA_PROJECT.md** - プロジェクト概要
2. `/home/ken/Spr_ws/spresense/security_camera/IMPLEMENTATION_PROMPT.md` - 実装ガイド
3. `/home/ken/Spr_ws/spresense/security_camera/README.md` - 仕様書インデックス

### 設計手法を深く学びたい方
1. **10_SPECIFICATION_METHODOLOGY.md** - 手法詳細
2. `/home/ken/Spr_ws/spresense/security_camera/FUNCTIONAL_SPEC.md` - 機能仕様例
3. `/home/ken/Spr_ws/spresense/security_camera/PROTOCOL_SPEC.md` - プロトコル仕様例

### PlantUML図の書き方を学びたい方
1. **10_SPECIFICATION_METHODOLOGY.md** Section 4 - PlantUML図活用
2. `/home/ken/Spr_ws/spresense/security_camera/FUNCTIONAL_SPEC.md` - 8種類の図
3. `/home/ken/Spr_ws/spresense/security_camera/SOFTWARE_SPEC_SPRESENSE.md` - 7種類の図

---

## 📊 プロジェクト統計

### プロジェクト1: BMI160姿勢推定

### 開発期間
- **開始**: 2025-12-13
- **完了**: 2025-12-14
- **総時間**: 約2日間

### コード量
- **アプリケーションコード**: 約27KB
- **ファイル数**: 10ファイル
- **総行数**: 約1,500行

### ドキュメント
- **メインドキュメント**: 8ファイル（約91KB）
- **プロンプト集**: 6ファイル（約60KB）
- **総文字数**: 約100,000文字
- **総ページ数**: 約150ページ相当

### 成果物
- **ファームウェア**: nuttx.spk (174KB)
- **ブートローダー**: loader.espk (307KB)
- **実行確認**: 1000秒以上の連続動作

---

### プロジェクト2: 防犯カメラシステム仕様書作成

### 開発期間
- **実施日**: 2025-12-15
- **総時間**: 約6時間

### ドキュメント量
- **仕様書**: 7ファイル（REQUIREMENTS, FUNCTIONAL, SOFTWARE×2, PROTOCOL, IMPLEMENTATION, README）
- **PlantUML図**: 30以上
- **総文字数**: 約67,000文字
- **総ページ数**: 約135ページ相当

### 設計内容
- **システム**: Spresense HDRカメラ + PC/Rust ハイブリッドシステム
- **通信**: USB CDC, カスタムバイナリプロトコル
- **映像**: H.264, 1280x720, 30fps, 2 Mbps
- **実装ステップ**: 15ステップ（Phase 1-3）

### 成果物（仕様書）
- **要求仕様書**: 25の質問で要求明確化
- **機能仕様書**: 11の機能要求、8種PlantUML図
- **Spresense側仕様書**: 6モジュール設計、7種PlantUML図
- **PC/Rust側仕様書**: 7モジュール設計、2種PlantUML図
- **プロトコル仕様書**: バイトレベル仕様、7種PlantUML図
- **実装プロンプト**: 15ステップの詳細ガイド

---

### 全プロジェクト統計

### 総開発期間
- **開始**: 2025-12-13
- **完了**: 2025-12-15
- **総日数**: 3日間

### 総ドキュメント量
- **case_study内**: 10ファイル（約120KB）
- **security_camera内**: 7ファイル（約340KB）
- **総文字数**: 約167,000文字
- **総ページ数**: 約285ページ相当

---

## 🎯 重要なポイント

### プロジェクト1（BMI160）の技術的ハイライト

### 技術的ハイライト

1. **NuttX/SDK二重.config構造**
   - SDK側: `/home/ken/Spr_ws/spresense/sdk/.config` (295B)
   - NuttX側: `/home/ken/Spr_ws/spresense/nuttx/.config` (68KB)
   - **ビルドシステムはNuttX側を参照**

2. **CONFIG命名規則**
   - 正しい: `CONFIG_EXAMPLES_BMI160_ORIENTATION`
   - 誤り: `CONFIG_MYPRO_BMI160_ORIENTATION`

3. **Make.defs重要性**
   ```makefile
   CONFIGURED_APPS += examples/bmi160_orientation
   ```

4. **Makefile最適化**
   - MODULE変数は不要
   - PROGNAME/PRIORITY/STACKSIZEのみ

### 学んだ重要な教訓

✅ **ビルドシステムの理解**: .config構造、命名規則、登録メカニズム
✅ **デバッグ手法**: 既存アプリとの比較、ビルドログの読み方
✅ **WSL2開発**: USB管理、シリアルポート、パーミッション
✅ **センサーフュージョン**: Madgwick AHRS、IMU特性、ドリフト対策

---

### プロジェクト2（防犯カメラ）の技術的ハイライト

1. **仕様書の階層構造**
   - 要求 → 機能 → ソフト → プロトコル → 実装
   - 各レベルで適切な抽象度
   - トレーサビリティ確保

2. **PlantUML図の活用**
   - システムアーキテクチャ図（全体構造）
   - シーケンス図（動的振る舞い）
   - 状態遷移図（ライフサイクル）
   - データフロー図（データ変換）

3. **実装プロンプト設計**
   - 15ステップに段階化
   - 各ステップ15-30分
   - NuttX固有の注意事項を明記
   - 具体的なコード例含む

4. **設計決定の根拠**
   - USB CDC: 安定性・簡単さ・十分な帯域
   - H.264: 高圧縮・ハードウェアサポート
   - Rust: メモリ安全・並行処理・高性能
   - カスタムプロトコル: 最小オーバーヘッド

### 両プロジェクトから得られた重要な知見

✅ **NuttX二重コンフィグ構造の理解と活用**
- BMI160プロジェクトで発見 → 防犯カメラプロジェクトで実装プロンプトに組み込み

✅ **段階的アプローチの重要性**
- Phase分割、ステップ分割で複雑さを管理

✅ **ドキュメントの力**
- 設計を可視化（PlantUML）
- 実装を加速（プロンプト）
- 知識を共有（仕様書）

✅ **AI活用開発の効率化**
- 適切なプロンプト設計で65%時間削減
- 具体的な参照先で精度2倍向上

---

## 🔗 関連リソース

### 公式ドキュメント
- [Spresense SDK セットアップガイド](https://developer.sony.com/spresense/development-guides/sdk_set_up_ide_ja)
- [NuttX ドキュメント](https://nuttx.apache.org/docs/latest/)
- [BMI160 データシート](https://www.bosch-sensortec.com/products/motion-sensors/imus/bmi160/)
- [Madgwick AHRS アルゴリズム](https://x-io.co.uk/open-source-imu-and-ahrs-algorithms/)

### 開発環境
- **SDK**: Spresense SDK v3.4.5
- **OS**: NuttX RTOS 12.3.0
- **ツールチェーン**: ARM GCC 12.2.1
- **開発環境**: Windows 11 + WSL2 (Ubuntu)

---

## 📞 サポート情報

### 質問がある場合
1. まず該当ドキュメントを確認
2. ISSUE_RESOLVED.mdで類似問題を検索
3. COMMANDS_SUMMARY.mdでコマンドを確認

### トラブルシューティング
- **ビルドエラー**: 04_ISSUE_RESOLVED.md 参照
- **登録されない**: 02_README.md Section 5.2 参照
- **USBデバイス**: 02_README.md Section 4.1 参照
- **実行エラー**: 05_EXECUTION_OUTPUT.md 参照
- **AI開発の効率化**: 08_EFFICIENCY_SUMMARY.md 参照

---

## ✅ チェックリスト

プロジェクトを再現する際のチェックリスト:

### 環境準備
- [ ] WSL2のインストール
- [ ] USB/IPの設定
- [ ] spresenseリポジトリのクローン
- [ ] 開発ツールのインストール

### アプリケーション開発
- [ ] ソースコードの作成
- [ ] 正しいディレクトリへの配置
- [ ] Kconfig/Makefileの作成
- [ ] CONFIG変数名の確認（EXAMPLES_*）

### ビルドシステム統合
- [ ] Kconfigへのsource追加
- [ ] Make.defsの作成と確認
- [ ] NuttX側.configへの設定追加
- [ ] ビルドログで"Register:"確認

### テストと確認
- [ ] ファームウェアビルド成功
- [ ] ブートローダーインストール
- [ ] ファームウェア書き込み
- [ ] NuttShellで実行確認

---

**インデックス作成日**: 2025-12-14
**最終更新**: 2025-12-15
**バージョン**: 3.0（防犯カメラプロジェクト追加、仕様書作成手法追加）

---

## 📝 ライセンスと謝辞

このドキュメントは、以下のプロジェクトの記録として作成されました:

**プロジェクト1**: Spresense SDK v3.4.5アップデート & BMI160姿勢推定アプリケーション開発
**プロジェクト2**: Spresense HDRカメラ防犯カメラシステム 要求・仕様書作成

### 使用技術・ライブラリ

- **Spresense SDK**: Sony Semiconductor Solutions Corporation
- **NuttX RTOS**: Apache Software Foundation
- **Madgwick AHRS**: Sebastian Madgwick
- **BMI160**: Bosch Sensortec
- **Rust**: The Rust Foundation
- **PlantUML**: PlantUML Project
- **FFmpeg**: FFmpeg Project
- **Tokio**: Tokio Contributors

本ドキュメントが、Spresense開発、組み込みシステム開発、そしてソフトウェア仕様書作成に携わるすべての方の助けとなれば幸いです。
