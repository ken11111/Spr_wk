#!/bin/bash

# Claude Code使用量チェックスクリプト

# 使用量制限の設定（環境に応じて調整）
MAX_REQUESTS_PER_DAY=100
MAX_REQUESTS_PER_HOUR=20

QUEUE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STATE_DIR="$QUEUE_DIR/state"
USAGE_LOG="$STATE_DIR/usage.log"

mkdir -p "$STATE_DIR"
touch "$USAGE_LOG"

# 現在時刻
CURRENT_TIME=$(date +%s)
CURRENT_DATE=$(date +%Y-%m-%d)
CURRENT_HOUR=$(date +%Y-%m-%d-%H)

# 今日の実行回数をカウント
count_today() {
    grep "^$CURRENT_DATE" "$USAGE_LOG" 2>/dev/null | wc -l
}

# 今時間の実行回数をカウント
count_this_hour() {
    grep "^$CURRENT_HOUR" "$USAGE_LOG" 2>/dev/null | wc -l
}

# 使用量チェック
check_limits() {
    local today_count=$(count_today)
    local hour_count=$(count_this_hour)

    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "📊 Claude Code使用量チェック"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "今日の実行回数: $today_count / $MAX_REQUESTS_PER_DAY"
    echo "今時間の実行回数: $hour_count / $MAX_REQUESTS_PER_HOUR"
    echo ""

    # 1日の制限チェック
    if [ $today_count -ge $MAX_REQUESTS_PER_DAY ]; then
        echo "❌ 1日の実行回数制限に達しました"
        echo "明日まで待機してください"
        return 1
    fi

    # 1時間の制限チェック
    if [ $hour_count -ge $MAX_REQUESTS_PER_HOUR ]; then
        echo "⚠️  1時間の実行回数制限に達しました"
        echo "次の時間まで待機してください"
        return 1
    fi

    echo "✅ 使用量OK - 実行可能です"
    echo ""

    # 使用ログに記録
    echo "$CURRENT_HOUR:00,$(date +%s)" >> "$USAGE_LOG"

    return 0
}

# メイン処理
main() {
    check_limits
}

main
