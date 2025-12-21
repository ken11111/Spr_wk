# スクリプト自動化 × AIプロンプト ハイブリッド効率化戦略

## 概要

Claude Codeとの対話（トークン消費）と、スクリプト自動化（トークン消費なし）を最適に組み合わせることで、**さらなる効率化とコスト削減**を実現します。

## 基本方針

```
【原則】
定型作業 → スクリプト自動化（トークン消費0）
判断必要 → AI対話（トークン消費）
```

### トークン消費の比較

| アプローチ | トークン消費 | 時間 | 適用場面 |
|-----------|------------|------|---------|
| AI対話のみ | 大 (72,000) | 6.5h | 全てAIに質問 |
| 最適化プロンプト | 中 (25,000) | 2.5-4h | 構造化された質問 |
| **ハイブリッド** | **小 (10,000)** | **1.5-2.5h** | **スクリプト+AI** |

## Phase別の自動化戦略

### Phase 1: 環境構築（自動化効果: 大）

#### スクリプト化すべきタスク ✅

##### 1.1 環境診断スクリプト

```bash
#!/bin/bash
# check_environment.sh - 環境診断を自動実行

echo "=== Spresense開発環境診断 ==="
echo ""

# SDKバージョン確認
echo "【SDKバージョン】"
if [ -f ~/Spr_ws/spresense/sdk/.version ]; then
    cat ~/Spr_ws/spresense/sdk/.version
else
    echo "SDK .versionファイルなし"
fi
echo ""

# Gitタグ確認
echo "【利用可能なバージョン】"
cd ~/Spr_ws/spresense
git tag -l | grep v3.4
echo ""

# ツールチェーン確認
echo "【ARM GCCツールチェーン】"
if command -v arm-none-eabi-gcc &> /dev/null; then
    arm-none-eabi-gcc --version | head -1
else
    echo "arm-none-eabi-gcc: 未インストール"
fi
echo ""

# サブモジュール状態確認
echo "【サブモジュール状態】"
git submodule status | head -5
echo ""

# PATH確認
echo "【PATH設定】"
echo $PATH | grep -o "spresenseenv" || echo "spresenseenv: PATHに未設定"
echo ""

echo "=== 診断完了 ==="
```

**効果**:
- AI対話なし（トークン消費: 0）
- 実行時間: 5秒
- 従来のAI対話: 2-3往復、約500トークン削減

##### 1.2 環境更新自動スクリプト

```bash
#!/bin/bash
# update_sdk.sh - SDK更新を自動実行

set -e  # エラーで停止

TARGET_VERSION="v3.4.5"

echo "=== Spresense SDK更新スクリプト ==="
echo "目標バージョン: $TARGET_VERSION"
echo ""

# 現在位置確認
cd ~/Spr_ws/spresense

# 1. リポジトリ更新
echo "[1/4] リポジトリ情報更新中..."
git fetch --all --tags
echo "✓ 完了"
echo ""

# 2. バージョン切り替え
echo "[2/4] $TARGET_VERSION にチェックアウト中..."
git checkout $TARGET_VERSION
echo "✓ 完了"
echo ""

# 3. サブモジュール更新
echo "[3/4] サブモジュール更新中..."
git submodule sync --recursive
git submodule update --init --recursive
echo "✓ 完了"
echo ""

# 4. 確認
echo "[4/4] 更新確認中..."
cat sdk/.version
echo ""

echo "=== 更新完了 ==="
echo ""
echo "次のステップ:"
echo "  1. cd ~/Spr_ws/spresense/sdk"
echo "  2. ./tools/config.py default"
echo "  3. make -j4"
```

**効果**:
- AI対話なし（トークン消費: 0）
- 実行時間: 30-60秒
- 従来のAI対話: 5-6往復、約2,000トークン削減

#### AI対話が必要なタスク 🤖

- **トラブルシューティング**: エラーの原因分析と解決策提案
- **選択肢の判断**: 複数の設定オプションから最適なものを選ぶ
- **バージョン互換性**: 特定バージョン間の変更点の説明

---

### Phase 2: ビルドシステム統合（自動化効果: 中）

#### スクリプト化すべきタスク ✅

##### 2.1 ビルドシステム診断スクリプト

