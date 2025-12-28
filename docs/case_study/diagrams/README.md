# Spresense ビルド環境 PlantUML図集

このディレクトリには、Spresense Phase 1.5 VGAビルド環境移行で明らかになった、システムの構造と依存関係を視覚化したPlantUML図が含まれています。

**最終更新**: 2025-12-28 (USB console troubleshooting flow 追加)

## 📊 含まれる図

### フェーズ別フロー図

#### 1. Phase 1: 環境構築フロー (`phase1_environment_setup_flow.puml`)
**目的**: 開発環境のセットアップ手順を理解する

**内容**:
- 現状確認（SDKバージョン、ツールチェーン）
- 環境診断と不足パッケージのインストール
- PATH設定
- ビルドテスト
- デバイス接続確認

**こんな時に見る**:
- 初めてSpresense開発を始める時
- 環境が壊れてゼロから構築する時
- エラーでどこが問題か分からない時

**関連ドキュメント**: [01_environment_check.md](../prompts/01_environment_check.md)

---

#### 2. Phase 2: builtin登録フロー (`phase2_builtin_registration_flow.puml`)
**目的**: アプリケーションをNuttXビルドシステムに統合する手順を理解する

**内容**:
- 既存アプリの分析方法
- Kconfig, Make.defs, Makefileの作成
- **最重要**: nuttx/.configへの設定
- builtin登録の検証
- よくある失敗（command not found）の原因と対処

**こんな時に見る**:
- カスタムアプリが「command not found」になる時
- builtin_list.hに登録されない時
- CONFIG設定の正しい場所が分からない時

**関連ドキュメント**: [02_build_system_integration.md](../prompts/02_build_system_integration.md)

---

#### 3. Phase 3: アプリケーション開発フロー (`phase3_application_development_flow.puml`)
**目的**: センサーアプリケーションの実装手順を理解する

**内容**:
- 要件定義からアーキテクチャ設計
- センサードライバーの確認と設定
- データ取得ループの実装
- フィルタリングとAHRS（姿勢推定）
- 出力方法の選択（シリアル、ファイル、LCD、ネットワーク）
- デバッグと性能最適化

**こんな時に見る**:
- センサーアプリを開発する時
- データ処理パイプラインを設計する時
- AHRSが発散する時

**関連ドキュメント**: [03_application_development.md](../prompts/03_application_development.md)

---

#### 4. カメラアプリケーションフロー (`camera_application_flow.puml`)
**目的**: カメラアプリケーション特有の実装を理解する

**内容**:
- 解像度、フォーマット、フレームレートの決定
- バッファ数の最適化（トリプルバッファリング）
- カメラ初期化シーケンス
- フレーム取得ループ（QBUF/DQBUF）
- エラーハンドリング（EAGAIN, EIO）
- パフォーマンス測定

**こんな時に見る**:
- カメラアプリを開発する時
- フレームドロップが発生する時
- バッファ管理を最適化したい時

**関連ドキュメント**:
- [camera_lessons_learned.md](../prompts/camera_lessons_learned.md)
- [camera_config_reference.md](../prompts/camera_config_reference.md)

---

### システム構造図

#### 5. CONFIG依存関係図 (`config_dependencies.puml`)
**目的**: NuttX CONFIG変数間の依存関係を理解する

**内容**:
- チップ選択（CXD56xx）からプロセッサアーキテクチャへの自動選択
- アーキテクチャ設定（CORTEXM4, ARMV7M）とツールチェーンの関係
- Work Queueとファイルシステム、SDカードの依存関係
- カメラサポートに必要な設定群
- アプリケーション設定の依存関係

**こんな時に見る**:
- 「CONFIG_XXXを追加したらなぜ他の設定も必要なの？」
- 「なぜSDカードを使うとWORK_QUEUEが必須なの？」
- 「アーキテクチャ設定は何が必要？」

