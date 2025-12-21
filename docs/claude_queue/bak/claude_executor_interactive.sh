#!/bin/bash

# Claude Code インタラクティブ実行スクリプト
# プロンプトをファイルに出力し、ユーザーに実行を促す

WORKING_DIR="$1"
PROMPT="$2"
TASK_ID="$3"

QUEUE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STATE_DIR="$QUEUE_DIR/state"
OUTPUT_DIR="$STATE_DIR/outputs"
PROMPT_FILE="$QUEUE_DIR/NEXT_TASK.md"
LOG_FILE="$STATE_DIR/execution.log"

mkdir -p "$OUTPUT_DIR"

# 引数チェック
if [ -z "$WORKING_DIR" ] || [ -z "$PROMPT" ]; then
    echo "❌ エラー: 作業ディレクトリとプロンプトが必要です"
    exit 1
fi

# 作業ディレクトリの確認
if [ ! -d "$WORKING_DIR" ]; then
    echo "❌ エラー: 作業ディレクトリが存在しません: $WORKING_DIR"
    exit 1
fi

# プロンプトファイルを生成
cat > "$PROMPT_FILE" <<EOF
# 次のタスク

**タスクID**: $TASK_ID
**作業ディレクトリ**: $WORKING_DIR
**生成時刻**: $(date '+%Y-%m-%d %H:%M:%S')

---

## 📋 実行してください

以下のプロンプトをClaude Codeで実行してください:

\`\`\`
作業ディレクトリ: $WORKING_DIR

$PROMPT
\`\`\`

---

## ✅ 実行後の手順

1. タスクが完了したら、以下のコマンドを実行:
   \`\`\`bash
   cd $QUEUE_DIR
   ./queue_manager_v2.sh run
   \`\`\`

2. 次のタスクが NEXT_TASK.md に出力されます

---

**自動生成**: Claude Code自動実行キューシステム
EOF

# コンソールに表示
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📋 次のタスクを準備しました"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "【タスクID】: $TASK_ID"
echo "【作業ディレクトリ】: $WORKING_DIR"
echo ""
echo "【プロンプト】:"
echo "$PROMPT"
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "📄 詳細は NEXT_TASK.md を確認してください"
echo ""
echo "💡 Claude Codeで以下を実行:"
echo "   「NEXT_TASK.mdを読んで、記載されているタスクを実行してください」"
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# ログに記録
echo "$(date '+%Y-%m-%d %H:%M:%S'),TASK_$TASK_ID,$WORKING_DIR,$PROMPT,READY" >> "$LOG_FILE"

# 特別な終了コードを返す: 99 = 準備完了（実行待ち）
exit 99
