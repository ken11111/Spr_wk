# Claude自動操作の実装オプション

## 📋 概要

Claude を自動操作する方法は主に3つあります。

| 方法 | 難易度 | コスト | 完全自動化 | 推奨度 |
|------|--------|--------|-----------|--------|
| **①API使用** | 低 | 有料 | ✅ | ⭐⭐⭐⭐⭐ 推奨 |
| **②Web自動化** | 中 | 無料 | ✅ | ⭐⭐⭐ 条件付き |
| **③CLI自動化** | 高 | 無料 | △ | ⭐⭐ 非推奨 |

---

## ① API使用（現在の実装）

### 概要

Anthropic公式APIを使用（既に実装済み）

### 特徴

**メリット**:
- ✅ 公式サポート
- ✅ 安定性が高い
- ✅ レート制限が明確
- ✅ ドキュメント完備
- ✅ ファイル操作が容易

**デメリット**:
- ❌ 有料（従量課金）
- ❌ APIキーが必要

### コスト

- Claude Sonnet 4.5: 約5円/タスク
- 月間300タスク: 約1,500円

### 実装状況

✅ **既に実装済み** (`claude_api_executor.py`)

---

## ② Web自動化（Selenium/Playwright）

### 概要

claude.ai のWebインターフェースをPythonで自動操作

### 技術スタック

```python
# Selenium使用
from selenium import webdriver
from selenium.webdriver.common.by import By
from selenium.webdriver.support.ui import WebDriverWait

# または Playwright使用
from playwright.sync_api import sync_playwright
```

### 実装可能性

**可能な操作**:
1. ✅ ログイン自動化
2. ✅ プロンプト送信
3. ✅ 応答の取得
4. ✅ チャット履歴の保存
5. ✅ ファイルアップロード

### サンプル実装（Playwright）

```python
#!/usr/bin/env python3
"""
Claude Web自動操作サンプル（Playwright使用）
"""

from playwright.sync_api import sync_playwright
import time

def automate_claude_web(prompt, email, password):
    with sync_playwright() as p:
        # ブラウザ起動
        browser = p.chromium.launch(headless=False)
        page = browser.new_page()

        # Claude.aiにアクセス
        page.goto("https://claude.ai")

        # ログイン（初回のみ）
        # ※セッション保存で2回目以降は不要
        if page.query_selector("text=Sign In"):
            page.click("text=Sign In")
            page.fill("input[type=email]", email)
            page.click("button:has-text('Continue')")
            # 認証コード入力待ち...
            time.sleep(30)  # メール確認時間

        # 新しいチャット作成
        page.click("button:has-text('New Chat')")

        # プロンプト入力
        page.fill("textarea[placeholder='Talk to Claude...']", prompt)
        page.press("textarea", "Enter")

        # 応答待ち
        page.wait_for_selector("div.message:has-text('Claude')")
        time.sleep(3)  # 応答完了待ち

        # 応答取得
        response = page.query_selector("div.message:last-child").inner_text()

        browser.close()
        return response

# 使用例
result = automate_claude_web(
    prompt="こんにちは",
    email="your@email.com",
    password="your-password"
)
print(result)
```

### メリット

- ✅ 無料（Claude Free/Proプラン内）
- ✅ APIキー不要
- ✅ 既存のClaudeアカウント使用
- ✅ Web UI の全機能が使用可能

### デメリット

- ❌ ブラウザが必要（ヘッドレスも可）
- ❌ UI変更で動作しなくなる可能性
- ❌ 認証が複雑（メール認証等）
- ❌ レート制限が不明確
- ❌ 利用規約違反の可能性

### 実装の課題

#### 1. 認証の問題

```python
# セッションクッキー保存で解決可能
browser = p.chromium.launch(headless=False)
context = browser.new_context(storage_state="session.json")

# 初回ログイン後、session.jsonに保存
context.storage_state(path="session.json")
```

#### 2. UI要素の特定

```python
# CSSセレクタが変わると動作しない
page.fill("textarea[placeholder='Talk to Claude...']", prompt)

# より堅牢な方法: data属性やrole属性を使用
page.fill("[role=textbox]", prompt)
```

#### 3. レート制限

- Claude Free: 制限あり（不明確）
- Claude Pro: より高い制限
- 過度な使用はアカウント停止リスク

### 利用規約の考慮

**Anthropic 利用規約**:
- 自動化ツールの使用は明示的に禁止されていない
- ただし、過度な負荷や不正使用は禁止
- API使用が推奨されている

**推奨**:
- 個人使用の範囲内で節度を持って使用
- 商用利用の場合はAPI使用を推奨

---

## ③ CLI自動化（Claude Code CLI）

### 概要

`claude` コマンドをPythonから自動制御

### 技術的課題

Claude Code CLIは対話型ツールのため、以下の課題があります:

#### 1. 標準入力の問題

```python
# ❌ 単純なパイプは動作しない
import subprocess
result = subprocess.run(
    ["claude"],
    input="プロンプト",
    capture_output=True
)
# → 対話モードが起動してしまう
```

