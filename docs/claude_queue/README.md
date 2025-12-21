# Claude Code 自動実行キューシステム

## 概要

CSVファイルでプロンプトをキュー管理し、指定時刻や間隔で自動的にClaude Codeを実行するシステムです。

**主要機能**:
- ✅ CSV形式のプロンプトキュー管理
- ✅ 優先度制御（high/medium/low）
- ✅ スケジュール実行（指定時刻）
- ✅ 依存関係管理（タスクの実行順序制御）
- ✅ 使用量制限チェック
- ✅ 実行履歴とログ管理
- ✅ 多重実行防止（ロック機構）

## ディレクトリ構成

```
claude_queue/
├── prompts.csv              - プロンプトキュー（編集してタスク追加）
├── queue_manager.sh         - キュー管理スクリプト ⭐
├── claude_executor.sh       - Claude Code実行スクリプト
├── check_usage.sh           - 使用量チェック
├── setup_cron.sh            - Cronスケジューラー設定
├── state/
│   ├── executed.log         - 実行済みログ
│   ├── execution.log        - 実行詳細ログ
│   ├── usage.log            - 使用量ログ
│   ├── cron.log             - Cron実行ログ
│   └── outputs/             - 実行結果の出力
└── README.md                - このファイル
```

## クイックスタート

### 1. スクリプトに実行権限を付与

```bash
cd ~/Spr_ws/GH_wk_test/docs/claude_queue
chmod +x *.sh
```

### 2. プロンプトキューの編集

`prompts.csv`を編集してタスクを追加:

```csv
id,priority,working_dir,prompt,status,scheduled_time,dependencies
1,high,/home/ken/Spr_ws/spresense,"SDKバージョンを確認してください",pending,,
2,medium,/home/ken/Spr_ws,"ファイル一覧を表示してください",pending,2025-12-15 09:00,1
```

**CSVフォーマット**:
- `id`: 一意のタスクID（数値）
- `priority`: 優先度（high/medium/low）
- `working_dir`: 作業ディレクトリの絶対パス
- `prompt`: Claude Codeに送るプロンプト
- `status`: タスク状態（pending/completed/failed）
- `scheduled_time`: 実行開始時刻（省略可、形式: YYYY-MM-DD HH:MM）
- `dependencies`: 依存タスクID（省略可、複数の場合は|区切り）
- `repeat`: 繰り返し実行フラグ（yes/no）
- `repeat_interval`: 繰り返し間隔（例: 12h, daily, weekly）

### 3. 手動実行テスト

```bash
# キュー状態を確認
./queue_manager.sh status

# 次のタスクを1件実行
./queue_manager.sh run

# 実行履歴をリセット（テスト用）
./queue_manager.sh reset
```

### 4. 自動実行スケジュールの設定

```bash
# Cronスケジューラーを設定
./setup_cron.sh

# 設定例:
# 1) 30分ごと
# 2) 1時間ごと
# 3) 平日9-18時の1時間ごと
# 4) 15分ごと
```

## 使用方法

### タスクの追加

`prompts.csv`に新しい行を追加:

```csv
6,high,/home/ken/Spr_ws/spresense,"ビルドシステムを診断してください",pending,,
```

### 優先度の設定

```csv
id,priority,...
1,high,...      # 最優先で実行
2,medium,...    # 中優先度
3,low,...       # 低優先度
```

同じ優先度の場合、CSV上で上にあるタスクが先に実行されます。

### スケジュール実行

特定の時刻以降に実行したい場合:

```csv
id,...,scheduled_time,...
5,...,2025-12-15 14:30,...   # 2025-12-15 14:30以降に実行
```

### 依存関係の設定

あるタスクが完了してから実行したい場合:

```csv
id,...,dependencies,repeat,repeat_interval
1,...,,no,              # 依存なし
2,...,1,no,             # タスク1が完了後に実行
3,...,1|2,no,           # タスク1と2が両方完了後に実行
```

### 繰り返し実行の設定 ⭐NEW

定期的に実行したいタスク（技術動向調査など）:

```csv
id,priority,working_dir,prompt,status,scheduled_time,dependencies,repeat,repeat_interval
10,high,/home/ken/Spr_ws,"AI技術の最新動向を調査",pending,,,yes,daily
11,medium,/home/ken/Spr_ws,"組み込み開発の最新技術をリサーチ",pending,,,yes,weekly
12,low,/home/ken/Spr_ws,"ビルドログをチェック",pending,,,yes,12h
13,high,/home/ken/Spr_ws,"セキュリティアップデート確認",pending,,,yes,3d
```

**repeat_interval の指定方法**:
- `12h` - 12時間ごと
- `daily` または `1d` - 毎日
- `weekly` または `1w` - 毎週
- `monthly` - 毎月（30日ごと）
- `3d` - 3日ごと
- `2w` - 2週間ごと