```bash
#!/bin/bash
# diagnose_builtin.sh - builtin登録診断

APP_NAME="bmi160_orientation"
APP_PATH="examples/$APP_NAME"

echo "=== NuttX Builtin登録診断 ==="
echo "アプリ名: $APP_NAME"
echo ""

# 1. .config ファイル検索
echo "【1. .configファイル検索】"
find ~/Spr_ws/spresense -name ".config" -exec ls -lh {} \;
echo ""

# 2. CONFIG変数確認
echo "【2. CONFIG変数確認】"
echo "SDK側 .config:"
grep -i "$APP_NAME" ~/Spr_ws/spresense/sdk/.config 2>/dev/null || echo "  設定なし"
echo ""
echo "NuttX側 .config:"
grep -i "$APP_NAME" ~/Spr_ws/spresense/nuttx/.config 2>/dev/null || echo "  設定なし"
echo ""

# 3. CONFIGURED_APPS確認
echo "【3. CONFIGURED_APPS確認】"
cd ~/Spr_ws/spresense/sdk
make --dry-run -p 2>&1 | grep "CONFIGURED_APPS.*=.*$APP_NAME" || echo "  CONFIGURED_APPSに未登録"
echo ""

# 4. builtin_list.h確認
echo "【4. builtin_list.h確認】"
grep -i "$APP_NAME" apps/builtin/builtin_list.h 2>/dev/null || echo "  builtin_list.hにエントリなし"
echo ""

# 5. ビルドログでRegister確認
echo "【5. 最終ビルドログでRegister確認】"
if [ -f build.log ]; then
    grep "Register:.*$APP_NAME" build.log || echo "  Registerログなし"
else
    echo "  build.logファイルなし（make 2>&1 | tee build.log で作成）"
fi
echo ""

echo "=== 診断完了 ==="
echo ""
echo "【診断結果の見方】"
echo "✓ NuttX側.configに設定あり → 正常"
echo "✓ CONFIGURED_APPSに含まれる → 正常"
echo "✓ builtin_list.hにエントリあり → 正常"
echo "✓ Registerログあり → 正常"
echo ""
echo "問題がある場合、この診断結果をClaude Codeに共有してください"
```

**効果**:
- AI対話: 診断結果の解釈のみ（1-2往復）
- トークン削減: 約4,000トークン（診断コマンドをAIに逐次依頼しない）
- 実行時間: 10秒

##### 2.2 アプリケーション登録チェックリスト

```bash
#!/bin/bash
# check_integration.sh - 統合チェックリスト自動確認

APP_NAME="bmi160_orientation"
APP_DIR="apps/examples/$APP_NAME"

cd ~/Spr_ws/spresense/sdk

echo "=== ビルドシステム統合チェックリスト ==="
echo ""

check_mark="✓"
cross_mark="✗"

# 1. ディレクトリ存在確認
if [ -d "$APP_DIR" ]; then
    echo "[$check_mark] 1. アプリディレクトリ存在: $APP_DIR"
else
    echo "[$cross_mark] 1. アプリディレクトリなし: $APP_DIR"
fi

# 2. 必須ファイル確認
for file in Kconfig Makefile Make.defs; do
    if [ -f "$APP_DIR/$file" ]; then
        echo "[$check_mark] 2. $file 存在"
    else
        echo "[$cross_mark] 2. $file なし"
    fi
done

# 3. CONFIG変数名チェック
if grep -q "CONFIG_EXAMPLES_" "$APP_DIR/Kconfig" 2>/dev/null; then
    echo "[$check_mark] 3. CONFIG_EXAMPLES_* プレフィックス使用"
else
    echo "[$cross_mark] 3. CONFIG命名規則違反（EXAMPLES_*を使用すべき）"
fi

# 4. Make.defsパスチェック
if grep -q "examples/$APP_NAME" "$APP_DIR/Make.defs" 2>/dev/null; then
    echo "[$check_mark] 4. Make.defs パス正しい（examples/$APP_NAME）"
else
    echo "[$cross_mark] 4. Make.defs パス誤り（examples/プレフィックス必要）"
fi

# 5. MODULE変数チェック（使用していないことを確認）
if grep -q "^MODULE" "$APP_DIR/Makefile" 2>/dev/null; then
    echo "[$cross_mark] 5. MODULE変数使用（削除推奨）"
else
    echo "[$check_mark] 5. MODULE変数未使用（正しい）"
fi

# 6. 親Kconfigへの登録確認
if grep -q "examples/$APP_NAME/Kconfig" apps/examples/Kconfig 2>/dev/null; then
    echo "[$check_mark] 6. 親Kconfig登録済み"
else
    echo "[$cross_mark] 6. 親Kconfig未登録"
fi

# 7. NuttX側.config確認
if grep -q "CONFIG_EXAMPLES_${APP_NAME^^}" ~/Spr_ws/spresense/nuttx/.config 2>/dev/null; then
    echo "[$check_mark] 7. NuttX側.config設定済み"
else
    echo "[$cross_mark] 7. NuttX側.config未設定（最重要）"
fi

echo ""
echo "=== チェック完了 ==="
echo ""
echo "全て [$check_mark] なら統合成功です"
echo "[$cross_mark] がある場合、該当項目を修正してください"
```

