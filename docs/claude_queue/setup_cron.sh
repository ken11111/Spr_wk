#!/bin/bash

# Cronスケジューラー設定スクリプト

QUEUE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CRON_FILE="/tmp/claude_queue_cron"

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "⏰ Claude Code キュースケジューラー設定"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Cron設定例を表示
cat <<'EOF' > "$CRON_FILE"
# Claude Code自動実行キュー設定
#
# 形式: 分 時 日 月 曜日 コマンド
# 例: 30分ごとに実行 - */30 * * * *
# 例: 毎時0分に実行 - 0 * * * *
# 例: 平日9-18時の毎時0分 - 0 9-18 * * 1-5

# 30分ごとにキューをチェックして実行
EOF

echo "*/30 * * * * $QUEUE_DIR/queue_manager.sh run >> $QUEUE_DIR/state/cron.log 2>&1" >> "$CRON_FILE"

echo "【設定内容】"
cat "$CRON_FILE"
echo ""

# Cron設定の選択肢を表示
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "スケジュール設定オプション:"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "1) 30分ごと (デフォルト)"
echo "2) 1時間ごと"
echo "3) 平日9-18時の1時間ごと"
echo "4) 15分ごと"
echo "5) カスタム設定"
echo "6) インストールせずに終了"
echo ""

read -p "選択してください [1-6]: " choice

case $choice in
    1)
        CRON_SCHEDULE="*/30 * * * *"
        CRON_DESC="30分ごと"
        ;;
    2)
        CRON_SCHEDULE="0 * * * *"
        CRON_DESC="1時間ごと"
        ;;
    3)
        CRON_SCHEDULE="0 9-18 * * 1-5"
        CRON_DESC="平日9-18時の1時間ごと"
        ;;
    4)
        CRON_SCHEDULE="*/15 * * * *"
        CRON_DESC="15分ごと"
        ;;
    5)
        echo ""
        echo "Cron形式で入力してください (例: */30 * * * *)"
        read -p "スケジュール: " CRON_SCHEDULE
        CRON_DESC="カスタム: $CRON_SCHEDULE"
        ;;
    6)
        echo "❌ インストールをキャンセルしました"
        rm -f "$CRON_FILE"
        exit 0
        ;;
    *)
        echo "❌ 無効な選択です"
        rm -f "$CRON_FILE"
        exit 1
        ;;
esac

# Cron設定ファイル作成
cat <<EOF > "$CRON_FILE"
# Claude Code自動実行キュー
# スケジュール: $CRON_DESC
$CRON_SCHEDULE $QUEUE_DIR/queue_manager.sh run >> $QUEUE_DIR/state/cron.log 2>&1
EOF

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📝 最終確認"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
cat "$CRON_FILE"
echo ""

read -p "この設定でCronに追加しますか? [y/N]: " confirm

if [[ "$confirm" =~ ^[Yy]$ ]]; then
    # 既存のcron設定を取得
    crontab -l > /tmp/current_cron 2>/dev/null || true

    # Claude Queueの既存設定を削除
    grep -v "claude_queue" /tmp/current_cron > /tmp/new_cron 2>/dev/null || true

    # 新しい設定を追加
    cat "$CRON_FILE" >> /tmp/new_cron

    # Cronに設定
    crontab /tmp/new_cron

    echo "✅ Cron設定が完了しました"
    echo ""
    echo "【確認】"
    echo "現在のCron設定:"
    crontab -l | grep claude_queue
    echo ""
    echo "【ログファイル】"
    echo "$QUEUE_DIR/state/cron.log"

    # クリーンアップ
    rm -f /tmp/current_cron /tmp/new_cron "$CRON_FILE"
else
    echo "❌ インストールをキャンセルしました"
    rm -f "$CRON_FILE"
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "【Cron設定の削除方法】"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "crontab -e"
echo "# claude_queue の行を削除して保存"
echo ""
