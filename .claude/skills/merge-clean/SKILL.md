---
name: merge-clean
description: 作業ブランチに溜まった未コミット変更を論理的なコミットに分割し、ビルド成果物を .gitignore で除外整理した上で、main へ --no-ff マージするまでの一連の作業を実行する。push は明示的指示があるまで行わない。
---

# Clean Merge Workflow

未整理のブランチを論理コミットに分割 → ビルド成果物の除外 → `--no-ff` マージまでを一括実行するスキル。

## 入力

- 引数 (任意): マージ先ブランチ名 (デフォルト `main`)
- 現在のブランチが作業ブランチである前提

## 手順

### 1. 状態の把握

並列で実行:
- `git status` (`-uall` は使わない)
- `git diff --stat`
- `git log --oneline <target>..HEAD` でコミット先行差分
- `git ls-files | grep -E '\.(o|a|d|elf|bin|spk)$'` で誤って tracked になったビルド成果物を検出

### 2. プラン提示 (実行前に必ず)

- 検出したビルド成果物の一覧
- 提案するコミット分割案 (例: `chore: 成果物クリーンアップ` + `feat: <機能>`)
- マージ方式 (`--no-ff` 固定、マージコミットメッセージのドラフト)
- これをユーザーに見せて「go」を待つ

### 3. ビルド成果物のクリーンアップ (該当する場合のみ)

- `.gitignore` を Read してから Edit で対応パターンを追記 (`*.o` など)
- `git rm --cached <files>` で untrack
- 必要ならワーキングツリーからも削除 (`rm <files>`)
- `chore: ビルド成果物 X を git 管理から除外` でコミット

### 4. 機能変更のコミット

- 関連ファイルを論理単位でグルーピングして `git add <specific files>` (`git add -A` は使わない)
- コミットメッセージは:
  - 1行目: 短い要約 (Phase 番号や機能名を含める)
  - 本文: なぜ・何を変更したかを箇条書き
  - 末尾: `Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>`
- HEREDOC で渡す:
  ```
  git commit -m "$(cat <<'EOF'
  <タイトル>

  <本文>

  Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
  EOF
  )"
  ```

### 5. サブモジュールの取り扱い

- サブモジュール内部の untracked/modified は **触らない**
- 親リポジトリのポインタが動いていない限り、サブモジュール変更はコミット対象外

### 6. `--no-ff` マージ

- `git checkout <target>` (デフォルト `main`)
- `git merge --no-ff <feature> -m "<merge message>"`
- マージコミットメッセージには含まれるコミット一覧を箇条書きで載せる

### 7. 検証と報告

- `git log --oneline --graph -10` でグラフ確認
- `git status` でクリーンであることを確認
- 以下をユーザーに報告:
  - マージ前後の HEAD SHA
  - origin との差分 (先行コミット数)
  - 残課題 (push 要否、ブランチ削除要否)

## 禁止事項

- ユーザーの明示的指示なしに `git push` しない
- ユーザーの明示的指示なしに作業ブランチを削除しない
- `git add -A` / `git add .` は使わない (機微ファイル混入を防ぐ)
- フックスキップ (`--no-verify`) は使わない
- 既存コミットの `--amend` は使わない (新規コミットを作る)

## 過去の成功パターン

phase11-adaptive-control → main マージ (2026-04-26) で確立:
- `chore: ビルド成果物 .o ファイルを git 管理から除外`
- `Phase 11.1 多変数制御基盤実装 - frame_statistics と enhanced_control`
- `Merge branch 'phase11-adaptive-control' into main` (--no-ff)
