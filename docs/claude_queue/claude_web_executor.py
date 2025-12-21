#!/usr/bin/env python3
"""
Claude Web自動操作スクリプト（Playwright使用）
Claude Code使用量を有効活用するための完全自動実行

特徴:
- 既存のClaudeアカウントを使用（無料 or Pro）
- API料金不要
- セッション保存で認証を自動化
"""

import os
import sys
import json
import time
from datetime import datetime
from pathlib import Path

try:
    from playwright.sync_api import sync_playwright, TimeoutError as PlaywrightTimeout
except ImportError:
    print("❌ エラー: playwright パッケージがインストールされていません")
    print("インストール: pip install playwright")
    print("ブラウザインストール: playwright install chromium")
    sys.exit(1)


class ClaudeWebExecutor:
    def __init__(self, session_file=None, headless=True):
        """
        Claude Web自動実行環境を初期化

        Args:
            session_file: セッションファイルのパス（認証情報保存）
            headless: ヘッドレスモード（Trueで非表示）
        """
        self.session_file = session_file or str(Path(__file__).parent / "session.json")
        self.headless = headless
        self.claude_url = "https://claude.ai"

    def execute(self, working_dir, prompt, task_id=None, timeout=300):
        """
        プロンプトを実行

        Args:
            working_dir: 作業ディレクトリ（参考情報として使用）
            prompt: 実行するプロンプト
            task_id: タスクID（ログ用）
            timeout: タイムアウト（秒）

        Returns:
            dict: {
                "success": bool,
                "output": str,
                "error": str (if failed)
            }
        """
        with sync_playwright() as p:
            try:
                # ブラウザ起動
                print(f"🌐 ブラウザ起動中...")
                browser = p.chromium.launch(headless=self.headless)

                # セッション読み込み（ある場合）
                if os.path.exists(self.session_file):
                    print(f"✅ セッション読み込み: {self.session_file}")
                    context = browser.new_context(storage_state=self.session_file)
                else:
                    print(f"⚠️  セッションなし - 初回ログインが必要です")
                    context = browser.new_context()

                page = context.new_page()

                # Claude.aiにアクセス
                print(f"🔗 Claude.ai にアクセス中...")
                page.goto(self.claude_url, wait_until="networkidle")

                # ログイン確認
                if not self._is_logged_in(page):
                    print("❌ ログインが必要です")
                    print("\n【初回セットアップ】")
                    print("1. 以下のコマンドで対話モードでログイン:")
                    print(f"   python3 {__file__} --setup")
                    print("")
                    browser.close()
                    return {
                        "success": False,
                        "error": "ログインが必要です。--setup オプションで初回設定を実行してください",
                        "output": ""
                    }

                print("✅ ログイン済み")

                # プロンプトに作業ディレクトリ情報を追加
                full_prompt = self._build_prompt(working_dir, prompt)

                # プロンプト送信
                print(f"📤 プロンプト送信中...")
                response = self._send_prompt_and_wait(page, full_prompt, timeout)

                # セッション保存
                context.storage_state(path=self.session_file)

                browser.close()

                print(f"✅ 実行完了")
                return {
                    "success": True,
                    "output": response,
                    "error": None
                }

            except PlaywrightTimeout as e:
                print(f"⏱️  タイムアウト: {str(e)}")
                browser.close()
                return {
                    "success": False,
                    "error": f"タイムアウト: {str(e)}",
                    "output": ""
                }

            except Exception as e:
                print(f"❌ エラー: {str(e)}")
                try:
                    browser.close()
                except:
                    pass
                return {
                    "success": False,
                    "error": str(e),
                    "output": ""
                }

    def setup_session(self):
        """
        初回ログインセッション設定（対話モード）
        """
        print("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
        print("🔐 Claude Web - 初回ログイン設定")
        print("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
        print("")
        print("ブラウザが起動します。")
        print("Claude.ai にログインしてください。")
        print("ログイン完了後、このウィンドウに戻って Enter を押してください。")
        print("")

        with sync_playwright() as p:
            browser = p.chromium.launch(headless=False)  # 表示モード
            context = browser.new_context()
            page = context.new_page()

            # Claude.aiにアクセス
            page.goto(self.claude_url)

            # ユーザーがログインするまで待機
            input("ログインが完了したら Enter を押してください...")

            # ログイン確認
            if self._is_logged_in(page):
                print("✅ ログイン成功")
                # セッション保存
                context.storage_state(path=self.session_file)
                print(f"✅ セッション保存: {self.session_file}")
                print("")
                print("セットアップ完了！これで自動実行が可能です。")
            else:
                print("❌ ログイン確認できませんでした")
                print("もう一度やり直してください。")

            browser.close()

    def _is_logged_in(self, page):
        """
        ログイン状態を確認

        Args:
            page: Playwrightページオブジェクト

        Returns:
            bool: ログイン済みならTrue
        """
        try:
            # チャット入力欄があればログイン済み
            page.wait_for_selector("div[contenteditable='true']", timeout=5000)
            return True
        except:
            return False

    def _build_prompt(self, working_dir, prompt):
        """
        作業ディレクトリ情報を含むプロンプトを構築

        Args:
            working_dir: 作業ディレクトリ
            prompt: ユーザープロンプト

        Returns:
            str: 構築されたプロンプト
        """
        return f"""作業ディレクトリ: {working_dir}

{prompt}

注: ファイルパスは絶対パスで指定してください。
"""

    def _send_prompt_and_wait(self, page, prompt, timeout):
        """
        プロンプトを送信して応答を待つ

        Args:
            page: Playwrightページオブジェクト
            prompt: 送信するプロンプト
            timeout: タイムアウト（秒）

        Returns:
            str: Claudeの応答
        """
        # チャット入力欄を探す
        input_selector = "div[contenteditable='true']"
        page.wait_for_selector(input_selector, timeout=10000)

        # プロンプト入力
        input_box = page.locator(input_selector)
        input_box.fill(prompt)

        # 送信（Enterキー）
        page.keyboard.press("Enter")

        # 応答待ち
        # Claudeの応答は通常、特定のクラス名を持つdiv要素に表示される
        # UI構造に依存するため、調整が必要な場合あり

        time.sleep(3)  # 初期応答待ち

        # 応答完了を待つ（ストリーミング完了）
        # 「Stop generating」ボタンがなくなるまで待つ
        max_wait = timeout
        waited = 0
        while waited < max_wait:
            try:
                # 生成中ボタンがあるかチェック
                stop_button = page.query_selector("button:has-text('Stop')")
                if not stop_button:
                    # 生成完了
                    break
            except:
                break

            time.sleep(1)
            waited += 1

        # 最後のメッセージを取得
        # Claude の応答は最新のメッセージブロックに表示される
        try:
            # メッセージ要素を全て取得
            messages = page.query_selector_all("div[data-test-render-count]")
            if messages:
                # 最後のメッセージ（Claude の応答）
                last_message = messages[-1]
                response_text = last_message.inner_text()
                return response_text
            else:
                # フォールバック: より広範囲で探す
                all_text = page.locator("main").inner_text()
                return all_text
        except Exception as e:
            print(f"⚠️  応答取得エラー: {e}")
            # フォールバック
            return page.locator("main").inner_text()

    def save_output(self, output_dir, task_id, result):
        """
        実行結果をファイルに保存

        Args:
            output_dir: 出力ディレクトリ
            task_id: タスクID
            result: 実行結果
        """
        os.makedirs(output_dir, exist_ok=True)

        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        output_file = os.path.join(output_dir, f"task_{task_id}_{timestamp}.txt")
        json_file = os.path.join(output_dir, f"task_{task_id}_{timestamp}.json")

        # テキスト出力
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write(f"タスクID: {task_id}\n")
            f.write(f"実行時刻: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write(f"実行方法: Web自動化（Playwright）\n")
            f.write("=" * 80 + "\n\n")

            if result['success']:
                f.write("【実行結果】\n\n")
                f.write(result['output'])
            else:
                f.write("【エラー】\n\n")
                f.write(result['error'])

        # JSON出力（詳細情報）
        with open(json_file, 'w', encoding='utf-8') as f:
            json.dump({
                'task_id': task_id,
                'timestamp': timestamp,
                'success': result['success'],
                'method': 'web_automation',
                'error': result.get('error'),
                'output_file': output_file
            }, f, ensure_ascii=False, indent=2)

        return output_file


def main():
    """
    コマンドライン実行のエントリーポイント

    使用法:
        python claude_web_executor.py <working_dir> <prompt> [task_id]
        python claude_web_executor.py --setup  (初回セットアップ)
    """
    # セットアップモード
    if len(sys.argv) == 2 and sys.argv[1] == "--setup":
        executor = ClaudeWebExecutor(headless=False)
        executor.setup_session()
        sys.exit(0)

    # 通常実行モード
    if len(sys.argv) < 3:
        print("使用法: python claude_web_executor.py <working_dir> <prompt> [task_id]")
        print("初回セットアップ: python claude_web_executor.py --setup")
        sys.exit(1)

    working_dir = sys.argv[1]
    prompt = sys.argv[2]
    task_id = sys.argv[3] if len(sys.argv) > 3 else "unknown"

    # 実行
    executor = ClaudeWebExecutor(headless=True)
    result = executor.execute(working_dir, prompt, task_id)

    # 出力を保存
    script_dir = Path(__file__).parent
    output_dir = script_dir / "state" / "outputs"
    output_file = executor.save_output(output_dir, task_id, result)

    print(f"\n📄 出力ファイル: {output_file}")

    # 終了コード
    if result['success']:
        sys.exit(0)  # 成功
    else:
        # エラーメッセージでレート制限を判定
        error = result.get('error', '').lower()
        if 'rate limit' in error or 'too many requests' in error:
            sys.exit(2)  # 使用量制限
        else:
            sys.exit(1)  # 一般エラー


if __name__ == "__main__":
    main()
