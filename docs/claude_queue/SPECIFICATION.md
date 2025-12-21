# Claude Code自動実行キューシステム 仕様書 v2.0

## 📋 システム概要

CSVベースのプロンプトキュー管理システム。指定時刻・間隔でClaude CLIのタスクを管理し、使用量制限時の自動リトライに対応。

**実行モード**: **インタラクティブ実行**
- キューシステムが次のタスクを `NEXT_TASK.md` に出力
- ユーザーがClaude CLI (`claude`) で手動実行
- 完了後、次のタスクを取得して繰り返し

**主要機能**:
- CSV形式のタスク管理
- 優先度制御（high/medium/low）
- スケジュール実行
- 依存関係管理
- 繰り返し実行（daily/weekly/monthly等）
- 使用量制限時の自動リトライ
- 多重実行防止

## 🗂️ ファイル構成

```
claude_queue/
├── prompts.csv                      - タスクキュー（ユーザー編集）
├── NEXT_TASK.md                     - 次のタスク（自動生成）
├── queue_manager_v2.sh              - キュー管理スクリプト（推奨）
├── queue_manager.sh                 - キュー管理スクリプト（v1）
├── claude_executor_interactive.sh   - インタラクティブ実行
├── claude_executor.sh               - Claude実行（スタブ）
├── check_usage_v2.sh                - 使用量チェック（5時間対応）
├── check_usage.sh                   - 使用量チェック（v1）
├── setup_cron.sh                    - スケジューラー設定
├── state/
│   ├── executed.log                 - 実行完了
│   ├── repeat_tasks.log             - 繰り返しタスク
│   ├── quota_exceeded.log           - 使用量制限保留
│   ├── retry_tasks.log              - リトライ履歴
│   └── outputs/                     - 実行結果
├── SPECIFICATION.md                 - 本仕様書
├── INTERACTIVE_MODE.md              - インタラクティブ実行ガイド
├── QUOTA_HANDLING.md                - 使用量制限詳細仕様
└── README.md                        - 詳細ドキュメント
```

## 📊 CSVフォーマット仕様

### カラム定義

| カラム | 型 | 必須 | デフォルト | 説明 |
|--------|-----|------|----------|------|
| id | 数値 | ✓ | - | 一意のタスクID |
| priority | 文字列 | ✓ | medium | high/medium/low |
| working_dir | パス | ✓ | - | 作業ディレクトリ（絶対パス） |
| prompt | 文字列 | ✓ | - | Claude Codeへのプロンプト |
| status | 文字列 | ✓ | pending | pending/completed/failed |
| scheduled_time | 日時 | - | - | 実行開始時刻（YYYY-MM-DD HH:MM） |
| dependencies | 文字列 | - | - | 依存タスクID（\|区切り） |
| repeat | yes/no | ✓ | no | 繰り返し実行フラグ |
| repeat_interval | 文字列 | - | - | 繰り返し間隔（12h/daily/weekly等） |
| retry_count | 数値 | ✓ | 0 | 現在のリトライ回数 |
| max_retries | 数値 | ✓ | 3 | 最大リトライ回数 |

### サンプル

```csv
id,priority,working_dir,prompt,status,scheduled_time,dependencies,repeat,repeat_interval,retry_count,max_retries
1,high,/home/ken/Spr_ws/spresense,"SDKバージョン確認",pending,,,no,,0,3
10,high,/home/ken/Spr_ws,"AI技術動向調査",pending,,,yes,daily,0,5
20,medium,/home/ken/Spr_ws,"レポート生成",pending,2025-12-15 09:00,10,no,,0,3
```

## 🎯 repeat_interval 指定

| 値 | 間隔 |
|----|------|
| `12h` | 12時間ごと |
| `daily` または `1d` | 毎日（24時間） |
| `3d` | 3日ごと |
| `weekly` または `1w` | 毎週（7日） |
| `2w` | 2週間ごと |
| `monthly` | 毎月（30日） |

## 🔧 コマンド仕様

### queue_manager_v2.sh（インタラクティブモード）

```bash
# 次のタスクをNEXT_TASK.mdに出力
./queue_manager_v2.sh run

# → NEXT_TASK.md が生成される
# → Claude CLI で実行: 「NEXT_TASK.mdを読んで、記載されているタスクを実行してください」
# → 完了後、再度 ./queue_manager_v2.sh run を実行

# キュー状態表示
./queue_manager_v2.sh status

# 履歴リセット
./queue_manager_v2.sh reset
```

