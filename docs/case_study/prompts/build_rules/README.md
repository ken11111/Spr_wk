# Spresense ビルドルール - 完全ガイド

**作成日**: 2025-12-16
**目的**: Spresense NuttX アプリケーションのビルドエラーを防ぎ、確実にビルド・登録する

---

## 📖 このドキュメントセットについて

このディレクトリには、Spresense (NuttX RTOS) でアプリケーションを開発する際の**ビルドルール**と**トラブルシューティングガイド**が含まれています。

実際のプロジェクト (BMI160 Orientation, Security Camera) で発生した問題とその解決策をベースに作成されており、**同じ失敗を繰り返さない**ための実践的なガイドです。

---

## 📚 ドキュメント一覧

| ファイル | 内容 | こんな時に読む |
|---------|------|--------------|
| **[00_INDEX.md](./00_INDEX.md)** | インデックス・概要 | 全体を把握したい |
| **[01_BUILD_RULES.md](./01_BUILD_RULES.md)** | 詳細ビルドルール | ルールを深く理解したい |
| **[02_CHECKLIST.md](./02_CHECKLIST.md)** | ビルド前チェックリスト | 新規アプリを作成する |
| **[03_TROUBLESHOOTING.md](./03_TROUBLESHOOTING.md)** | トラブルシューティング | エラーが発生した |
| **[04_QUICK_REFERENCE.md](./04_QUICK_REFERENCE.md)** | クイックリファレンス | コマンドをコピペしたい |
| **[README.md](./README.md)** | 本ファイル | 初めて読む |

---

## 🎯 推奨される読み方

### 初めてSpresenseアプリを作る方

1. **[README.md](./README.md)** (本ファイル) - 全体像を把握
2. **[01_BUILD_RULES.md](./01_BUILD_RULES.md)** - ルールを理解
3. **[02_CHECKLIST.md](./02_CHECKLIST.md)** - チェックリストに従って作業

### 既にアプリを作っているがエラーが出た方

1. **[03_TROUBLESHOOTING.md](./03_TROUBLESHOOTING.md)** - エラーメッセージで検索
2. 解決しない場合 → **[01_BUILD_RULES.md](./01_BUILD_RULES.md)** で基本ルールを確認

### コマンドだけ知りたい方

1. **[04_QUICK_REFERENCE.md](./04_QUICK_REFERENCE.md)** - コマンドをコピペ

---

## 🔑 最重要ポイント (3つだけ覚える!)

### 1. Make.defs で $(APPDIR)/ を使わない ⭐

**これが最頻出の失敗原因です!**

```makefile
# ❌ 間違い - アプリが登録されない
CONFIGURED_APPS += $(APPDIR)/examples/my_app

# ✅ 正しい - アプリが登録される
CONFIGURED_APPS += examples/my_app
```

### 2. NuttX/.config が実際に使われる ⭐

**SDK/.config ではなく、NuttX/.config を確認すること!**

```
SDK/.config       ← 小さい (295-400B) - 使われない ❌
NuttX/.config     ← 大きい (60-70KB)  - 使われる ✅
```

### 3. defconfig が設定の大元 ⭐

**設定を変更したら defconfig に追加すること!**

```bash
# 設定を追加する場所
/home/ken/Spr_ws/spresense/sdk/configs/default/defconfig
```

---

## 🚀 クイックスタート (5分でアプリ作成)

### ステップ1: プロジェクト作成

```bash
cd /home/ken/Spr_ws/spresense/sdk/apps/examples
mkdir -p my_app
cd my_app
```

### ステップ2: ファイル作成 (既存例からコピー推奨)

```bash
# 成功例からコピー
cp ../bmi160_orientation/Kconfig ./Kconfig
cp ../bmi160_orientation/Makefile ./Makefile
cp ../bmi160_orientation/Make.defs ./Make.defs

# メインファイル作成
cat > my_app_main.c << 'EOF'
#include <nuttx/config.h>
#include <stdio.h>

int main(int argc, FAR char *argv[])
{
    printf("Hello from my_app!\n");
    return 0;
}
EOF
```

