#!/bin/bash

# Claude API自動実行システム セットアップスクリプト

QUEUE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "🚀 Claude API自動実行システム セットアップ"
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

# 2. anthropic パッケージインストール
echo "【2/4】anthropic パッケージインストール"
if python3 -c "import anthropic" 2>/dev/null; then
    ANTHROPIC_VERSION=$(python3 -c "import anthropic; print(anthropic.__version__)")
    echo "✅ anthropic $ANTHROPIC_VERSION (既にインストール済み)"
else
    echo "📦 anthropic パッケージをインストール中..."
    pip3 install anthropic
    if [ $? -eq 0 ]; then
        echo "✅ インストール完了"
    else
        echo "❌ インストール失敗"
        echo ""
        echo "手動でインストールしてください:"
        echo "  pip3 install anthropic"
        exit 1
    fi
fi
echo ""

# 3. APIキー設定
echo "【3/4】APIキー設定"
if [ -f "$QUEUE_DIR/.env" ]; then
    echo "✅ .env ファイルが既に存在します"
    echo ""
    read -p ".env ファイルを再設定しますか？ [y/N]: " -n 1 -r
    echo ""
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "スキップします"
    else
        setup_api_key=true
    fi
else
    setup_api_key=true
fi

if [ "$setup_api_key" = true ]; then
    echo ""
    echo "APIキーの設定方法を選択してください:"
    echo "1) APIキーを入力（推奨）"
    echo "2) 後で手動設定"
    read -p "選択 [1-2]: " -n 1 -r
    echo ""

    case $REPLY in
        1)
            echo ""
            echo "Anthropic APIキーを入力してください:"
            echo "（取得先: https://console.anthropic.com/）"
            read -p "APIキー: " -r API_KEY

            if [ -n "$API_KEY" ]; then
                echo "ANTHROPIC_API_KEY=$API_KEY" > "$QUEUE_DIR/.env"
                echo "✅ .env ファイルに保存しました"
            else
                echo "❌ APIキーが入力されませんでした"
                exit 1
            fi
            ;;
        2)
            echo ""
            echo "手動設定の手順:"
            echo "1. Anthropic APIキーを取得: https://console.anthropic.com/"
            echo "2. .env ファイルを作成:"
            echo "   cd $QUEUE_DIR"
            echo "   echo 'ANTHROPIC_API_KEY=your-api-key' > .env"
            ;;
        *)
            echo "❌ 無効な選択です"
            exit 1
            ;;
    esac
fi
echo ""

# 4. 実行権限設定
echo "【4/4】実行権限設定"
chmod +x "$QUEUE_DIR/claude_api_executor.py"
chmod +x "$QUEUE_DIR/claude_executor.sh"
chmod +x "$QUEUE_DIR/queue_manager_v2.sh"
echo "✅ 実行権限を設定しました"
echo ""

# 完了
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
echo "【ドキュメント】"
echo "- QUICKSTART.md - クイックスタートガイド"
echo "- SPECIFICATION.md - システム仕様"
echo "- API_GUIDE.md - API版の使い方"
echo ""