**インタラクティブ実行フロー**:
1. `./queue_manager_v2.sh run` → `NEXT_TASK.md` 生成
2. Claude CLI (`claude`) で `NEXT_TASK.md` を読んで実行
3. 完了後、手順1に戻る

詳細は `INTERACTIVE_MODE.md` を参照。

### setup_cron.sh

```bash
# 対話式スケジューラー設定
./setup_cron.sh

# 選択肢:
# 1) 30分ごと
# 2) 1時間ごと
# 3) 平日9-18時の1時間ごと
# 4) 15分ごと
# 5) カスタム
```

### check_usage.sh

```bash
# 使用量チェック
./check_usage.sh

# 戻り値:
# 0 - 実行可能
# 1 - 使用量制限到達
```

## 📈 実行フロー

### 通常タスク

```
1. CSVから次のタスクを取得
   ├─ status = pending
   ├─ 依存関係チェック
   ├─ scheduled_time チェック
   └─ retry_count < max_retries
   ↓
2. 使用量チェック
   ├─ OK → 実行
   └─ NG → 保留（quota_exceeded.log）
   ↓
3. Claude Code実行
   ├─ 成功 → executed.log
   ├─ エラー → retry_count++, リトライ
   └─ 使用量制限 → 保留, 自動リトライ
```

### 繰り返しタスク

```
1. 最終実行時刻 + repeat_interval ≤ 現在時刻？
   ├─ Yes → 実行
   └─ No → スキップ
   ↓
2. 実行（通常タスクと同様）
   ↓
3. repeat_tasks.log に記録
   ↓
4. 次回実行時刻 = 現在時刻 + repeat_interval
```

## 🔄 終了コード定義

| コード | 意味 | 動作 |
|--------|------|------|
| 0 | 正常終了 | executed.log に記録、次へ |
| 1 | 一般エラー | retry_count++、リトライ |
| 2 | 使用量制限 | quota_exceeded.log、自動リトライ |
| 3 | リトライ上限 | スキップ |

## ⏱️ 自動リトライ間隔仕様

### Claude Code使用量リセットサイクル

```
Claude Code使用量は5時間ごとにリセットされる

例: 10:00に制限到達
↓
15:00（5時間後）に使用量リセット
↓
自動リトライで実行再開
```

### リトライ間隔の推奨設定

| シナリオ | Cron間隔 | 理由 |
|---------|---------|------|
| **推奨** | **30分ごと** | 5時間以内に10回試行可能 |
| 高頻度 | 15分ごと | 5時間以内に20回試行（過剰） |
| 低頻度 | 1時間ごと | 5時間以内に5回試行 |
| 特殊 | 4.5-5時間ごと | リセット直後を狙う |

### リトライタイミングの計算

**Cron 30分ごとの場合**:
```
10:00 - 使用量制限到達
10:30 - リトライ1回目（失敗）
11:00 - リトライ2回目（失敗）
11:30 - リトライ3回目（失敗）
...
15:00 - 使用量リセット
15:00 - リトライ10回目（成功！）
```

**最適化: 4.5時間待機**:
```
10:00 - 使用量制限到達
    ↓ scheduled_time を 14:30 に設定
14:30 - リトライ（高確率で成功）
```

### scheduled_time による制御

使用量制限時、次回リトライを4.5時間後に設定:

```csv
# 使用量制限到達時
id,priority,...,scheduled_time,...
10,high,...,2025-12-14 14:30,...  ← 自動設定（現在時刻+4.5h）
```

**実装**:
```bash
# 使用量制限検出時
current_time=$(date +%s)
next_retry=$((current_time + 16200))  # 4.5時間 = 16200秒
next_retry_str=$(date -d "@$next_retry" '+%Y-%m-%d %H:%M')

# CSVのscheduled_timeを更新
# → 次回Cron実行時、scheduled_timeをチェックしてスキップ
```

## 📊 優先度制御

### 実行順序

```
1. priority でソート（high > medium > low）
2. 同優先度の場合、CSV上の順序
3. 繰り返しタスクと通常タスクは区別なし
```

### 例

```csv
id,priority,...
5,high,...      ← 1番目に実行
2,high,...      ← 2番目
10,medium,...   ← 3番目
3,low,...       ← 4番目
```

## 🔒 多重実行防止

### ロックファイル機構