**重要なポイント**:
- `CONFIG_ARCH_CHIP_CXD56XX=y` → 自動的に `CONFIG_ARCH_CORTEXM4=y` が選択される
- しかし `CONFIG_ARCH_ARMV7M=y` は手動設定が必要
- SDカードとファイルシステム自動マウントは両方とも `CONFIG_SCHED_WORKQUEUE` に依存

---

#### 6. ビルドシステムコンポーネント図 (`build_system_components.puml`)
**目的**: ファイル間の関係とビルドプロセスを理解する

**内容**:
- 設定ファイル（.config, Make.defs, defconfig）の生成フロー
- ビルドツール（config.py, kconfig-conf, mkspk）の役割
- アプリケーションの登録メカニズム
- ソースファイルからビルド成果物への変換プロセス
- 環境変数の影響

**こんな時に見る**:
- 「builtin_list.cはどうやって生成されるの？」
- 「Make.defsとは何？どこで使われる？」
- 「defconfigはどうやって.configになるの？」
- 「PATHとCROSSDEVはどこで使われる？」

**重要なポイント**:
- `nuttx/.config` の `CONFIG_EXAMPLES_*=y` から `builtin_list.c` が自動生成される
- `Make.defs` に `CROSSDEV = arm-none-eabi-` を設定しないとシステムgccが使われる
- `PATH` 環境変数で arm-none-eabi-gcc が見つかる必要がある

---

#### 7. ビルドフロー図 (`build_flow.puml`)
**目的**: 推奨されるビルド手順を理解する

**内容**:
- クリーン環境からの開始
- defconfigの適用
- 設定のカスタマイズ（標準アプリ無効化 → カスタムアプリ有効化）
- PATH設定
- ビルド実行
- エラー時のトラブルシューティング分岐
- ビルド成果物の検証

**こんな時に見る**:
- 「ビルドの正しい手順は？」
- 「どこでエラーが起こりやすい？」
- 「検証はどうやる？」

**重要なポイント**:
- defconfigから始めることで依存関係が自動解決される
- 並列ビルド（`make -j$(nproc)`）で高速化
- ビルドログを保存して問題解析を容易にする

**所要時間**: 5-15分（エラーがない場合）

---

### トラブルシューティング図

#### 8. トラブルシューティングフロー図 (`troubleshooting_flow.puml`)
**目的**: エラー発生時の診断と解決手順を理解する

**内容**:
- 6つの主要エラーパターンとその解決策
  1. kconfig-conf not found
  2. システムgccが使用される
  3. arm_exception.S not found
  4. CONFIG_* undeclared
  5. SDIO/Work Queue設定欠落
  6. スタック警告オプションの値が空
- その他のエラーへの対処法
- 緊急対処法（defconfigからの再開）

**こんな時に見る**:
- 「ビルドエラーが出た！どうする？」
- 「このエラーメッセージは何番のパターン？」
- 「解決策の優先順位は？」

**重要なポイント**:
- よくあるエラーは1-5分で解決可能
- 複雑なエラーは緊急対処（defconfig再設定）で5-10分で解決
- チートシート → 詳細ガイド → defconfig再開 の順で試す

---

#### 9. USB コンソールトラブルシューティングフロー図 (`usb_console_troubleshooting_flow.puml`) ⭐NEW
**目的**: USB シリアルコンソール接続の問題を診断・解決する手順を理解する

**内容**:
- `sercon: command not found` エラーの診断
- CONFIG設定の確認と修正
- 3つの解決策（手動sercon / 自動起動 / 両方）の選択
- 再ビルドと検証の手順
- デバイス未検出時のトラブルシューティング
- ドライバーとWSL2接続の確認

**こんな時に見る**:
- 「`sercon: command not found`が出た！」
- 「`/dev/ttyACM0`が見つからない」
- 「defconfigから移行したらUSBが動かない」
- 「新規ビルド環境でUSB接続ができない」

**重要なポイント**:
- `CONFIG_SYSTEM_CDCACM=y` → sercon/serdisコマンドが有効化される
- `CONFIG_NSH_USBCONSOLE=y` → NuttShell起動時に自動的にUSBコンソール有効化
- 推奨: 両方を有効化（解決策C）して最も柔軟な構成にする