#### 2. PTY（疑似端末）を使用

```python
# ✅ ptyを使えば可能
import pty
import os
import subprocess

def automate_claude_cli(prompt):
    master, slave = pty.openpty()

    process = subprocess.Popen(
        ["claude"],
        stdin=slave,
        stdout=slave,
        stderr=slave
    )

    os.write(master, (prompt + "\n").encode())
    time.sleep(5)

    output = os.read(master, 10000).decode()
    process.terminate()

    return output
```

#### 3. expect使用

```python
# ✅ pexpect ライブラリを使用
import pexpect

def automate_claude_cli(prompt):
    child = pexpect.spawn("claude")
    child.expect("›")  # プロンプト待ち
    child.sendline(prompt)
    child.expect("›", timeout=60)  # 応答待ち

    output = child.before.decode()
    child.close()

    return output
```

### サンプル実装（pexpect使用）

```python
#!/usr/bin/env python3
"""
Claude CLI自動操作サンプル（pexpect使用）
"""

import pexpect
import sys

def automate_claude_cli(working_dir, prompt):
    """
    Claude Code CLIを自動操作

    Args:
        working_dir: 作業ディレクトリ
        prompt: 実行するプロンプト

    Returns:
        str: Claude の応答
    """
    # 作業ディレクトリに移動してClaude起動
    child = pexpect.spawn("bash", ["-c", f"cd {working_dir} && claude"])

    try:
        # プロンプト待ち（最大30秒）
        child.expect("›", timeout=30)

        # プロンプト送信
        child.sendline(prompt)

        # 応答待ち（最大300秒）
        index = child.expect(["›", pexpect.EOF, pexpect.TIMEOUT], timeout=300)

        if index == 0:
            # 正常終了
            output = child.before.decode('utf-8')
        elif index == 1:
            # EOF
            output = child.before.decode('utf-8')
        else:
            # タイムアウト
            output = "TIMEOUT"

        # 終了
        child.sendline("exit")
        child.close()

        return output

    except Exception as e:
        child.close()
        return f"ERROR: {str(e)}"

# 使用例
if __name__ == "__main__":
    result = automate_claude_cli(
        working_dir="/home/ken/Spr_ws",
        prompt="README.mdを読んで要約してください"
    )
    print(result)
```

### メリット

- ✅ 無料
- ✅ APIキー不要
- ✅ ファイル操作が容易

### デメリット

- ❌ 実装が複雑
- ❌ 不安定（タイミング依存）
- ❌ セッション管理が難しい
- ❌ エラーハンドリングが困難
- ❌ Claude Code CLIの仕様変更で動作しなくなる

---

## 🎯 推奨される実装方法

### 現在の状況に応じた推奨

#### ケース1: コストを払える（推奨）

**→ ① API使用（既に実装済み）**

理由:
- 最も安定
- 公式サポート
- 月1,500円程度で完全自動化

#### ケース2: 無料で自動化したい

**→ ② Web自動化（Playwright）**

理由:
- API次に安定
- 実装難易度: 中
- セッション保存で認証問題解決可能

**実装手順**:
1. Playwright インストール
2. ログイン自動化
3. セッション保存
4. プロンプト送信・応答取得

#### ケース3: 検証・実験目的

**→ ③ CLI自動化（pexpect）**

理由:
- 技術的に興味深い
- ローカル環境で完結

---

## 📊 比較表

| 項目 | API | Web自動化 | CLI自動化 |
|------|-----|----------|----------|
| **安定性** | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐ |
| **コスト** | 有料 | 無料* | 無料 |
| **実装難易度** | 低 | 中 | 高 |
| **保守性** | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐ |
| **速度** | 速い | 遅い（ブラウザ） | 中 |
| **ファイル操作** | API経由 | アップロード必要 | 直接アクセス |

*Claude Pro契約が必要な場合あり

---

## 💡 実装提案

### オプションA: API版を継続（推奨）

**理由**:
- 既に実装済み
- 最も安定
- 月1,500円程度

### オプションB: Web自動化版を追加実装

**実装内容**:
- `claude_web_executor.py` を作成
- Playwright使用
- セッション保存
- API版のフォールバック

**使い分け**:
- 重要タスク: API版
- 軽いタスク: Web自動化版

### オプションC: ハイブリッド

**設定ファイルで切り替え**:
```json
{
  "executor": "api",  // api, web, cli
  "api_key": "...",
  "web_session": "session.json"
}
```

---

## ❓ どの方法を実装しますか？

1. **API版のまま（推奨）** - 既に完成、最も安定
2. **Web自動化版を追加実装** - 無料、実装可能
3. **CLI自動化版を追加実装** - 技術的挑戦、不安定
4. **ハイブリッド版** - 全て実装、切り替え可能

ご希望を教えてください！

---

**作成日**: 2025-12-14
**目的**: Claude自動操作の実装オプション比較
