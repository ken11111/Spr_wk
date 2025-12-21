#!/bin/bash

# Claude Code 自動実行キュー管理スクリプト（繰り返し実行対応）
# 使用法: ./queue_manager.sh [run|status|add|reset]

QUEUE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CSV_FILE="$QUEUE_DIR/prompts.csv"
STATE_DIR="$QUEUE_DIR/state"
EXECUTED_LOG="$STATE_DIR/executed.log"
REPEAT_LOG="$STATE_DIR/repeat_tasks.log"
LOCK_FILE="$STATE_DIR/queue.lock"

# 初期化
mkdir -p "$STATE_DIR"
touch "$EXECUTED_LOG"
touch "$REPEAT_LOG"

# ロックファイルチェック（多重実行防止）
check_lock() {
    if [ -f "$LOCK_FILE" ]; then
        local pid=$(cat "$LOCK_FILE")
        if ps -p "$pid" > /dev/null 2>&1; then
            echo "⚠️  既に実行中です (PID: $pid)"
            exit 1
        else
            echo "🔓 古いロックを削除"
            rm -f "$LOCK_FILE"
        fi
    fi
}

# ロック取得
acquire_lock() {
    echo $$ > "$LOCK_FILE"
}

# ロック解放
release_lock() {
    rm -f "$LOCK_FILE"
}

# repeat_intervalを秒数に変換
interval_to_seconds() {
    local interval="$1"

    case "$interval" in
        *h)
            # 時間単位（例: 12h）
            echo $(( ${interval%h} * 3600 ))
            ;;
        *d|daily)
            # 日単位（例: 2d, daily）
            if [ "$interval" = "daily" ]; then
                echo 86400
            else
                echo $(( ${interval%d} * 86400 ))
            fi
            ;;
        *w|weekly)
            # 週単位（例: 2w, weekly）
            if [ "$interval" = "weekly" ]; then
                echo 604800
            else
                echo $(( ${interval%w} * 604800 ))
            fi
            ;;
        *m|monthly)
            # 月単位（例: monthly）
            echo 2592000  # 30日として計算
            ;;
        *)
            # デフォルト（数値のみの場合は秒）
            echo "$interval"
            ;;
    esac
}

# 繰り返しタスクが実行可能かチェック
can_run_repeat_task() {
    local id="$1"
    local repeat_interval="$2"
    local current_time=$(date +%s)

    # 繰り返し実行ログから最終実行時刻を取得
    local last_run=$(grep "^$id," "$REPEAT_LOG" 2>/dev/null | tail -n 1 | cut -d',' -f2)

    if [ -z "$last_run" ]; then
        # 未実行の場合は実行可能
        return 0
    fi

    # インターバルを秒数に変換
    local interval_seconds=$(interval_to_seconds "$repeat_interval")
    local next_run_time=$((last_run + interval_seconds))

    if [ $current_time -ge $next_run_time ]; then
        return 0  # 実行可能
    else
        return 1  # まだ実行できない
    fi
}

# CSVから次のタスクを取得
get_next_task() {
    local current_time=$(date +%s)

    # CSVを読み込み（ヘッダースキップ）
    tail -n +2 "$CSV_FILE" | while IFS=',' read -r id priority working_dir prompt status scheduled_time dependencies repeat repeat_interval; do
        # statusがpending以外ならスキップ
        if [ "$status" != "pending" ]; then
            continue
        fi

        # 繰り返し実行タスクの処理
        if [ "$repeat" = "yes" ]; then
            # 繰り返し実行可能かチェック
            if ! can_run_repeat_task "$id" "$repeat_interval"; then
                continue  # まだ実行できない
            fi
        else
            # 通常タスク：すでに実行済みならスキップ
            if grep -q "^$id," "$EXECUTED_LOG" 2>/dev/null; then
                continue
            fi
        fi

        # scheduled_timeのチェック
        if [ -n "$scheduled_time" ]; then
            local scheduled_epoch=$(date -d "$scheduled_time" +%s 2>/dev/null)
            if [ $? -eq 0 ] && [ $scheduled_epoch -gt $current_time ]; then
                continue  # まだ実行時刻ではない
            fi
        fi

        # dependenciesのチェック
        if [ -n "$dependencies" ]; then
            local dep_met=true
            IFS='|' read -ra DEPS <<< "$dependencies"
            for dep in "${DEPS[@]}"; do
                if ! grep -q "^$dep," "$EXECUTED_LOG" 2>/dev/null; then
                    dep_met=false
                    break
                fi
            done
            if [ "$dep_met" = false ]; then
                continue  # 依存関係が満たされていない
            fi
        fi

        # 優先度でソート（high > medium > low）
        echo "$priority|$id|$working_dir|$prompt|$repeat|$repeat_interval"
    done | sort -r | head -n 1
}

