#!/bin/bash

# Claude Web自動実行システム セットアップスクリプト

QUEUE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "🌐 Claude Web自動実行システム セットアップ"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# 1. Python環境チェック
echo "【1/4】Python環境チェック"
if ! command -v python3 &> /dev/null; then
    echo "❌ python3がインストールされていません"
    echo ""
    echo "インストール方法:"
    echo "  Ubuntu/Debian: sudo apt install python3 python3-pip"
    echo "  macOS: brew install python3"
    exit 1
fi

PYTHON_VERSION=$(python3 --version)
echo "✅ $PYTHON_VERSION"
echo ""

# 2. playwright パッケージインストール
echo "【2/4】playwright パッケージインストール"
if python3 -c "import playwright" 2>/dev/null; then
    echo "✅ playwright (既にインストール済み)"
else
    echo "📦 playwright パッケージをインストール中..."
    pip3 install playwright
    if [ $? -eq 0 ]; then
        echo "✅ インストール完了"
    else
        echo "❌ インストール失敗"
        exit 1
    fi
fi
echo ""

# 3. Chromiumブラウザインストール
echo "【3/4】Chromiumブラウザインストール"
echo "📦 Playwright用ブラウザをインストール中..."
python3 -m playwright install chromium

if [ $? -eq 0 ]; then
    echo "✅ ブラウザインストール完了"
else
    echo "❌ ブラウザインストール失敗"
    exit 1
fi
echo ""

# 4. 初回ログイン設定
echo "【4/4】初回ログイン設定"
echo ""
echo "これからブラウザが起動します。"
echo "Claude.ai にログインしてください。"
echo ""
read -p "準備ができたら Enter を押してください..."

python3 "$QUEUE_DIR/claude_web_executor.py" --setup

if [ $? -eq 0 ]; then
    echo "✅ ログイン設定完了"
else
    echo "❌ ログイン設定失敗"
    echo "後で手動で実行してください:"
    echo "  python3 claude_web_executor.py --setup"
fi
echo ""

# 5. 実行権限設定
chmod +x "$QUEUE_DIR/claude_web_executor.py"
chmod +x "$QUEUE_DIR/claude_executor.sh"
chmod +x "$QUEUE_DIR/queue_manager_v2.sh"

# 6. 設定ファイル作成
if [ ! -f "$QUEUE_DIR/executor_config.json" ]; then
    cat > "$QUEUE_DIR/executor_config.json" <<'EOF'
{
  "executor": "web",
  "note": "executor: api または web を指定"
}
EOF
    echo "✅ 設定ファイル作成: executor_config.json"
fi

# 完了
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "✅ セットアップ完了"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "【次のステップ】"
echo ""
echo "1. タスクを追加:"
echo "   nano prompts.csv"
echo ""
echo "2. テスト実行:"
echo "   ./queue_manager_v2.sh run"
echo ""
echo "3. 自動実行設定（Cron）:"
echo "   ./setup_cron.sh"
echo ""
echo "【切り替え】"
echo "API版に切り替える:"
echo "  ./switch_executor.sh api"
echo ""
echo "Web版に切り替える:"
echo "  ./switch_executor.sh web"
echo ""