**効果**:
- 一目で問題箇所を特定
- AI対話: 問題の解決方法のみ質問（1-2往復）
- トークン削減: 約3,000トークン

#### AI対話が必要なタスク 🤖

- **既存アプリとの比較**: sixaxisとの構造的な違いの分析
- **エラーメッセージ解釈**: ビルドエラーの原因特定と解決策
- **最適な修正方法**: 複数の修正案から最適なものを選択

---

### Phase 3: アプリケーション実装（自動化効果: 小〜中）

#### スクリプト化すべきタスク ✅

##### 3.1 開発環境セットアップ

```bash
#!/bin/bash
# setup_app_skeleton.sh - アプリケーションスケルトン作成

APP_NAME=$1
APP_TITLE=$2

if [ -z "$APP_NAME" ] || [ -z "$APP_TITLE" ]; then
    echo "使用法: $0 <app_name> <app_title>"
    echo "例: $0 bmi160_orientation \"BMI160 Orientation\""
    exit 1
fi

APP_DIR="apps/examples/$APP_NAME"

cd ~/Spr_ws/spresense/sdk

echo "=== アプリケーションスケルトン作成 ==="
echo "アプリ名: $APP_NAME"
echo "タイトル: $APP_TITLE"
echo ""

# 1. ディレクトリ作成
echo "[1/5] ディレクトリ作成..."
mkdir -p "$APP_DIR"

# 2. Kconfig生成
echo "[2/5] Kconfig生成..."
cat > "$APP_DIR/Kconfig" <<EOF
config EXAMPLES_${APP_NAME^^}
    tristate "$APP_TITLE"
    default n
    ---help---
        Enable $APP_TITLE application.

if EXAMPLES_${APP_NAME^^}

config EXAMPLES_${APP_NAME^^}_PROGNAME
    string "Program name"
    default "$APP_NAME"

config EXAMPLES_${APP_NAME^^}_PRIORITY
    int "Task priority"
    default 100

config EXAMPLES_${APP_NAME^^}_STACKSIZE
    int "Stack size"
    default 2048

endif
EOF

# 3. Makefile生成
echo "[3/5] Makefile生成..."
cat > "$APP_DIR/Makefile" <<EOF
include \$(APPDIR)/Make.defs

PROGNAME  = \$(CONFIG_EXAMPLES_${APP_NAME^^}_PROGNAME)
PRIORITY  = \$(CONFIG_EXAMPLES_${APP_NAME^^}_PRIORITY)
STACKSIZE = \$(CONFIG_EXAMPLES_${APP_NAME^^}_STACKSIZE)

MAINSRC = ${APP_NAME}_main.c

include \$(APPDIR)/Application.mk
EOF

# 4. Make.defs生成
echo "[4/5] Make.defs生成..."
cat > "$APP_DIR/Make.defs" <<EOF
ifneq (\$(CONFIG_EXAMPLES_${APP_NAME^^}),)
CONFIGURED_APPS += examples/$APP_NAME
endif
EOF

# 5. メインファイルテンプレート生成
echo "[5/5] メインファイルテンプレート生成..."
cat > "$APP_DIR/${APP_NAME}_main.c" <<EOF
#include <nuttx/config.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    printf("$APP_TITLE\\n");
    printf("TODO: Implement application logic\\n");

    return 0;
}
EOF

echo ""
echo "=== スケルトン作成完了 ==="
echo ""
echo "作成されたファイル:"
ls -lh "$APP_DIR"
echo ""
echo "次のステップ:"
echo "  1. apps/examples/Kconfig に source行を追加"
echo "  2. nuttx/.config にCONFIG変数を追加"
echo "  3. ${APP_NAME}_main.c を実装"
```

**効果**:
- 定型的なファイル作成を自動化
- AI対話: ビジネスロジックの実装のみ
- トークン削減: 約1,500トークン
- 時間削減: 15分

##### 3.2 ビルド＆デプロイ自動化