# タスク実行
execute_task() {
    local task_info="$1"
    IFS='|' read -r priority id working_dir prompt repeat repeat_interval <<< "$task_info"

    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "🚀 タスク実行開始"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "ID: $id"
    echo "優先度: $priority"
    echo "作業ディレクトリ: $working_dir"
    echo "プロンプト: $prompt"
    if [ "$repeat" = "yes" ]; then
        echo "繰り返し: 有効 (間隔: $repeat_interval)"
    fi
    echo "開始時刻: $(date '+%Y-%m-%d %H:%M:%S')"
    echo ""

    # Claude Code実行
    local start_time=$(date +%s)
    local exit_code=0

    if [ -x "$QUEUE_DIR/claude_executor.sh" ]; then
        "$QUEUE_DIR/claude_executor.sh" "$working_dir" "$prompt" "$id"
        exit_code=$?
    else
        echo "❌ claude_executor.sh が見つかりません"
        exit_code=1
    fi

    local end_time=$(date +%s)
    local duration=$((end_time - start_time))

    # 実行ログに記録
    if [ "$repeat" = "yes" ]; then
        # 繰り返しタスクは repeat_tasks.log に記録
        echo "$id,$end_time,$duration,$exit_code,$repeat_interval,$prompt" >> "$REPEAT_LOG"
    else
        # 通常タスクは executed.log に記録
        echo "$id,$(date '+%Y-%m-%d %H:%M:%S'),$duration,$exit_code,$prompt" >> "$EXECUTED_LOG"
    fi

    echo ""
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    if [ $exit_code -eq 0 ]; then
        echo "✅ タスク完了 (実行時間: ${duration}秒)"
        if [ "$repeat" = "yes" ]; then
            local interval_seconds=$(interval_to_seconds "$repeat_interval")
            local next_run=$((end_time + interval_seconds))
            local next_run_str=$(date -d "@$next_run" '+%Y-%m-%d %H:%M:%S')
            echo "🔄 次回実行予定: $next_run_str"
        fi
    else
        echo "❌ タスク失敗 (終了コード: $exit_code)"
    fi
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo ""

    return $exit_code
}

# キュー状態表示
show_status() {
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "📊 Claude Code キュー状態"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo ""

    local total=$(tail -n +2 "$CSV_FILE" | wc -l)
    local executed=$(wc -l < "$EXECUTED_LOG")
    local repeat_count=$(tail -n +2 "$CSV_FILE" | cut -d',' -f8 | grep -c "yes")
    local pending=$((total - executed - repeat_count))

    echo "総タスク数: $total"
    echo "  通常タスク: $((total - repeat_count))"
    echo "  繰り返しタスク: $repeat_count"
    echo "実行済み（通常）: $executed"
    echo "残り（通常）: $pending"
    echo ""

    echo "【次の実行候補タスク】"
    local next_task=$(get_next_task)
    if [ -n "$next_task" ]; then
        IFS='|' read -r priority id working_dir prompt repeat repeat_interval <<< "$next_task"
        echo "  ID: $id"
        echo "  優先度: $priority"
        echo "  プロンプト: ${prompt:0:60}..."
        if [ "$repeat" = "yes" ]; then
            echo "  繰り返し: 有効 (間隔: $repeat_interval)"
        fi
    else
        echo "  実行可能なタスクはありません"
    fi
    echo ""

    echo "【繰り返しタスクの状態】"
    if [ -s "$REPEAT_LOG" ]; then
        # 繰り返しタスクの最終実行時刻を表示
        tail -n +2 "$CSV_FILE" | while IFS=',' read -r id priority working_dir prompt status scheduled_time dependencies repeat repeat_interval; do
            if [ "$repeat" = "yes" ]; then
                local last_run=$(grep "^$id," "$REPEAT_LOG" 2>/dev/null | tail -n 1)
                if [ -n "$last_run" ]; then
                    local last_time=$(echo "$last_run" | cut -d',' -f2)
                    local last_time_str=$(date -d "@$last_time" '+%Y-%m-%d %H:%M:%S')
                    local interval_seconds=$(interval_to_seconds "$repeat_interval")
                    local next_run=$((last_time + interval_seconds))
                    local next_run_str=$(date -d "@$next_run" '+%Y-%m-%d %H:%M:%S')
                    local current_time=$(date +%s)

                    if [ $current_time -ge $next_run ]; then
                        echo "  ✅ ID:$id - 実行可能 (最終: $last_time_str)"
                    else
                        echo "  ⏳ ID:$id - 次回: $next_run_str (最終: $last_time_str)"
                    fi
                else
                    echo "  🆕 ID:$id - 未実行 (間隔: $repeat_interval)"
                fi
            fi
        done
    else
        echo "  繰り返しタスクの実行履歴はありません"
    fi
    echo ""

    echo "【最近の実行履歴】"
    if [ -s "$EXECUTED_LOG" ]; then
        tail -n 5 "$EXECUTED_LOG" | while IFS=',' read -r id timestamp duration exit_code prompt; do
            local status_icon="✅"
            [ "$exit_code" != "0" ] && status_icon="❌"
            echo "  $status_icon ID:$id - ${prompt:0:50}... (${duration}秒)"
        done
    else
        echo "  実行履歴はありません"
    fi
    echo ""
}

# メイン処理
main() {
    local command="${1:-run}"

    case "$command" in
        run)
            check_lock
            acquire_lock
            trap release_lock EXIT

            local next_task=$(get_next_task)
            if [ -n "$next_task" ]; then
                execute_task "$next_task"
            else
                echo "ℹ️  実行可能なタスクはありません"
            fi
            ;;

        status)
            show_status
            ;;

        reset)
            echo "🔄 キュー状態をリセット"
            read -p "繰り返しタスクの履歴も削除しますか? [y/N]: " confirm
            if [[ "$confirm" =~ ^[Yy]$ ]]; then
                rm -f "$EXECUTED_LOG" "$REPEAT_LOG" "$LOCK_FILE"
                echo "✅ 全ての履歴をリセットしました"
            else
                rm -f "$EXECUTED_LOG" "$LOCK_FILE"
                echo "✅ 通常タスクの履歴をリセットしました（繰り返しタスクは保持）"
            fi
            touch "$EXECUTED_LOG"
            touch "$REPEAT_LOG"
            ;;

        *)
            echo "使用法: $0 [run|status|reset]"
            echo ""
            echo "  run    - 次のタスクを実行"
            echo "  status - キュー状態を表示"
            echo "  reset  - 実行履歴をリセット"
            exit 1
            ;;
    esac
}

main "$@"