### ステップ3: ファイル編集

- `Kconfig`: `bmi160_orientation` → `my_app` に置換
- `Makefile`: `bmi160_orientation` → `my_app` に置換
- `Make.defs`: `bmi160_orientation` → `my_app` に置換

**重要**: Make.defs で `$(APPDIR)/` を使わないこと!

### ステップ4: ビルドシステムに登録

```bash
# 1. examples/Kconfig に追加
nano /home/ken/Spr_ws/spresense/sdk/apps/examples/Kconfig
# 以下を追加:
# source "examples/my_app/Kconfig"

# 2. defconfig に追加
nano /home/ken/Spr_ws/spresense/sdk/configs/default/defconfig
# 以下を追加:
# CONFIG_EXAMPLES_MY_APP=y
# CONFIG_EXAMPLES_MY_APP_PROGNAME="my_app"
# CONFIG_EXAMPLES_MY_APP_PRIORITY=100
# CONFIG_EXAMPLES_MY_APP_STACKSIZE=2048
```

### ステップ5: ビルド・フラッシュ

```bash
cd /home/ken/Spr_ws/spresense/sdk
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin:$PATH
make
sudo tools/flash.sh -c /dev/ttyUSB0 nuttx.spk
```

### ステップ6: 確認

```bash
minicom -D /dev/ttyUSB0 -b 115200
# nsh> help
# → my_app が表示されるはず!
# nsh> my_app
# → Hello from my_app!
```

---

## ⚠️ よくある失敗パターン

### 失敗1: アプリが help に表示されない (最頻出!)

**原因**: Make.defs で `$(APPDIR)/` を使用

**解決**:
```bash
# Make.defs を確認
cat /home/ken/Spr_ws/spresense/sdk/apps/examples/my_app/Make.defs

# $(APPDIR)/ を削除
CONFIGURED_APPS += examples/my_app  # ✅ 正しい
```

