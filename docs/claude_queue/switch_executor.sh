#!/bin/bash

# 実行方法切り替えスクリプト（API ⇔ Web）

QUEUE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_FILE="$QUEUE_DIR/executor_config.json"

MODE="$1"

if [ -z "$MODE" ]; then
    echo "使用法: $0 [api|web]"
    echo ""
    echo "現在の設定:"
    if [ -f "$CONFIG_FILE" ]; then
        cat "$CONFIG_FILE"
    else
        echo "設定ファイルがありません"
    fi
    exit 1
fi

case "$MODE" in
    api)
        cat > "$CONFIG_FILE" <<'EOF'
{
  "executor": "api",
  "note": "Anthropic API使用（有料・安定）"
}
EOF
        echo "✅ API版に切り替えました"
        echo ""
        echo "【必要な設定】"
        echo "- APIキー: .env ファイルに ANTHROPIC_API_KEY を設定"
        echo "- セットアップ: ./setup_api.sh"
        ;;

    web)
        cat > "$CONFIG_FILE" <<'EOF'
{
  "executor": "web",
  "note": "Web自動化使用（無料・Claude Code使用量活用）"
}
EOF
        echo "✅ Web版に切り替えました"
        echo ""
        echo "【必要な設定】"
        echo "- 初回ログイン: python3 claude_web_executor.py --setup"
        echo "- セットアップ: ./setup_web.sh"
        ;;

    *)
        echo "❌ エラー: 無効なモード: $MODE"
        echo "使用法: $0 [api|web]"
        exit 1
        ;;
esac

echo ""
echo "【確認】"
cat "$CONFIG_FILE"
