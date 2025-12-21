#!/bin/bash

# Claude Code 自動実行キュー管理スクリプト v2（使用量制限対応強化版）
# 使用法: ./queue_manager_v2.sh [run|status|add|reset]

QUEUE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CSV_FILE="$QUEUE_DIR/prompts.csv"
STATE_DIR="$QUEUE_DIR/state"
EXECUTED_LOG="$STATE_DIR/executed.log"
REPEAT_LOG="$STATE_DIR/repeat_tasks.log"
QUOTA_LOG="$STATE_DIR/quota_exceeded.log"
RETRY_LOG="$STATE_DIR/retry_tasks.log"
LOCK_FILE="$STATE_DIR/queue.lock"

# 初期化
mkdir -p "$STATE_DIR"
touch "$EXECUTED_LOG"
touch "$REPEAT_LOG"
touch "$QUOTA_LOG"
touch "$RETRY_LOG"

# 終了コード定義
EXIT_SUCCESS=0
EXIT_GENERAL_ERROR=1
EXIT_QUOTA_EXCEEDED=2
EXIT_MAX_RETRIES=3
EXIT_PREPARED=99  # インタラクティブモード: タスク準備完了（実行待ち）

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
        *h) echo $(( ${interval%h} * 3600 )) ;;
        *d|daily)
            if [ "$interval" = "daily" ]; then
                echo 86400
            else
                echo $(( ${interval%d} * 86400 ))
            fi
            ;;
        *w|weekly)
            if [ "$interval" = "weekly" ]; then
                echo 604800
            else
                echo $(( ${interval%w} * 604800 ))
            fi
            ;;
        *m|monthly) echo 2592000 ;;
        *) echo "$interval" ;;
    esac
}

# 繰り返しタスクが実行可能かチェック
can_run_repeat_task() {
    local id="$1"
    local repeat_interval="$2"
    local current_time=$(date +%s)

    local last_run=$(grep "^$id," "$REPEAT_LOG" 2>/dev/null | tail -n 1 | cut -d',' -f2)

    if [ -z "$last_run" ]; then
        return 0
    fi

    local interval_seconds=$(interval_to_seconds "$repeat_interval")
    local next_run_time=$((last_run + interval_seconds))

    if [ $current_time -ge $next_run_time ]; then
        return 0
    else
        return 1
    fi
}

# CSVから次のタスクを取得
get_next_task() {
    local current_time=$(date +%s)

    tail -n +2 "$CSV_FILE" | while IFS=',' read -r id priority working_dir prompt status scheduled_time dependencies repeat repeat_interval retry_count max_retries; do
        # statusがpending以外ならスキップ
        if [ "$status" != "pending" ]; then
            continue
        fi

        # max_retries超過チェック
        if [ -n "$max_retries" ] && [ "$retry_count" -ge "$max_retries" ]; then
            continue  # リトライ上限到達
        fi

        # 繰り返し実行タスクの処理
        if [ "$repeat" = "yes" ]; then
            if ! can_run_repeat_task "$id" "$repeat_interval"; then
                continue
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
                continue
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
                continue
            fi
        fi

        echo "$priority|$id|$working_dir|$prompt|$repeat|$repeat_interval|$retry_count|$max_retries"
    done | sort -r | head -n 1
}

# CSVのretry_countを更新
update_retry_count() {
    local task_id="$1"
    local new_count="$2"

    # 一時ファイルに更新
    awk -v id="$task_id" -v count="$new_count" 'BEGIN{FS=OFS=","}
        NR==1 {print; next}
        $1==id {$10=count}
        {print}' "$CSV_FILE" > "$CSV_FILE.tmp"

    mv "$CSV_FILE.tmp" "$CSV_FILE"
}