```bash
#!/bin/bash
# build_and_deploy.sh - ビルド・フラッシュ自動実行

DEVICE="/dev/ttyUSB0"
SDK_DIR="$HOME/Spr_ws/spresense/sdk"

cd "$SDK_DIR"

echo "=== Spresense ビルド＆デプロイ自動化 ==="
echo ""

# 1. クリーンビルド
echo "[1/4] ビルド中..."
make clean > /dev/null 2>&1
if make -j4 2>&1 | tee build.log; then
    echo "✓ ビルド成功"
else
    echo "✗ ビルドエラー"
    echo ""
    echo "エラーログ（最後の20行）:"
    tail -20 build.log
    exit 1
fi
echo ""

# 2. Register確認
echo "[2/4] builtin登録確認..."
if grep "Register:" build.log | tail -5; then
    echo "✓ builtin登録確認"
else
    echo "✗ builtin登録なし"
fi
echo ""

# 3. デバイス確認
echo "[3/4] デバイス確認..."
if [ -e "$DEVICE" ]; then
    echo "✓ $DEVICE 存在"
else
    echo "✗ $DEVICE なし"
    echo ""
    echo "WSL2の場合、Windowsで以下を実行:"
    echo "  usbipd attach --wsl --busid <BUSID>"
    exit 1
fi
echo ""

# 4. フラッシュ
echo "[4/4] ファームウェア書き込み中..."
if ./tools/flash.sh -c "$DEVICE" ../nuttx/nuttx.spk; then
    echo "✓ 書き込み成功"
else
    echo "✗ 書き込みエラー"
    exit 1
fi

echo ""
echo "=== デプロイ完了 ==="
echo ""
echo "次のコマンドで接続:"
echo "  minicom -D $DEVICE -b 115200"
```

**効果**:
- ビルド・デプロイを1コマンドで完結
- AI対話: エラー時のみ
- 時間削減: 毎回5-10分

#### AI対話が必要なタスク 🤖

- **アルゴリズム実装**: Madgwick AHRS、位置推定ロジック
- **API使用方法**: Spresense SDK特有のAPI
- **最適化**: パフォーマンス改善、メモリ削減
- **コードレビュー**: 実装の品質チェック

---

## ハイブリッド戦略の実践例

### 実践例1: Phase 1環境構築

#### 従来のAI対話のみ（トークン: 約15,000）
```
ユーザー: SDKを更新したい
AI: 現在のバージョンを確認してください
ユーザー: [確認コマンド実行]
AI: v3.0.0ですね。v3.4.5へのステップは...
... (8-10往復)
```

#### ハイブリッド戦略（トークン: 約2,000）
```
# 1. スクリプトで診断（トークン: 0）
$ ./check_environment.sh
=== 結果表示 ===

# 2. スクリプトで更新（トークン: 0）
$ ./update_sdk.sh
=== 自動実行 ===

# 3. エラー時のみAIに相談（トークン: 2,000）
ユーザー: 「サブモジュール更新でエラーが出ました: [エラー内容]」
AI: [エラー解析と解決策提示]
```

**効果**: トークン87%削減、時間60%削減

---

### 実践例2: Phase 2ビルドシステム統合

#### 従来のAI対話のみ（トークン: 約35,000）
```
15-20往復の試行錯誤...
```

#### ハイブリッド戦略（トークン: 約8,000）
```
# 1. スケルトン自動生成（トークン: 0）
$ ./setup_app_skeleton.sh bmi160_orientation "BMI160 Orientation"

# 2. 統合チェック（トークン: 0）
$ ./check_integration.sh
[$cross_mark] 7. NuttX側.config未設定

# 3. 診断（トークン: 0）
$ ./diagnose_builtin.sh
=== 問題箇所が明確に ===

# 4. AIに具体的な質問（トークン: 2,000）
ユーザー: 「NuttX側.configに設定がありません。
どこにどのように追加すべきか教えてください」
AI: [具体的な追加位置と内容を提示]

# 5. 修正後の確認（トークン: 0）
$ ./check_integration.sh
全て [✓] → 成功！
```

**効果**: トークン77%削減、時間65%削減

---

## スクリプトライブラリの構成

### 推奨フォルダ構造

```
~/Spr_ws/scripts/
├── environment/
│   ├── check_environment.sh      - 環境診断
│   ├── update_sdk.sh             - SDK更新
│   └── install_toolchain.sh      - ツールチェーンインストール
│
├── build_system/
│   ├── setup_app_skeleton.sh     - アプリスケルトン作成
│   ├── check_integration.sh      - 統合チェック
│   └── diagnose_builtin.sh       - builtin診断
│
├── development/
│   ├── build_and_deploy.sh       - ビルド＆デプロイ
│   ├── quick_test.sh             - クイックテスト
│   └── collect_logs.sh           - ログ収集
│
└── README.md                      - スクリプト使用ガイド
```

