# Claude Code クイックリファレンス

## 🚀 開発フローチート

```
START
  ↓
[Phase 1] 環境構築 (1-2h)
  ├─ SDK更新確認
  ├─ ツールチェーンインストール
  └─ ビルドテスト
  ↓
[Phase 2] ビルド統合 (2-4h) ★最難関★
  ├─ Kconfig/Makefile作成
  ├─ .config設定（NuttX側！）
  └─ builtin登録確認
  ↓
[Phase 3] アプリ実装 (3-5h)
  ├─ センサーデータ取得
  ├─ アルゴリズム実装
  └─ テスト・最適化
  ↓
DONE (合計: 6-11h)
```

## 💡 プロンプトの黄金律

### 1. 具体的に書く
```
❌ 「動きません」
✅ 「ビルドは成功しますが、NuttShellで'command not found'エラーが出ます」
```

### 2. 段階的に進める
```
❌ 「環境構築からアプリ実装まで全部やって」
✅ 「まず現在のSDKバージョンを確認してください」
```

### 3. コンテキストを共有
```
❌ 「エラーが出ました」
✅ 「make実行時に以下のエラー: [エラー全文を貼り付け]」
```

## 🎯 フェーズ別 必須コマンド

### Phase 1: 環境構築

```bash
# バージョン確認
cat ~/Spr_ws/spresense/sdk/.version
git tag -l | grep v3.4

# アップデート
git checkout v3.4.5
git submodule update --init --recursive

# ツールチェーン確認
arm-none-eabi-gcc --version

# ビルドテスト
./tools/config.py default
make -j4
ls -lh ../nuttx/nuttx.spk
```

**プロンプト例**:
```
現在のSDKバージョンを確認し、v3.4.5へのアップデート手順を
段階的に提示してください。各ステップ後の確認方法も教えてください。
```

### Phase 2: ビルドシステム統合 ★重要★

```bash
# .configファイル探索（最重要！）
find ~/Spr_ws/spresense -name ".config" -exec ls -lh {} \;
# → SDK側とNuttX側の2つがある

# CONFIGURED_APPS確認（デバッグの鍵）
make --dry-run -p 2>&1 | grep CONFIGURED_APPS

# builtin登録確認
make 2>&1 | grep "Register:"
grep -i "アプリ名" apps/builtin/builtin_list.h

# 既存アプリとの比較
diff -u apps/examples/sixaxis/Makefile \
        apps/examples/yourapp/Makefile
```

**最重要プロンプト**:
```
アプリがビルドできますが、NuttShellで実行できません。

【確認したいこと】
1. .configファイルは複数存在しますか？
2. ビルドシステムはどの.configを参照していますか？
3. CONFIGURED_APPSにアプリは含まれていますか？

以下のコマンドで調査してください：
- find コマンドで.config検索
- make --dry-run -p でCONFIGURED_APPS確認
```

### Phase 3: アプリケーション実装

```bash
# センサーデバイス確認
ls -l /dev/accel0 /dev/gyro0

# ビルド
make -j4

# 実行
minicom -D /dev/ttyUSB0 -b 115200
# NuttShell> yourapp
```

**プロンプト例**:
```
BMI160センサーからデータを取得する機能を実装してください。

【要件】
- デバイス: /dev/accel0, /dev/gyro0
- サンプリング: 100Hz
- エラーハンドリング必須

Spresense SDK v3.4.5のAPIに準拠した実装をお願いします。
```

## 🔧 トラブルシューティング・チートシート

| 症状 | 原因候補 | 確認コマンド | プロンプト |
|------|---------|-------------|-----------|
| command not found | builtin未登録 | `grep -i "app" apps/builtin/builtin_list.h` | 「builtin_list.hにエントリがありません。原因を調査してください」 |
| Register: が出ない | Make.defs未反映 | `make --dry-run -p \| grep CONFIGURED` | 「CONFIGURED_APPSに含まれていません。Make.defsを確認してください」 |
| CONFIG変数エラー | 命名規則違反 | `grep CONFIG apps/examples/yourapp/*` | 「CONFIG変数名が正しいか、sixaxisと比較してください」 |
| リンクエラー | ライブラリパス誤り | `cat apps/examples/yourapp/Makefile` | 「外部ライブラリのインクルードパス設定を確認してください」 |

## 📋 必須チェックリスト

### Phase 1完了時
- [ ] `arm-none-eabi-gcc --version` で12.2.1表示
- [ ] `make` が正常完了
- [ ] `nuttx/nuttx.spk` が生成される（約170KB）
- [ ] ビルドログにエラーなし

### Phase 2完了時
- [ ] `make 2>&1 | grep "Register:"` でアプリ名表示
- [ ] `builtin_list.h` にエントリあり
- [ ] `make --dry-run -p | grep CONFIGURED_APPS` にアプリ名あり
- [ ] `nuttx/.config` にCONFIG変数設定済み

### Phase 3完了時
- [ ] ビルド成功
- [ ] NuttShellでアプリ起動
- [ ] センサーデータ取得成功
- [ ] エラーハンドリング動作確認
- [ ] メモリリークなし