# タスク実行
execute_task() {
    local task_info="$1"
    IFS='|' read -r priority id working_dir prompt repeat repeat_interval retry_count max_retries <<< "$task_info"

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
    if [ "$retry_count" -gt 0 ]; then
        echo "リトライ: ${retry_count}回目 (最大: $max_retries回)"
    fi
    echo "開始時刻: $(date '+%Y-%m-%d %H:%M:%S')"
    echo ""

    # Claude API実行（完全自動）
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

    # 終了コードに応じた処理
    case $exit_code in
        $EXIT_SUCCESS)
            # 正常終了
            if [ "$repeat" = "yes" ]; then
                echo "$id,$end_time,$duration,$exit_code,$repeat_interval,$prompt" >> "$REPEAT_LOG"
            else
                echo "$id,$(date '+%Y-%m-%d %H:%M:%S'),$duration,$exit_code,$prompt" >> "$EXECUTED_LOG"
            fi

            # リトライカウントをリセット
            update_retry_count "$id" 0

            echo ""
            echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
            echo "✅ タスク完了 (実行時間: ${duration}秒)"
            if [ "$repeat" = "yes" ]; then
                local interval_seconds=$(interval_to_seconds "$repeat_interval")
                local next_run=$((end_time + interval_seconds))
                local next_run_str=$(date -d "@$next_run" '+%Y-%m-%d %H:%M:%S')
                echo "🔄 次回実行予定: $next_run_str"
            fi
            ;;

        $EXIT_QUOTA_EXCEEDED)
            # 使用量制限到達
            echo "$id,$(date '+%Y-%m-%d %H:%M:%S'),$duration,quota_exceeded,$prompt" >> "$QUOTA_LOG"

            echo ""
            echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
            echo "⏸️  使用量制限到達 - タスクを保留"
            echo "次回の自動実行時に再試行されます"
            echo "現在の使用状況を確認してください"
            ;;

        *)
            # エラー終了
            local new_retry_count=$((retry_count + 1))
            update_retry_count "$id" "$new_retry_count"

            echo "$id,$(date '+%Y-%m-%d %H:%M:%S'),$duration,$exit_code,$new_retry_count,$max_retries,$prompt" >> "$RETRY_LOG"

            echo ""
            echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
            echo "❌ タスク失敗 (終了コード: $exit_code)"

            if [ "$new_retry_count" -lt "$max_retries" ]; then
                echo "🔄 リトライ: ${new_retry_count}/${max_retries}回"
                echo "次回の自動実行時に再試行されます"
            else
                echo "⛔ リトライ上限到達 (${max_retries}回)"
                echo "タスクをスキップします"
            fi
            ;;
    esac
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
    local quota_count=$(wc -l < "$QUOTA_LOG")
    local retry_count=$(wc -l < "$RETRY_LOG")

    echo "総タスク数: $total"
    echo "  通常タスク: $((total - repeat_count))"
    echo "  繰り返しタスク: $repeat_count"
    echo "実行済み（通常）: $executed"
    if [ $quota_count -gt 0 ]; then
        echo "使用量制限による保留: $quota_count"
    fi
    if [ $retry_count -gt 0 ]; then
        echo "リトライ中: $retry_count"
    fi
    echo ""

    echo "【次の実行候補タスク】"
    local next_task=$(get_next_task)
    if [ -n "$next_task" ]; then
        IFS='|' read -r priority id working_dir prompt repeat repeat_interval retry_count max_retries <<< "$next_task"
        echo "  ID: $id"
        echo "  優先度: $priority"
        echo "  プロンプト: ${prompt:0:60}..."
        if [ "$repeat" = "yes" ]; then
            echo "  繰り返し: 有効 (間隔: $repeat_interval)"
        fi
        if [ "$retry_count" -gt 0 ]; then
            echo "  リトライ: ${retry_count}/${max_retries}回目"
        fi
    else
        echo "  実行可能なタスクはありません"
    fi
    echo ""

    if [ $quota_count -gt 0 ]; then
        echo "【使用量制限により保留中のタスク】"
        tail -n 5 "$QUOTA_LOG" | while IFS=',' read -r id timestamp duration status prompt; do
            echo "  ⏸️  ID:$id - ${prompt:0:50}... ($timestamp)"
        done
        echo ""
    fi

    echo "【繰り返しタスクの状態】"
    tail -n +2 "$CSV_FILE" | while IFS=',' read -r id priority working_dir prompt status scheduled_time dependencies repeat repeat_interval retry_count max_retries; do
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
            echo "1) 通常タスクの履歴のみ"
            echo "2) 繰り返しタスクの履歴のみ"
            echo "3) 全ての履歴（quota, retry含む）"
            echo "4) キャンセル"
            read -p "選択してください [1-4]: " choice

            case $choice in
                1)
                    rm -f "$EXECUTED_LOG" "$LOCK_FILE"
                    echo "✅ 通常タスクの履歴をリセットしました"
                    ;;
                2)
                    rm -f "$REPEAT_LOG"
                    echo "✅ 繰り返しタスクの履歴をリセットしました"
                    ;;
                3)
                    rm -f "$EXECUTED_LOG" "$REPEAT_LOG" "$QUOTA_LOG" "$RETRY_LOG" "$LOCK_FILE"
                    # CSVのretry_countもリセット
                    awk 'BEGIN{FS=OFS=","} NR==1 {print; next} {$10=0; print}' "$CSV_FILE" > "$CSV_FILE.tmp"
                    mv "$CSV_FILE.tmp" "$CSV_FILE"
                    echo "✅ 全ての履歴をリセットしました"
                    ;;
                4)
                    echo "❌ キャンセルしました"
                    exit 0
                    ;;
                *)
                    echo "❌ 無効な選択です"
                    exit 1
                    ;;
            esac

            touch "$EXECUTED_LOG" "$REPEAT_LOG" "$QUOTA_LOG" "$RETRY_LOG"
            ;;

        complete)
            local task_id="$2"
            if [ -z "$task_id" ]; then
                echo "❌ エラー: タスクIDを指定してください"
                echo "使用法: $0 complete <タスクID>"
                exit 1
            fi

            # タスク情報を取得
            local task_info=$(grep "^$task_id," "$CSV_FILE")
            if [ -z "$task_info" ]; then
                echo "❌ エラー: タスクID $task_id が見つかりません"
                exit 1
            fi

            IFS=',' read -r id priority working_dir prompt status scheduled_time dependencies repeat repeat_interval retry_count max_retries <<< "$task_info"

            # 完了として記録
            local current_time=$(date +%s)
            if [ "$repeat" = "yes" ]; then
                echo "$id,$current_time,0,0,$repeat_interval,$prompt" >> "$REPEAT_LOG"
                echo "✅ 繰り返しタスク ID:$id を完了としてマークしました"
                local interval_seconds=$(interval_to_seconds "$repeat_interval")
                local next_run=$((current_time + interval_seconds))
                local next_run_str=$(date -d "@$next_run" '+%Y-%m-%d %H:%M:%S')
                echo "🔄 次回実行予定: $next_run_str"
            else
                echo "$id,$(date '+%Y-%m-%d %H:%M:%S'),0,0,$prompt" >> "$EXECUTED_LOG"
                echo "✅ タスク ID:$id を完了としてマークしました"
            fi

            # リトライカウントをリセット
            update_retry_count "$id" 0
            ;;

        *)
            echo "使用法: $0 [run|status|reset|complete]"
            echo ""
            echo "  run              - 次のタスクを準備"
            echo "  status           - キュー状態を表示"
            echo "  reset            - 実行履歴をリセット"
            echo "  complete <ID>    - タスクを完了としてマーク"
            exit 1
            ;;
    esac
}

main "$@"
