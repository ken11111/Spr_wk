#!/bin/bash

# Claude Code使用量チェックスクリプト v2（5時間リセット対応）

# Claude Code使用量制限
# 5時間ごとにリセットされる
RESET_INTERVAL=18000  # 5時間 = 18000秒
MAX_REQUESTS_PER_WINDOW=50  # 5時間ウィンドウあたりの最大実行回数

QUEUE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STATE_DIR="$QUEUE_DIR/state"
USAGE_LOG="$STATE_DIR/usage.log"
WINDOW_LOG="$STATE_DIR/usage_windows.log"

mkdir -p "$STATE_DIR"
touch "$USAGE_LOG"
touch "$WINDOW_LOG"

# 現在時刻
CURRENT_TIME=$(date +%s)

# 現在の5時間ウィンドウの開始時刻を計算
get_current_window_start() {
    local current=$1
    # 5時間 = 18000秒でウィンドウを区切る
    # 0:00からの経過時間を5時間で割って、開始時刻を計算
    local window_id=$((current / RESET_INTERVAL))
    echo $((window_id * RESET_INTERVAL))
}

# 現在のウィンドウの実行回数をカウント
count_current_window() {
    local window_start=$(get_current_window_start $CURRENT_TIME)
    local window_end=$((window_start + RESET_INTERVAL))

    # usage.logから現在のウィンドウ内の実行をカウント
    local count=0
    while IFS=',' read -r timestamp; do
        if [ -n "$timestamp" ] && [ "$timestamp" -ge "$window_start" ] && [ "$timestamp" -lt "$window_end" ]; then
            count=$((count + 1))
        fi
    done < "$USAGE_LOG"

    echo $count
}

# 次のウィンドウ開始時刻を取得
get_next_window_start() {
    local window_start=$(get_current_window_start $CURRENT_TIME)
    echo $((window_start + RESET_INTERVAL))
}

# 使用量チェック
check_limits() {
    local window_start=$(get_current_window_start $CURRENT_TIME)
    local window_end=$((window_start + RESET_INTERVAL))
    local next_window=$(get_next_window_start)
    local current_count=$(count_current_window)

    local window_start_str=$(date -d "@$window_start" '+%Y-%m-%d %H:%M:%S')
    local window_end_str=$(date -d "@$window_end" '+%Y-%m-%d %H:%M:%S')
    local next_window_str=$(date -d "@$next_window" '+%Y-%m-%d %H:%M:%S')

    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "📊 Claude Code使用量チェック（5時間リセット対応）"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "現在時刻: $(date '+%Y-%m-%d %H:%M:%S')"
    echo ""
    echo "【現在の5時間ウィンドウ】"
    echo "  開始: $window_start_str"
    echo "  終了: $window_end_str"
    echo "  実行回数: $current_count / $MAX_REQUESTS_PER_WINDOW"
    echo ""

    # 制限チェック
    if [ $current_count -ge $MAX_REQUESTS_PER_WINDOW ]; then
        local remaining=$((window_end - CURRENT_TIME))
        local remaining_minutes=$((remaining / 60))

        echo "❌ 使用量制限に達しました"
        echo ""
        echo "【次回実行可能時刻】"
        echo "  $next_window_str"
        echo "  （あと ${remaining_minutes}分後）"
        echo ""
        echo "【推奨リトライ時刻】"
        local retry_time=$((window_end - 1800))  # 30分前（安全マージン）
        local retry_time_str=$(date -d "@$retry_time" '+%Y-%m-%d %H:%M:%S')
        echo "  $retry_time_str"
        echo "  （リセット30分前 = 確実に実行可能）"
        echo ""

        # 次回実行可能時刻を返す（終了コード2で）
        echo "$next_window_str" > "$STATE_DIR/next_available.txt"
        return 2
    fi

    echo "✅ 使用量OK - 実行可能です"
    echo ""
    echo "【残り実行可能回数】"
    echo "  $((MAX_REQUESTS_PER_WINDOW - current_count))回"
    echo "  （このウィンドウが終了するまで）"
    echo ""

    # 使用ログに記録
    echo "$CURRENT_TIME" >> "$USAGE_LOG"

    # ウィンドウログに記録
    echo "$window_start,$window_end,$current_count,$MAX_REQUESTS_PER_WINDOW" >> "$WINDOW_LOG"

    return 0
}

# 古いログのクリーンアップ（24時間より古いエントリを削除）
cleanup_old_logs() {
    local cutoff=$((CURRENT_TIME - 86400))  # 24時間前

    # usage.logのクリーンアップ
    awk -v cutoff="$cutoff" -F',' '$1 >= cutoff' "$USAGE_LOG" > "$USAGE_LOG.tmp"
    mv "$USAGE_LOG.tmp" "$USAGE_LOG"

    # window_logのクリーンアップ
    awk -v cutoff="$cutoff" -F',' '$2 >= cutoff' "$WINDOW_LOG" > "$WINDOW_LOG.tmp"
    mv "$WINDOW_LOG.tmp" "$WINDOW_LOG"
}

# メイン処理
main() {
    # 古いログをクリーンアップ
    cleanup_old_logs

    # 使用量チェック
    check_limits
    return $?
}

main "$@"