---

## トークン消費の最適化指針

### 意思決定ツリー

```
タスク発生
    ↓
定型作業？
    ├─ Yes → スクリプト実行（トークン: 0）
    │          ↓
    │        成功？
    │          ├─ Yes → 完了
    │          └─ No → AIに相談（トークン: 小）
    │
    └─ No → 判断必要？
              ├─ Yes → AIに相談（トークン: 中）
              └─ No → ドキュメント確認
```

### タスク分類表

| タスク | 方法 | トークン | 時間 |
|--------|------|---------|------|
| 環境診断 | スクリプト | 0 | 5秒 |
| SDK更新 | スクリプト | 0 | 60秒 |
| エラー解析 | AI | 500-1000 | 2-5分 |
| ビルド確認 | スクリプト | 0 | 10秒 |
| アプリスケルトン作成 | スクリプト | 0 | 5秒 |
| 統合診断 | スクリプト | 0 | 10秒 |
| 問題原因分析 | AI | 1000-2000 | 5-10分 |
| アルゴリズム実装 | AI | 3000-5000 | 15-30分 |
| コードレビュー | AI | 2000-3000 | 10-15分 |
| ビルド＆デプロイ | スクリプト | 0 | 90秒 |

---

## 効果測定

### トークン消費比較

| アプローチ | Phase 1 | Phase 2 | Phase 3 | 合計 | 削減率 |
|-----------|---------|---------|---------|------|--------|
| AI対話のみ | 15,000 | 35,000 | 22,000 | 72,000 | - |
| 最適化プロンプト | 6,000 | 10,000 | 9,000 | 25,000 | 65% |
| **ハイブリッド** | **2,000** | **5,000** | **3,000** | **10,000** | **86%** |

### コスト比較（Claude API料金ベース）

| アプローチ | トークン | コスト | 削減 |
|-----------|---------|--------|------|
| AI対話のみ | 72,000 | $1.29 | - |
| 最適化プロンプト | 25,000 | $0.45 | 65% |
| **ハイブリッド** | **10,000** | **$0.18** | **86%** |

### 時間比較

| アプローチ | Phase 1 | Phase 2 | Phase 3 | 合計 |
|-----------|---------|---------|---------|------|
| AI対話のみ | 90-120分 | 180-240分 | 120-180分 | 6.5-9h |
| 最適化プロンプト | 40-60分 | 60-90分 | 60-90分 | 2.5-4h |
| **ハイブリッド** | **20-30分** | **30-45分** | **40-60分** | **1.5-2.5h** |

---

## スクリプト作成のベストプラクティス

### 1. エラーハンドリング

```bash
#!/bin/bash
set -e  # エラーで即座に停止
set -u  # 未定義変数の使用でエラー

# 関数でのエラーハンドリング
function safe_command() {
    if ! "$@"; then
        echo "エラー: $* の実行に失敗"
        echo "この診断結果をClaude Codeに共有してください"
        exit 1
    fi
}
```

### 2. ユーザーフレンドリーな出力

```bash
# 進捗表示
echo "[1/5] 環境確認中..."
echo "✓ 完了"

# 色付き出力
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${GREEN}✓ 成功${NC}"
echo -e "${RED}✗ エラー${NC}"
```

### 3. AI連携を想定した出力

```bash
# 診断結果を構造化して出力
cat <<EOF
=== 診断結果（Claude Codeに共有用） ===

【環境情報】
SDK: $SDK_VERSION
ツールチェーン: $GCC_VERSION

【検出された問題】
$ISSUES

【推奨される次のステップ】
$NEXT_STEPS
EOF
```

---

## まとめ

### ハイブリッド戦略の効果

```
トークン削減: 86% (72,000 → 10,000)
コスト削減: 86% ($1.29 → $0.18)
時間削減: 72% (6.5-9h → 1.5-2.5h)
```

### 黄金比

```
スクリプト自動化: 70%のタスク
AI対話: 30%のタスク（判断・分析・実装）
```

### 実装優先度

1. **最優先**: Phase 2の診断・チェックスクリプト
   - 効果最大（トークン5,000以上削減）

2. **推奨**: Phase 1の環境管理スクリプト
   - 頻繁に使用、大きな時間削減

3. **有用**: Phase 3のビルド・デプロイスクリプト
   - 反復作業の効率化

---

**作成日**: 2025-12-14
**対象**: Spresense開発の効率化
**推奨度**: ★★★★★
**ROI**: 極めて高い（一度作成すれば永続的に効果）
