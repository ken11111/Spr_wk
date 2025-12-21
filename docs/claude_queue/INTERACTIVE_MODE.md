# インタラクティブ実行モード ガイド

## 📋 概要

このキューシステムは**インタラクティブ実行モード**を採用しています。

### なぜインタラクティブモードか？

Claude CLIは対話型のツールであり、スクリプトから直接実行することができません。そのため、次のような仕組みを採用しています:

1. ✅ キュー管理システムが次のタスクを `NEXT_TASK.md` に出力
2. ✅ ユーザーがClaude CLIでそのタスクを実行
3. ✅ 完了後、再度キューを実行して次のタスクへ

## 🔄 実行フロー

### ステップ1: 次のタスクを取得

```bash
cd /home/ken/Spr_ws/claude_queue
./queue_manager_v2.sh run
```

**出力例**:
```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
📋 次のタスクを準備しました
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

【タスクID】: 5
【作業ディレクトリ】: /home/ken/Spr_ws

【プロンプト】:
PATENT_ANALYSIS.mdを読んで弁理士の視点で評価、こちらで検討が必要な案件を5つまで出してください

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

📄 詳細は NEXT_TASK.md を確認してください

💡 Claude CLIで以下を実行:
   「NEXT_TASK.mdを読んで、記載されているタスクを実行してください」

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

### ステップ2: NEXT_TASK.mdを確認

```bash
cat NEXT_TASK.md
```

または、好きなエディタで開いて内容を確認します。

### ステップ3: Claude CLIで実行

**Claude CLIセッション内で**:

```
NEXT_TASK.mdを読んで、記載されているタスクを実行してください
```

Claude が自動的に:
1. `NEXT_TASK.md` を読み込む
2. 作業ディレクトリに移動（または指定されたパスのファイルを操作）
3. プロンプトに従ってタスクを実行

### ステップ4: 次のタスクへ

タスク完了後、**同じClaude CLIセッション内で**:

```bash
cd /home/ken/Spr_ws/claude_queue
./queue_manager_v2.sh run
```

これで次のタスクが `NEXT_TASK.md` に出力され、ステップ3に戻ります。

## 🎯 実用例

### 例1: 1つのタスクだけ実行

```bash
# 次のタスクを取得
./queue_manager_v2.sh run

# Claude CLIで実行
# → NEXT_TASK.mdを読んで実行

# 完了！
```

### 例2: 複数のタスクを連続実行

Claude CLIセッション内で:

```
# 1つ目のタスク
NEXT_TASK.mdを読んで、記載されているタスクを実行してください

# 完了したら次へ
cd /home/ken/Spr_ws/claude_queue
./queue_manager_v2.sh run

# 2つ目のタスク
NEXT_TASK.mdを読んで、記載されているタスクを実行してください

# 完了したら次へ
cd /home/ken/Spr_ws/claude_queue
./queue_manager_v2.sh run

# ... 繰り返し
```

### 例3: Cronと組み合わせた半自動化

**Cronで30分ごとにNEXT_TASK.mdを更新**:

```cron
*/30 * * * * /home/ken/Spr_ws/claude_queue/queue_manager_v2.sh run >> /home/ken/Spr_ws/claude_queue/cron.log 2>&1
```

**ユーザーの作業**:
- 定期的に `NEXT_TASK.md` をチェック
- 新しいタスクがあればClaude CLIで実行
- 完了後、自動的に次のタスクがNEXT_TASK.mdに反映される

## 📊 キュー状態の確認

いつでも現在のキュー状態を確認できます:

```bash
./queue_manager_v2.sh status
```

**出力例**:
```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
📊 Claude Code キュー状態
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

総タスク数: 9
  通常タスク: 6
  繰り返しタスク: 3
実行済み（通常）: 2

【次の実行候補タスク】
  ID: 5
  優先度: medium
  プロンプト: PATENT_ANALYSIS.mdを読んで弁理士の視点で評価、こちらで検討...