## 🎓 重要な発見（Phase 2で特に重要）

### .configの二重構造
```
~/Spr_ws/spresense/
├── sdk/.config          (295B)   ← SDK側（参照されない）
└── nuttx/.config        (68KB)   ← NuttX側（ビルドシステムが参照）★重要★
```

**教訓**: 設定は**NuttX側の.config**に追加すること！

### CONFIG変数の命名規則
```
apps/examples/  → CONFIG_EXAMPLES_*
apps/system/    → CONFIG_SYSTEM_*
apps/platform/  → CONFIG_PLATFORM_*

❌ CONFIG_MYPRO_YOURAPP     （カスタムプレフィックス）
✅ CONFIG_EXAMPLES_YOURAPP  （標準プレフィックス）
```

### Make.defsのパス指定
```makefile
❌ CONFIGURED_APPS += yourapp
✅ CONFIGURED_APPS += examples/yourapp
```

### Makefileの最適化
```makefile
❌ MODULE = $(CONFIG_EXAMPLES_YOURAPP)  # 不要
✅ PROGNAME  = $(CONFIG_EXAMPLES_YOURAPP_PROGNAME)
✅ PRIORITY  = $(CONFIG_EXAMPLES_YOURAPP_PRIORITY)
✅ STACKSIZE = $(CONFIG_EXAMPLES_YOURAPP_STACKSIZE)
```

## 🚨 よくある間違いと即効解決

### 間違い1: SDK側.configのみ設定
```bash
# 確認
ls -lh ~/Spr_ws/spresense/sdk/.config        # 295B
ls -lh ~/Spr_ws/spresense/nuttx/.config      # 68KB

# NuttX側に設定を追加
vi ~/Spr_ws/spresense/nuttx/.config
# CONFIG_EXAMPLES_YOURAPP=y を追加
```

**プロンプト**:
```
「.configファイルが2つあります。どちらに設定すべきですか？
ビルドシステムが参照するファイルを特定してください。」
```

### 間違い2: CONFIG変数名の誤り
```bash
# 確認
grep "CONFIG_" apps/examples/sixaxis/Kconfig
# → CONFIG_EXAMPLES_SIXAXIS を確認

# 修正
vi apps/examples/yourapp/Kconfig
# CONFIG_MYPRO_* → CONFIG_EXAMPLES_* に変更
```

**プロンプト**:
```
「CONFIG変数の命名規則を、動作しているsixaxisアプリと比較してください。
正しいプレフィックスを教えてください。」
```

### 間違い3: Make.defsのパス省略
```makefile
# apps/examples/yourapp/Make.defs

# ❌ 間違い
CONFIGURED_APPS += yourapp

# ✅ 正解
CONFIGURED_APPS += examples/yourapp
```

**プロンプト**:
```
「Make.defsのCONFIGURED_APPSのパス指定を、
sixaxisアプリと比較して正しい形式を教えてください。」
```

## 💬 緊急時のテンプレート

### ビルドエラー時
```
ビルドでエラーが発生しました。

【実行コマンド】
make -j4

【エラーメッセージ】
[エラーの最初の行から最後まで全文貼り付け]

【確認済み】
- ARM GCC: インストール済み
- PATH: 設定済み

【依頼】
エラーの原因を特定し、解決手順を段階的に提案してください。
```

### 実行エラー時
```
アプリがNuttShellで実行できません。

【症状】
nsh> yourapp
nsh: yourapp: command not found

【確認済み】
- ビルド: 成功
- nuttx.spk: 生成済み
- ファームウェア: 書き込み済み

【依頼】
builtin登録が正しく行われているか、以下を調査してください：
1. ビルドログの"Register:"確認
2. builtin_list.hの確認
3. CONFIGURED_APPSの確認
```

### デバッグ支援依頼
```
問題の原因がわかりません。段階的に調査したいです。

【状況】
[現在の状況を具体的に]

【依頼】
優先度の高い調査項目から順に、実行すべきコマンドを提案してください。
各調査結果を報告するので、次の調査を指示してください。
```

## 📊 期待される効果

| 項目 | 従来 | AI活用 | 倍率 |
|------|------|--------|------|
| 環境構築 | 4-6h | 1h | **5x** |
| ビルド統合 | 6-8h | 2h | **4x** |
| アプリ実装 | 8-12h | 3h | **3x** |
| ドキュメント | 6-8h | 0.5h | **12x** |
| **合計** | **24-34h** | **6.5h** | **4x** |

## 🎯 成功のコツ

1. **焦らない**: 段階的に進める
2. **既存例を活用**: sixaxis等の動作例を参照させる
3. **ログを共有**: エラーは全文共有
4. **発見を報告**: 「.configが2つありました」等を伝える
5. **レビュー依頼**: 実装後は必ずコードレビューを依頼

---

**最終更新**: 2025-12-14
**対象**: Spresense SDK v3.4.5
**検証済み**: BMI160アプリ開発プロジェクト