**繰り返しタスクの特徴**:
- ✅ 指定間隔ごとに自動的に再実行
- ✅ 実行完了後、次回実行予定時刻を表示
- ✅ 通常タスクとは別の`repeat_tasks.log`で管理
- ✅ 優先度制御で重要な繰り返しタスクを優先実行可能

**使用例**:
```csv
# 毎日朝9時にAI技術動向を調査
20,high,/home/ken/Spr_ws,"AI技術の最新動向をGitHub Trending、Hacker News、arXivから調査してまとめる",pending,2025-12-15 09:00,,yes,daily

# 週1回、月曜朝にセキュリティ情報を収集
21,high,/home/ken/Spr_ws,"最新のセキュリティ脆弱性情報を収集し、影響を分析",pending,,,yes,weekly

# 12時間ごとにビルド状態を監視
22,medium,/home/ken/Spr_ws/spresense,"ビルドログをチェックしてエラーや警告を報告",pending,,,yes,12h
```

### キュー状態の確認

```bash
./queue_manager.sh status
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
残り（通常）: 3

【次の実行候補タスク】
  ID: 10
  優先度: high
  プロンプト: AI技術の最新動向を調査してください。主要なニュースサイト...
  繰り返し: 有効 (間隔: daily)

【繰り返しタスクの状態】
  ✅ ID:10 - 実行可能 (最終: 2025-12-13 09:00:00)
  ⏳ ID:11 - 次回: 2025-12-20 09:00:00 (最終: 2025-12-13 09:00:00)
  🆕 ID:12 - 未実行 (間隔: 12h)

【最近の実行履歴】
  ✅ ID:1 - SDKバージョンを確認してください (45秒)
  ✅ ID:2 - ファイル一覧を表示してください (12秒)
```

### 手動実行

```bash
# 次のタスクを1件実行
./queue_manager.sh run
```

### 実行履歴のリセット

```bash
# 全てのタスクを未実行状態に戻す
./queue_manager.sh reset
```

## 使用量制限

`check_usage.sh`で使用量を制限できます（デフォルト設定）:

```bash
MAX_REQUESTS_PER_DAY=100    # 1日の最大実行回数
MAX_REQUESTS_PER_HOUR=20    # 1時間の最大実行回数
```

制限に達すると、自動的に実行をスキップします。

## ログファイル

### executed.log
実行済みタスクの記録:
```
1,2025-12-14 10:30:45,45,0,SDKバージョンを確認してください
```
形式: `ID,実行日時,実行時間(秒),終了コード,プロンプト`

### execution.log
実行詳細ログ（全実行の詳細）

### usage.log
使用量ログ（時間別の実行回数）

### cron.log
Cronからの自動実行ログ

### outputs/
各タスクの実行結果を個別ファイルで保存

## スケジューラー設定

### Cron設定の確認

```bash
crontab -l | grep claude_queue
```

### Cron設定の編集

```bash
crontab -e
```

### Cron設定の削除

```bash
crontab -e
# claude_queue の行を削除して保存
```

## 高度な使用例

### 例1: 定期的な環境診断

```csv
id,priority,working_dir,prompt,status,scheduled_time,dependencies
10,high,/home/ken/Spr_ws/spresense,"環境診断スクリプトを実行してください",pending,,
11,medium,/home/ken/Spr_ws/spresense,"ビルド状態を確認してください",pending,,10
12,low,/home/ken/Spr_ws,"診断結果をまとめてREADME.mdを更新してください",pending,,11
```

### 例2: 時間指定での段階的ビルド

```csv
id,priority,working_dir,prompt,status,scheduled_time,dependencies
20,high,/home/ken/Spr_ws/spresense,"設定ファイルを確認してください",pending,2025-12-15 09:00,
21,high,/home/ken/Spr_ws/spresense,"ビルドを実行してください",pending,2025-12-15 09:30,20
22,high,/home/ken/Spr_ws/spresense,"ビルド結果を報告してください",pending,2025-12-15 10:00,21
```

### 例3: コードレビュー自動化

```csv
id,priority,working_dir,prompt,status,scheduled_time,dependencies,repeat,repeat_interval
30,medium,/home/ken/Spr_ws/project,"src/ディレクトリのコードをレビューしてください",pending,,,no,
31,medium,/home/ken/Spr_ws/project,"テストカバレッジを確認してください",pending,,,no,
32,low,/home/ken/Spr_ws/project,"改善提案をまとめてください",pending,,30|31,no,
```

### 例4: 繰り返しタスクの活用 ⭐推奨

