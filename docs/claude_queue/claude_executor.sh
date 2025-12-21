#!/bin/bash

# Claude実行スクリプト（API / Web 自動切り替え）
# 使用法: ./claude_executor.sh <working_dir> <prompt> <task_id>

WORKING_DIR="$1"
PROMPT="$2"
TASK_ID="$3"

QUEUE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STATE_DIR="$QUEUE_DIR/state"
OUTPUT_DIR="$STATE_DIR/outputs"
LOG_FILE="$STATE_DIR/execution.log"
CONFIG_FILE="$QUEUE_DIR/executor_config.json"

# 出力ディレクトリ作成
mkdir -p "$OUTPUT_DIR"

# 引数チェック
if [ -z "$WORKING_DIR" ] || [ -z "$PROMPT" ]; then
    echo "❌ エラー: 作業ディレクトリとプロンプトが必要です"
    echo "使用法: $0 <working_dir> <prompt> <task_id>"
    exit 1
fi

# 作業ディレクトリの確認
if [ ! -d "$WORKING_DIR" ]; then
    echo "❌ エラー: 作業ディレクトリが存在しません: $WORKING_DIR"
    exit 1
fi

# 実行モードを取得
get_executor_mode() {
    if [ -f "$CONFIG_FILE" ]; then
        # jqがあれば使用
        if command -v jq &> /dev/null; then
            jq -r '.executor' "$CONFIG_FILE" 2>/dev/null || echo "web"
        else
            # jqがない場合はgrepで抽出
            grep -oP '"executor":\s*"\K[^"]+' "$CONFIG_FILE" 2>/dev/null || echo "web"
        fi
    else
        # デフォルトはweb
        echo "web"
    fi
}

EXECUTOR_MODE=$(get_executor_mode)

echo "🤖 Claude実行 (モード: $EXECUTOR_MODE)"
echo ""

# Python環境チェック
check_python() {
    if ! command -v python3 &> /dev/null; then
        echo "❌ エラー: python3がインストールされていません"
        exit 1
    fi
}

# 使用量チェック（オプション）
check_usage() {
    # 使用量チェックスクリプトがあれば実行
    if [ -x "$QUEUE_DIR/check_usage_v2.sh" ]; then
        "$QUEUE_DIR/check_usage_v2.sh"
        return $?
    elif [ -x "$QUEUE_DIR/check_usage.sh" ]; then
        "$QUEUE_DIR/check_usage.sh"
        return $?
    fi
    # スクリプトがなければスキップ
    return 0
}

# API版実行
execute_api() {
    echo "📡 API版で実行"
    echo ""
    echo "タスクID: $TASK_ID"
    echo "作業ディレクトリ: $WORKING_DIR"
    echo "プロンプト: ${PROMPT:0:100}..."
    echo ""

    # anthropic パッケージチェック
    if ! python3 -c "import anthropic" 2>/dev/null; then
        echo "❌ エラー: anthropic パッケージがインストールされていません"
        echo ""
        echo "インストール方法:"
        echo "  pip install anthropic"
        echo ""
        echo "または Web版に切り替え:"
        echo "  ./switch_executor.sh web"
        exit 1
    fi

    # Python実行スクリプトを呼び出し
    python3 "$QUEUE_DIR/claude_api_executor.py" "$WORKING_DIR" "$PROMPT" "$TASK_ID"
    local exit_code=$?

    # ログに記録
    echo "$(date '+%Y-%m-%d %H:%M:%S'),TASK_$TASK_ID,$WORKING_DIR,$PROMPT,EXIT_$exit_code,MODE_API" >> "$LOG_FILE"

    return $exit_code
}

# Web版実行
execute_web() {
    echo "🌐 Web版で実行"
    echo ""
    echo "タスクID: $TASK_ID"
    echo "作業ディレクトリ: $WORKING_DIR"
    echo "プロンプト: ${PROMPT:0:100}..."
    echo ""

    # playwright パッケージチェック
    if ! python3 -c "import playwright" 2>/dev/null; then
        echo "❌ エラー: playwright パッケージがインストールされていません"
        echo ""
        echo "インストール方法:"
        echo "  pip install playwright"
        echo "  playwright install chromium"
        echo ""
        echo "または API版に切り替え:"
        echo "  ./switch_executor.sh api"
        exit 1
    fi

    # セッションファイル確認
    if [ ! -f "$QUEUE_DIR/session.json" ]; then
        echo "❌ エラー: ログインセッションがありません"
        echo ""
        echo "初回セットアップ:"
        echo "  python3 claude_web_executor.py --setup"
        echo ""
        echo "または:"
        echo "  ./setup_web.sh"
        exit 1
    fi

    # Python実行スクリプトを呼び出し
    python3 "$QUEUE_DIR/claude_web_executor.py" "$WORKING_DIR" "$PROMPT" "$TASK_ID"
    local exit_code=$?

    # ログに記録
    echo "$(date '+%Y-%m-%d %H:%M:%S'),TASK_$TASK_ID,$WORKING_DIR,$PROMPT,EXIT_$exit_code,MODE_WEB" >> "$LOG_FILE"

    return $exit_code
}

# メイン処理
main() {
    # Python環境チェック
    check_python

    # 使用量チェック
    if ! check_usage; then
        echo "⚠️  使用量制限に達しています"
        echo "後ほど再試行してください"
        exit 2
    fi

    # モード別に実行
    case "$EXECUTOR_MODE" in
        api)
            execute_api
            ;;
        web)
            execute_web
            ;;
        *)
            echo "❌ エラー: 不明な実行モード: $EXECUTOR_MODE"
            echo "executor_config.json を確認してください"
            exit 1
            ;;
    esac

    local exit_code=$?

    echo ""
    case $exit_code in
        0)
            echo "✅ 正常終了"
            ;;
        1)
            echo "❌ エラー終了"
            ;;
        2)
            echo "⏸️  使用量制限到達"
            ;;
        *)
            echo "❌ 不明なエラー (コード: $exit_code)"
            ;;
    esac

    return $exit_code
}

main