**所要時間**: 設定のみ 5分、再ビルド含む 10-15分
**難易度**: ★★☆☆☆

**関連ドキュメント**: [usb_console_troubleshooting.md](../prompts/usb_console_troubleshooting.md)

---

#### 10. ディレクトリ構造図 (`directory_structure.puml`)
**目的**: ファイルの配置と役割を理解する

**内容**:
- GH_wk_test全体のディレクトリ構造
- spresense/sdk と spresense/nuttx の関係
- アプリケーションディレクトリの配置
- 設定ファイルの場所
- ビルド成果物の場所
- ツールチェーンの配置（$HOME/spresenseenv）
- ドキュメントとPlantUML図の配置

**こんな時に見る**:
- 「.configファイルはどこにある？」
- 「自分のアプリはどこに置く？」
- 「builtin_list.hはどこで生成される？」
- 「nuttx.spkはどこにできる？」

**重要なポイント**:
- 設定ファイルは2つある（`sdk/.config` と `nuttx/.config`）
- 最重要は `nuttx/.config`（アプリ登録を制御）
- ツールチェーンは `$HOME/spresenseenv/usr/bin` にある
- builtin関連ファイルは自動生成（手動編集不可）

---

## 🎨 PlantUML図の使い方

### オンラインで表示
1. [PlantUML Online Server](http://www.plantuml.com/plantuml/uml/) にアクセス
2. `.puml`ファイルの内容をコピー＆ペースト
3. 「Submit」をクリックして図を表示

### ローカルでレンダリング（推奨）

#### VSCodeを使用
```bash
# PlantUML拡張機能をインストール
code --install-extension jebbs.plantuml

# .pumlファイルを開いて Alt+D でプレビュー
code config_dependencies.puml
```

#### コマンドラインを使用
```bash
# PlantUMLをインストール
sudo apt-get install plantuml

# PNG画像を生成
plantuml config_dependencies.puml

# SVG画像を生成（推奨：拡大しても綺麗）
plantuml -tsvg config_dependencies.puml

# すべての図を一括生成
plantuml *.puml
```

### 画像として保存
生成されたPNG/SVG画像は、プレゼンテーションやドキュメントに埋め込めます。

---

## 📚 関連ドキュメント

これらの図は以下のドキュメントと連携して使用することを推奨します：

### トラブルシューティング
- [build_environment_migration_troubleshooting.md](../prompts/build_environment_migration_troubleshooting.md)
  - 詳細なトラブル事例と解決策
  - ベストプラクティス
  - クリーンビルド手順

- [build_troubleshooting_cheatsheet.md](../prompts/build_troubleshooting_cheatsheet.md)
  - クイックリファレンス
  - よくあるエラーと即座の解決策
  - ワンライナー修正集

### カメラ関連
- [camera_config_reference.md](../prompts/camera_config_reference.md)
  - カメラ設定の詳細
  - 解像度とバッファ設定

- [camera_lessons_learned.md](../prompts/camera_lessons_learned.md)
  - カメラ開発の教訓
  - 性能最適化のヒント

---

## 🔄 図の更新

これらの図は、Phase 1.5 VGAビルド環境移行（2025-12-28）時点の情報に基づいています。

### 更新が必要なケース
- SDK/NuttXのバージョンアップ時
- 新しい依存関係が発見された時
- ビルドプロセスが変更された時
- 新しいトラブルパターンが見つかった時

### 更新方法
1. `.puml`ファイルを直接編集
2. PlantUMLでレンダリングして確認
3. 関連ドキュメントも更新
4. Gitにコミット

---

## 💡 活用例

### 新規メンバーのオンボーディング
```
1日目: directory_structure.puml でディレクトリ構造を理解
2日目: build_system_components.puml でビルドシステムを理解
3日目: config_dependencies.puml で設定の依存関係を理解
4日目: build_flow.puml に従って実際にビルド
5日目: troubleshooting_flow.puml を参照しながらトラブル解決を練習
```

### トラブルシューティング時
```
エラー発生
↓
troubleshooting_flow.puml でエラーパターンを特定
↓
該当する図（config_dependencies.puml等）で詳細を確認
↓
build_troubleshooting_cheatsheet.md で解決策を実行
```

### ドキュメント作成時
これらの図をエクスポート（PNG/SVG）して、以下に活用：
- プレゼンテーション資料
- 設計書
- トレーニング資料
- README.md

---

## 📊 図の統計

| 図 | 主要コンポーネント数 | 依存関係数 | 複雑度 | 対象フェーズ |
|----|---------------------|-----------|--------|------------|
| phase1_environment_setup_flow.puml | 20 | 15 | ★★☆☆☆ | Phase 1 |
| phase2_builtin_registration_flow.puml | 25 | 20 | ★★★★★ | Phase 2 |
| phase3_application_development_flow.puml | 30 | 25 | ★★★☆☆ | Phase 3 |
| camera_application_flow.puml | 35 | 30 | ★★★★☆ | Phase 3 |
| config_dependencies.puml | 40+ | 50+ | ★★★★★ | 全フェーズ |
| build_system_components.puml | 30+ | 40+ | ★★★★☆ | 全フェーズ |
| build_flow.puml | 15 | 10 | ★★☆☆☆ | 全フェーズ |
| troubleshooting_flow.puml | 20 | 15 | ★★★☆☆ | 全フェーズ |
| directory_structure.puml | 50+ | 20+ | ★★★★☆ | 全フェーズ |

---

## ✅ チェックリスト: 図の理解度確認

自分の理解度を確認してみましょう：

**Phase 1: 環境構築**
- [ ] 現状確認に必要なコマンドを3つ以上言える
- [ ] PATHの設定方法を説明できる
- [ ] ビルドテストの手順を説明できる

**Phase 2: builtin登録**
- [ ] nuttx/.configとsdk/.configの違いを説明できる
- [ ] CONFIG_EXAMPLES_プレフィックスが必要な理由を説明できる
- [ ] builtin_list.hがどこから生成されるか説明できる
- [ ] "command not found"の主な原因を3つ言える

**Phase 3: アプリ開発**
- [ ] センサーデータ取得の基本フローを説明できる
- [ ] AHRSパラメータ調整の考え方を説明できる
- [ ] デバッグ時の確認ポイントを3つ以上言える

**カメラアプリ**
- [ ] トリプルバッファリングのメリットを説明できる
- [ ] QBUF/DQBUFの役割を説明できる
- [ ] EAGAIN と EIO の違いを説明できる

**CONFIG依存関係**
- [ ] なぜCONFIG_ARCH_ARMV7M=yが必要か説明できる
- [ ] SDカードサポートに必要な設定を3つ以上言える
- [ ] カメラに必要な設定を5つ以上言える

**ビルドシステム**
- [ ] builtin_list.cがどこから生成されるか説明できる
- [ ] Make.defsの役割を説明できる
- [ ] PATHとCROSSDEVの違いを説明できる

**ビルドフロー**
- [ ] defconfigから始めるべき理由を説明できる
- [ ] ビルドの検証手順を説明できる
- [ ] エラー時の最初の対処を説明できる

**トラブルシューティング**
- [ ] 6つの主要エラーパターンを覚えている
- [ ] 各エラーの解決策を即座に言える
- [ ] 緊急対処法を説明できる

**ディレクトリ構造**
- [ ] 重要な設定ファイルの場所を3つ以上言える
- [ ] nuttx.spkがどこに生成されるか知っている
- [ ] ツールチェーンの場所を知っている

すべてチェックできたら、あなたはSpresense開発のエキスパートです！

---

**作成日**: 2025-12-28
**対象バージョン**: Spresense SDK v3.3.0 / NuttX 12.x
**プロジェクト**: Phase 1.5 VGAビルド環境移行
**ステータス**: ✅ 完成