**技術動向の自動調査**:
```csv
id,priority,working_dir,prompt,status,scheduled_time,dependencies,repeat,repeat_interval
40,high,/home/ken/Spr_ws,"AI/機械学習の最新論文をarXiv、Google Scholar、Papers with Codeから調査し、重要なものを3-5件ピックアップして要約してください",pending,,,yes,daily
41,high,/home/ken/Spr_ws,"GitHub Trendingから注目のAI/組み込み関連リポジトリをチェックし、有用なものを報告してください",pending,,,yes,daily
42,medium,/home/ken/Spr_ws,"Hacker NewsのトップストーリーからAI技術関連のニュースを抽出し、まとめてください",pending,,,yes,12h
```

**定期的なシステム監視**:
```csv
id,priority,working_dir,prompt,status,scheduled_time,dependencies,repeat,repeat_interval
50,high,/home/ken/Spr_ws/spresense,"ビルドログを確認し、新しいwarningやerrorがあれば報告してください",pending,,,yes,12h
51,medium,/home/ken/Spr_ws,"ディスク使用量をチェックし、80%を超えていたら警告してください",pending,,,yes,daily
52,low,/home/ken/Spr_ws,"TODO.mdファイルを読んで、期限が近いタスクをリマインドしてください",pending,,,yes,daily
```

**セキュリティ情報収集**:
```csv
id,priority,working_dir,prompt,status,scheduled_time,dependencies,repeat,repeat_interval
60,high,/home/ken/Spr_ws,"最新のCVE情報をチェックし、使用中のライブラリに関連する脆弱性があれば報告してください",pending,,,yes,daily
61,medium,/home/ken/Spr_ws,"NuttX、Spresenseのセキュリティアップデート情報を確認してください",pending,,,yes,weekly
```

**繰り返しタスクの利点**:
- ✅ **情報収集の自動化**: 毎日の技術動向チェックが自動化
- ✅ **見逃し防止**: 定期的な監視でシステム異常を早期発見
- ✅ **作業の平準化**: 定型作業をClaude Codeに任せて開発に集中
- ✅ **トレンド把握**: 継続的な情報収集で技術トレンドを見逃さない

## トラブルシューティング

### Q1: タスクが実行されない

**確認事項**:
```bash
# 1. キュー状態を確認
./queue_manager.sh status

# 2. 依存関係をチェック
cat prompts.csv | grep -E "^(id|タスクID)"

# 3. scheduled_timeをチェック
# 現在時刻より未来になっていないか確認

# 4. Cronログを確認
tail -f state/cron.log
```

### Q2: 多重実行されている

ロックファイルを確認:
```bash
ls -l state/queue.lock

# 古いロックを削除
rm -f state/queue.lock
```

### Q3: Claude Code実行エラー

`claude_executor.sh`の実行部分を環境に合わせて調整:
```bash
# ファイルを編集
nano claude_executor.sh

# 実際のClaude Code CLI呼び出しを実装
```

## カスタマイズ

### claude_executor.sh の実装

現在は暫定実装（プロンプトを記録のみ）です。
実際のClaude Code実行方法は環境に応じて実装してください:

```bash
# 例: Claude Code CLIが使える場合
echo "$PROMPT" | claude code --working-dir "$WORKING_DIR"

# 例: API経由の場合
curl -X POST https://api.anthropic.com/v1/messages \
  -H "anthropic-api-key: $API_KEY" \
  -d "{\"prompt\": \"$PROMPT\", ...}"
```

### 使用量制限のカスタマイズ

`check_usage.sh`を編集:
```bash
MAX_REQUESTS_PER_DAY=200    # 1日の制限を変更
MAX_REQUESTS_PER_HOUR=50    # 1時間の制限を変更
```

## 安全な使用のための推奨事項

1. **バックアップ**: 重要なプロンプトはバージョン管理
2. **テスト実行**: 本番前に`./queue_manager.sh run`で手動テスト
3. **ログ監視**: 定期的に`state/cron.log`を確認
4. **使用量管理**: `check_usage.sh`で過剰実行を防止
5. **依存関係**: 複雑な依存関係は図示して管理

## よくある使用パターン

### パターン1: 毎朝の環境確認

```csv
# Cron: 0 9 * * 1-5 (平日朝9時)
id,priority,working_dir,prompt,status,scheduled_time,dependencies
100,high,/home/ken/Spr_ws,"環境診断スクリプトを実行",pending,,
101,medium,/home/ken/Spr_ws,"ビルド状態を確認",pending,,100
```

### パターン2: 夜間バッチ処理

```csv
# Cron: 0 22 * * * (毎日22時)
id,priority,working_dir,prompt,status,scheduled_time,dependencies
200,high,/home/ken/Spr_ws,"全プロジェクトのテストを実行",pending,,
201,medium,/home/ken/Spr_ws,"カバレッジレポートを生成",pending,,200
202,low,/home/ken/Spr_ws,"結果をREPORT.mdにまとめる",pending,,201
```

## ライセンス

このスクリプト集は自由に使用・改変していただいて構いません。

---

**作成日**: 2025-12-14
**対象**: Claude Code自動化
**目的**: トークン使用の効率化と作業の自動化