詳細: [03_TROUBLESHOOTING.md - アプリが help に表示されない](./03_TROUBLESHOOTING.md#1-アプリが-help-に表示されない)

---

### 失敗2: 設定が反映されない

**原因**: SDK/.config を見ている (間違い)

**解決**:
```bash
# ❌ 間違い
cat /home/ken/Spr_ws/spresense/sdk/.config

# ✅ 正しい
cat /home/ken/Spr_ws/spresense/nuttx/.config | grep MY_APP
```

詳細: [01_BUILD_RULES.md - 2重コンフィグ構造](./01_BUILD_RULES.md#6-2重コンフィグ構造-最重要)

---

### 失敗3: ビルドエラー (arm-none-eabi-gcc not found)

**原因**: PATH 未設定

**解決**:
```bash
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin:$PATH
make
```

詳細: [03_TROUBLESHOOTING.md - ビルドエラー](./03_TROUBLESHOOTING.md#2-ビルドエラー)

---

## 📊 ドキュメントの使い分け

### 新規アプリケーション作成

```
1. README.md (本ファイル) で全体把握
   ↓
2. 02_CHECKLIST.md でステップバイステップ作業
   ↓
3. 04_QUICK_REFERENCE.md でコマンドコピペ
```

### エラー発生時

```
1. 03_TROUBLESHOOTING.md でエラー検索
   ↓
2. 解決しない → 01_BUILD_RULES.md で基本ルール確認
   ↓
3. それでも解決しない → 成功例と比較
```

### ルールを深く理解したい

```
1. 01_BUILD_RULES.md を精読
   ↓
2. 実際のプロジェクト (bmi160_orientation, security_camera) を参照
```

---

## 🔍 このドキュメントセットの特徴

### 1. 実践的

- 実際のプロジェクト (BMI160 Orientation, Security Camera) での失敗経験をベースに作成
- 同じ失敗が2回繰り返された → ドキュメント化の必要性

### 2. 体系的

- ルール、チェックリスト、トラブルシューティング、リファレンスの4本柱
- 目的に応じて最適なドキュメントにアクセス可能

### 3. コピペ可能

- コマンドやテンプレートはそのままコピペ可能
- 即座に作業を開始できる

### 4. 失敗パターン重視

- 成功例だけでなく、失敗パターンも明示
- 「なぜダメなのか」を説明

---

## 📚 参考プロジェクト

このドキュメントセットは、以下の実際のプロジェクトから抽出されました:

### 1. BMI160 Orientation (2025-12-14)

- **場所**: `/home/ken/Spr_ws/spresense/sdk/apps/examples/bmi160_orientation/`
- **問題**: アプリが help に表示されない
- **原因**: Make.defs で `$(APPDIR)/` を使用
- **解決**: `$(APPDIR)/` を削除

### 2. Security Camera (2025-12-16)

- **場所**: `/home/ken/Spr_ws/spresense/sdk/apps/examples/security_camera/`
- **問題**: アプリが help に表示されない
- **原因**: Make.defs で `$(APPDIR)/` を使用 (同じ失敗!)
- **解決**: `$(APPDIR)/` を削除

**教訓**: 同じ失敗が繰り返された → ビルドルールの文書化が必要

---

## 🎯 このドキュメントセットの目的

1. **失敗の再発防止**: 同じミスを繰り返さない
2. **迅速な問題解決**: エラー発生時に即座に解決
3. **知識の体系化**: 散在する知識を一元化
4. **新規開発の加速**: チェックリストで確実に進める

---

## 🚨 緊急時の対処

### アプリが help に表示されない

1. **[03_TROUBLESHOOTING.md](./03_TROUBLESHOOTING.md#1-アプリが-help-に表示されない)** を開く
2. 原因1から順番に確認
3. ほぼ100%、Make.defs の問題

### ビルドエラーが解決しない

1. **[03_TROUBLESHOOTING.md](./03_TROUBLESHOOTING.md#2-ビルドエラー)** でエラーメッセージを検索
2. 成功例と比較:
   ```bash
   diff /home/ken/Spr_ws/spresense/sdk/apps/examples/bmi160_orientation/Makefile \
        /home/ken/Spr_ws/spresense/sdk/apps/examples/my_app/Makefile
   ```

---

## 📞 サポート

### 詳細ドキュメント

- **[01_BUILD_RULES.md](./01_BUILD_RULES.md)** - 各ルールの詳細説明
- **[03_TROUBLESHOOTING.md](./03_TROUBLESHOOTING.md)** - 全エラーパターン

### 成功例

- `/home/ken/Spr_ws/spresense/sdk/apps/examples/bmi160_orientation/`
- `/home/ken/Spr_ws/spresense/sdk/apps/examples/security_camera/`

### ケーススタディ

- `/home/ken/Spr_ws/case_study/12_PHASE1_BUILD_SUCCESS.md` - Phase 1 ビルド成功記録
- `/home/ken/Spr_ws/case_study/prompts/02_build_system_integration.md` - ビルドシステム統合

---

## ✅ まとめ

このビルドルールドキュメントセットを使うことで:

1. **Make.defs の罠** にハマらない
2. **2重コンフィグ構造** を理解できる
3. **ビルドエラー** を迅速に解決できる
4. **新規アプリ** を確実に作成できる

**最重要**: Make.defs で `$(APPDIR)/` を使わない!

---

**作成日**: 2025-12-16
**作成者**: Claude Code (Sonnet 4.5)
**ベース経験**: BMI160 Orientation + Security Camera プロジェクト

**次のステップ**: [02_CHECKLIST.md](./02_CHECKLIST.md) でアプリを作成してみましょう!
