# Spresense ビルドルール - インデックス

**最終更新**: 2025-12-16
**プロジェクト**: Spresense NuttX アプリケーションビルドシステム
**目的**: ビルドエラーを防ぎ、確実にアプリケーションを登録する

---

## 📚 ドキュメント構成

| # | ファイル | 内容 | 用途 |
|---|---------|------|------|
| 00 | [00_INDEX.md](./00_INDEX.md) | 本ファイル - インデックス | 全体把握 |
| 01 | [01_BUILD_RULES.md](./01_BUILD_RULES.md) | **詳細ビルドルール** | ルール理解・参照 |
| 02 | [02_CHECKLIST.md](./02_CHECKLIST.md) | **ビルド前チェックリスト** | ビルド前確認 |
| 03 | [03_TROUBLESHOOTING.md](./03_TROUBLESHOOTING.md) | **トラブルシューティング** | エラー解決 |
| 04 | [04_QUICK_REFERENCE.md](./04_QUICK_REFERENCE.md) | **クイックリファレンス** | 即座の確認 |

---

## 🎯 使い方

### 新規アプリケーション作成時

1. **[02_CHECKLIST.md](./02_CHECKLIST.md)** を開く
2. チェックリストに従って作業
3. 不明点は **[01_BUILD_RULES.md](./01_BUILD_RULES.md)** を参照
4. エラー発生時は **[03_TROUBLESHOOTING.md](./03_TROUBLESHOOTING.md)** を確認

### ビルドエラー発生時

1. **[03_TROUBLESHOOTING.md](./03_TROUBLESHOOTING.md)** で症状を検索
2. 解決策を試す
3. 解決しない場合は **[01_BUILD_RULES.md](./01_BUILD_RULES.md)** で基本ルールを再確認

### コマンド確認時

1. **[04_QUICK_REFERENCE.md](./04_QUICK_REFERENCE.md)** でコマンドをコピペ

---

## 🔑 最重要ポイント (必読!)

### 1. Make.defs のパス指定

**絶対ルール**: `$(APPDIR)/` を使わない!

```makefile
# ❌ 間違い - アプリが登録されない
CONFIGURED_APPS += $(APPDIR)/examples/my_app

# ✅ 正しい - アプリが登録される
CONFIGURED_APPS += examples/my_app
```

### 2. 2重コンフィグ構造

**絶対ルール**: NuttX/.config が実際に使われる!

```
SDK/.config       ← 小さい (295-400B) - ビルドで使われない
NuttX/.config     ← 大きい (60-70KB)  - 実際に使われる ✅
```

### 3. defconfig が大元

**絶対ルール**: 設定変更は defconfig に追加!

```bash
# 設定を追加する場所
/home/ken/Spr_ws/spresense/sdk/configs/default/defconfig
```

### 4. ビルドコマンド

**絶対ルール**: PATH を正しく設定!

```bash
cd /home/ken/Spr_ws/spresense/sdk
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin:$PATH
make
```

---

## 📋 典型的な作業フロー

### フェーズ1: プロジェクト構造作成

1. ディレクトリ作成: `sdk/apps/examples/my_app/`
2. ファイル作成: `Kconfig`, `Makefile`, `Make.defs`, `*.c`, `*.h`

### フェーズ2: ビルドシステム統合

1. `Kconfig` を `examples/Kconfig` に source 追加
2. `defconfig` に CONFIG 追加
3. `Make.defs` でパス指定 ($(APPDIR)/ なし!)

### フェーズ3: ビルド・テスト

1. ビルド: `make`
2. フラッシュ: `sudo tools/flash.sh -c /dev/ttyUSB0 nuttx.spk`
3. 確認: `help` でアプリ表示を確認

---

## ⚠️ よくある失敗パターン

| 失敗 | 原因 | 解決策 |
|-----|------|--------|
| `help` でアプリが表示されない | Make.defs で $(APPDIR)/ 使用 | $(APPDIR)/ を削除 |
| CONFIG が反映されない | SDK/.config を編集 | NuttX/.config を確認 |
| ビルドエラー (リンカ) | Makefile の CSRCS/MAINSRC 誤り | ファイル名を確認 |
| make: command not found | PATH 未設定 | PATH を設定 |

---

## 🚀 クイックスタート

### 最速でアプリを作成・ビルド

```bash
# 1. プロジェクト構造作成
cd /home/ken/Spr_ws/spresense/sdk/apps/examples
mkdir -p my_app
cd my_app

# 2. 既存の成功例をコピー
cp ../bmi160_orientation/Kconfig ./Kconfig
cp ../bmi160_orientation/Makefile ./Makefile
cp ../bmi160_orientation/Make.defs ./Make.defs

# 3. ファイルを編集 (プロジェクト名を置換)

# 4. Kconfig 登録
cd /home/ken/Spr_ws/spresense/sdk/apps/examples
# Kconfig に source 行を追加

# 5. defconfig 設定
cd /home/ken/Spr_ws/spresense/sdk/configs/default
# defconfig に CONFIG 行を追加

# 6. ビルド
cd /home/ken/Spr_ws/spresense/sdk
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin:$PATH
make

# 7. フラッシュ
sudo tools/flash.sh -c /dev/ttyUSB0 nuttx.spk

# 8. 確認
minicom -D /dev/ttyUSB0 -b 115200
# nsh> help
```

---

## 📖 参考ドキュメント

### 本プロジェクト

- `/home/ken/Spr_ws/case_study/12_PHASE1_BUILD_SUCCESS.md` - Phase 1 ビルド成功記録
- `/home/ken/Spr_ws/case_study/prompts/02_build_system_integration.md` - ビルドシステム統合

### 成功例

- `/home/ken/Spr_ws/spresense/sdk/apps/examples/bmi160_orientation/` - BMI160 センサーアプリ
- `/home/ken/Spr_ws/spresense/sdk/apps/examples/security_camera/` - セキュリティカメラアプリ

---

## 🔍 ビルドルールの由来

これらのビルドルールは、以下の実際のトラブルシューティング経験から抽出されました:

### Case Study 1: BMI160 Orientation (2025-12-14)

**問題**: アプリが `help` に表示されない
**原因**: Make.defs で `$(APPDIR)/examples/bmi160_orientation` を使用
**解決**: `examples/bmi160_orientation` に修正

### Case Study 2: Security Camera (2025-12-16)

**問題**: アプリが `help` に表示されない
**原因**: Make.defs で `$(APPDIR)/examples/security_camera` を使用
**解決**: `examples/security_camera` に修正

**パターン**: 同じ失敗が繰り返されている → ビルドルールの文書化が必要

---

## ✅ このドキュメントセットの目的

1. **失敗の再発防止**: 同じミスを繰り返さない
2. **迅速な問題解決**: エラー発生時に即座に解決
3. **知識の体系化**: 散在する知識を一元化
4. **新規開発の加速**: チェックリストで確実に進める

---

**作成日**: 2025-12-16
**作成者**: Claude Code (Sonnet 4.5)
**ベース経験**: BMI160 Orientation + Security Camera プロジェクト
