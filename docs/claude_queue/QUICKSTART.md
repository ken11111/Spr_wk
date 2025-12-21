# クイックスタートガイド

## 🚀 5分で始める Claude キュー実行

### 前提条件

- ✅ Claude CLI (`claude`) がインストール済み
- ✅ `/home/ken/Spr_ws/claude_queue` ディレクトリに移動済み

### ステップ1: タスクを追加

`prompts.csv` を編集:

```csv
id,priority,working_dir,prompt,status,scheduled_time,dependencies,repeat,repeat_interval,retry_count,max_retries
100,high,/home/ken/Spr_ws,"試しのタスク: README.mdを読んでサマリーを作成",pending,,,no,,0,3
```

### ステップ2: 次のタスクを取得

```bash
./queue_manager_v2.sh run
```

**出力**:
```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
📋 次のタスクを準備しました
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

【タスクID】: 100
【作業ディレクトリ】: /home/ken/Spr_ws

【プロンプト】:
試しのタスク: README.mdを読んでサマリーを作成

💡 Claude CLIで以下を実行:
   「NEXT_TASK.mdを読んで、記載されているタスクを実行してください」
```

### ステップ3: Claude CLI で実行

**Claude CLI セッション内で**:

```
NEXT_TASK.mdを読んで、記載されているタスクを実行してください
```

Claude が自動的に:
1. `NEXT_TASK.md` を読み込む
2. 作業ディレクトリのREADME.mdを読む
3. サマリーを作成

### ステップ4: 次のタスクへ（同じClaude CLIセッション内）

```bash
cd /home/ken/Spr_ws/claude_queue
./queue_manager_v2.sh run
```

→ 次のタスクが `NEXT_TASK.md` に出力される

→ ステップ3に戻る

---

## 📊 現在のキュー状態を確認

```bash
./queue_manager_v2.sh status
```

**出力例**:
```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
📊 Claude Code キュー状態
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

総タスク数: 8
  通常タスク: 5
  繰り返しタスク: 3
実行済み（通常）: 2

【次の実行候補タスク】
  ID: 3
  優先度: low
  プロンプト: "case_studyディレクトリのファイル一覧を表示してください"...

【繰り返しタスクの状態】
  🆕 ID:1 - 未実行 (間隔: weekly)
  🆕 ID:10 - 未実行 (間隔: daily)
  ⏳ ID:11 - 次回: 2025-12-21 18:46:35 (最終: 2025-12-14 18:46:35)
```

---

## 🎯 よくある使い方

### 1. 通常のタスク（1回のみ実行）

```csv
1,high,/home/ken/Spr_ws/spresense,"SDKバージョンを確認",pending,,,no,,0,3
```

### 2. 繰り返しタスク（毎日実行）

```csv
10,high,/home/ken/Spr_ws,"AI技術動向調査",pending,,,yes,daily,0,5
```

### 3. スケジュール実行（指定時刻に実行）

```csv
20,medium,/home/ken/Spr_ws,"朝のレポート",pending,2025-12-15 09:00,,no,,0,3
```

### 4. 依存関係のあるタスク（順次実行）

```csv
30,high,/home/ken/Spr_ws,"データ収集",pending,,,no,,0,3
31,high,/home/ken/Spr_ws,"データ分析",pending,,30,no,,0,3
32,medium,/home/ken/Spr_ws,"レポート作成",pending,,31,no,,0,3
```

→ ID:30 → ID:31 → ID:32 の順に実行される

---

## ⏱️ Cronとの連携（半自動化）

**Cronで30分ごとにNEXT_TASK.mdを更新**:

```bash
# Cron設定
crontab -e

# 以下を追加
*/30 * * * * /home/ken/Spr_ws/claude_queue/queue_manager_v2.sh run >> /home/ken/Spr_ws/claude_queue/cron.log 2>&1
```

**動作**:
- 30分ごとに `NEXT_TASK.md` が自動更新される
- ユーザーは好きなタイミングでClaude CLIで実行
- 完了後、次のタスクが自動的に準備される

---

## 🔄 使用量制限の扱い

Claude CLIの使用量は**5時間ごとにリセット**されます。

### 制限到達時の動作

1. ⏸️ タスクは保留状態になる
2. 📝 `quota_exceeded.log` に記録される
3. ⏰ 4.5時間後に自動リトライが設定される
4. ✅ 使用量回復後、自動的に実行される

### 使用量の確認

```bash
./check_usage_v2.sh
```

**出力例**:
```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
📊 Claude Code使用量チェック（5時間リセット対応）
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

【現在の5時間ウィンドウ】
  開始: 2025-12-14 10:00:00
  終了: 2025-12-14 15:00:00
  実行回数: 25 / 50

✅ 使用量OK - 実行可能です

【残り実行可能回数】
  25回
```

---

## 📚 詳細ドキュメント

- **INTERACTIVE_MODE.md** - インタラクティブ実行の詳細ガイド
- **SPECIFICATION.md** - システム仕様（簡潔版）
- **QUOTA_HANDLING.md** - 使用量制限の詳細
- **README.md** - 完全なドキュメント（40KB+）

---

## 🛠️ トラブルシューティング

### Q: タスクが実行されない

**確認**:
```bash
# ステータス確認
./queue_manager_v2.sh status

# CSVファイル確認
cat prompts.csv
```

**原因と対処**:
- `status` が `pending` でない → CSVを編集
- `scheduled_time` が未来の時刻 → 時刻を過去に変更またはクリア
- `dependencies` が未完了 → 依存タスクを先に実行
- `retry_count >= max_retries` → CSVの `retry_count` を0にリセット

### Q: NEXT_TASK.mdが更新されない

**確認**:
```bash
# ロックファイルを確認
ls -la state/queue.lock

# あれば削除
rm -f state/queue.lock
```

### Q: 全てのログをリセットしたい

```bash
./queue_manager_v2.sh reset

# → オプション3を選択（全ての履歴をリセット）
```

---

## ✅ まとめ

1. **`prompts.csv`** でタスクを管理
2. **`./queue_manager_v2.sh run`** で次のタスクを取得
3. **Claude CLI** で `NEXT_TASK.md` を実行
4. 完了後、手順2に戻る

**これだけ！**

---

**作成日**: 2025-12-14
**対象**: Claude Code自動実行キューシステム v2
**目的**: 5分で始められる簡潔なガイド