【繰り返しタスクの状態】
  🆕 ID:1 - 未実行 (間隔: weekly)
  🆕 ID:10 - 未実行 (間隔: daily)
  🆕 ID:11 - 未実行 (間隔: weekly)
```

## ⚙️ 高度な使い方

### scheduled_time を使った遅延実行

使用量制限時、システムは自動的に `scheduled_time` を設定します:

```csv
id,priority,...,scheduled_time,...
10,high,...,2025-12-14 14:30,...  ← 4.5時間後に設定
```

次回 `./queue_manager_v2.sh run` 実行時、scheduled_timeをチェックして:
- 時刻前 → スキップ
- 時刻後 → 実行

### 手動でのタスク追加

`prompts.csv` を直接編集してタスクを追加:

```csv
id,priority,working_dir,prompt,status,scheduled_time,dependencies,repeat,repeat_interval,retry_count,max_retries
100,high,/home/ken/Spr_ws,"新しいタスク",pending,,,no,,0,3
```

## 🛡️ 使用量制限の扱い

### 制限到達時の動作

Claude CLIの使用量制限に達した場合:

1. ❌ タスクは実行できない
2. ⏸️ `quota_exceeded.log` に記録
3. ⏰ 次回リトライは4.5時間後に設定
4. ✅ 自動的に再試行される

### 手動での確認

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
  （このウィンドウが終了するまで）
```

## 📝 ベストプラクティス

### 1. 優先度の活用

重要なタスクは `high` に設定:

```csv
1,high,/home/ken/Spr_ws/case_study,"進捗更新",pending,,,yes,weekly,0,3
```

### 2. 依存関係の設定

段階的な処理:

```csv
30,high,/home/ken/Spr_ws,"データ収集",pending,,,no,,0,3
31,high,/home/ken/Spr_ws,"データ分析",pending,,30,no,,0,3
32,medium,/home/ken/Spr_ws,"レポート作成",pending,,31,no,,0,3
```

### 3. 繰り返しタスクの管理

定期的な調査タスク:

```csv
10,high,/home/ken/Spr_ws,"AI技術動向調査",pending,,,yes,daily,0,5
11,medium,/home/ken/Spr_ws,"組み込み技術リサーチ",pending,,,yes,weekly,0,5
```

### 4. Cronとの連携

```cron
# 30分ごとにNEXT_TASK.mdを更新
*/30 * * * * /home/ken/Spr_ws/claude_queue/queue_manager_v2.sh run

# 使用量を定期チェック（ログ出力）
0 * * * * /home/ken/Spr_ws/claude_queue/check_usage_v2.sh >> /home/ken/usage.log
```

## 🔧 トラブルシューティング

### Q: NEXT_TASK.mdが更新されない

**確認**:
```bash
# 実行可能なタスクがあるか確認
./queue_manager_v2.sh status

# ロックファイルがあるか確認
ls -la state/queue.lock

# あれば削除
rm -f state/queue.lock
```

### Q: タスクがスキップされる

**原因**:
1. `scheduled_time` が未来の時刻
2. `dependencies` が未完了
3. `retry_count >= max_retries`

**対処**:
```bash
# CSVを確認
cat prompts.csv | grep "^タスクID,"

# 必要に応じて scheduled_time をクリア
# または dependencies を削除
```

### Q: 使用量制限が続く

```bash
# 現在の使用状況を確認
./check_usage_v2.sh

# 5時間ウィンドウのリセットを待つ
# または、max_retriesを増やす
```

## 📚 関連ドキュメント

- **SPECIFICATION.md** - システム仕様（簡潔版）
- **QUOTA_HANDLING.md** - 使用量制限の詳細
- **README.md** - 完全なドキュメント（40KB+）
- **prompts.csv** - タスクキュー（編集用）

---

**作成日**: 2025-12-14
**対象**: Claude Code自動実行キューシステム v2
**目的**: インタラクティブ実行モードの使い方ガイド