```bash
state/queue.lock
├─ 実行開始時に作成（PIDを記録）
├─ 実行終了時に削除
└─ 既存ロック確認（プロセス存在チェック）
```

## 📝 ログファイル仕様

### executed.log
```
形式: ID,実行日時,実行時間(秒),終了コード,プロンプト
例: 1,2025-12-14 10:30:00,45,0,SDKバージョン確認
```

### repeat_tasks.log
```
形式: ID,実行時刻(epoch),実行時間(秒),終了コード,間隔,プロンプト
例: 10,1734163800,120,0,daily,AI技術動向調査
```

### quota_exceeded.log
```
形式: ID,実行日時,実行時間(秒),ステータス,プロンプト
例: 5,2025-12-14 15:30:00,2,quota_exceeded,レポート生成
```

### retry_tasks.log
```
形式: ID,実行日時,実行時間(秒),終了コード,リトライ回数,最大回数,プロンプト
例: 3,2025-12-14 11:00:00,30,1,2,3,データ分析
```

## 🎯 使用例

### 1. 技術動向調査の自動化

```csv
10,high,/home/ken/Spr_ws,"AI/機械学習の最新論文を調査",pending,,,yes,daily,0,5
11,high,/home/ken/Spr_ws,"GitHub Trendingをチェック",pending,,,yes,12h,0,5
```

### 2. 定期レポート生成

```csv
20,high,/home/ken/Spr_ws,"週次レポート生成",pending,,,yes,weekly,0,3
21,medium,/home/ken/Spr_ws,"月次サマリー作成",pending,,,yes,monthly,0,3
```

### 3. 段階的処理

```csv
30,high,/home/ken/Spr_ws,"データ収集",pending,,,no,,0,3
31,high,/home/ken/Spr_ws,"データ分析",pending,,30,no,,0,3
32,medium,/home/ken/Spr_ws,"レポート作成",pending,,31,no,,0,3
```

## ⚙️ 設定値

### check_usage.sh

```bash
MAX_REQUESTS_PER_DAY=100    # 1日の最大実行回数
MAX_REQUESTS_PER_HOUR=20    # 1時間の最大実行回数
```

### Cron推奨設定

```cron
# 30分ごと（推奨）
*/30 * * * * /path/to/queue_manager_v2.sh run

# 15分ごと（高頻度）
*/15 * * * * /path/to/queue_manager_v2.sh run

# 1時間ごと（低頻度）
0 * * * * /path/to/queue_manager_v2.sh run
```

## 🔧 技術仕様

### 対応OS
- Linux (WSL2含む)
- macOS

### 必須コマンド
- bash (4.0以降)
- date (GNU coreutils)
- grep, awk, sed
- crontab

### オプション
- Claude Code CLI
- curl (API経由実行時)

## 📊 パフォーマンス

### 処理性能
- CSV読み込み: O(n) - タスク数に比例
- 優先度ソート: O(n log n)
- 次タスク取得: 1秒以内（1000タスクまで）

### リソース使用量
- メモリ: <10MB
- ディスク: ログファイルのみ（可変）
- CPU: ほぼゼロ（待機時）

## 🛡️ 制限事項

### CSVサイズ
- 推奨: 1000行以下
- 最大: 10000行（パフォーマンス低下）

### プロンプト長
- 推奨: 500文字以内
- 最大: CSV読み込み可能範囲

### 同時実行
- 不可（ロックファイルで制御）

### 依存関係
- 循環依存は未検出（ユーザー責任）
- 深さ制限なし

## 🔄 バージョン履歴

### v2.0 (2025-12-14)
- 使用量制限時の自動リトライ機構追加
- retry_count, max_retries カラム追加
- quota_exceeded.log, retry_tasks.log 追加
- 終了コード体系の整理

### v1.0 (2025-12-14)
- 初版リリース
- 基本的なキュー管理機能
- 繰り返し実行機能
- スケジュール実行機能

## 📞 サポート情報

### ドキュメント
- `README.md` - 詳細ドキュメント
- `QUOTA_HANDLING.md` - 使用量制限仕様
- `SPECIFICATION.md` - 本仕様書（簡潔版）

### トラブルシューティング
- README.md の「トラブルシューティング」セクション参照

### カスタマイズ
- `claude_executor.sh` - Claude Code実行方法
- `check_usage.sh` - 使用量制限値

---

**仕様バージョン**: 2.0
**最終更新**: 2025-12-14
**対象システム**: Claude Code自動実行キューシステム
