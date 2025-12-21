#!/usr/bin/env python3
"""
Claude API Executor - 完全自動実行版
Anthropic APIを使用してプロンプトを自動実行
"""

import os
import sys
import json
from datetime import datetime
from pathlib import Path

try:
    import anthropic
except ImportError:
    print("❌ エラー: anthropic パッケージがインストールされていません")
    print("インストール: pip install anthropic")
    sys.exit(1)


class ClaudeAPIExecutor:
    def __init__(self, api_key=None):
        """
        Claude API実行環境を初期化

        Args:
            api_key: Anthropic APIキー（Noneの場合は環境変数から取得）
        """
        self.api_key = api_key or os.environ.get("ANTHROPIC_API_KEY")
        if not self.api_key:
            raise ValueError(
                "APIキーが設定されていません。\n"
                "以下のいずれかの方法で設定してください:\n"
                "1. 環境変数: export ANTHROPIC_API_KEY='your-key'\n"
                "2. .env ファイル: ANTHROPIC_API_KEY=your-key\n"
                "3. config.json ファイル"
            )

        self.client = anthropic.Anthropic(api_key=self.api_key)
        self.model = "claude-sonnet-4-5-20250929"
        self.max_tokens = 8000

    def execute(self, working_dir, prompt, task_id=None):
        """
        プロンプトを実行

        Args:
            working_dir: 作業ディレクトリ
            prompt: 実行するプロンプト
            task_id: タスクID（ログ用）

        Returns:
            dict: {
                "success": bool,
                "output": str,
                "usage": dict,
                "error": str (if failed)
            }
        """
        try:
            # 作業ディレクトリが存在するか確認
            if not os.path.isdir(working_dir):
                return {
                    "success": False,
                    "error": f"作業ディレクトリが存在しません: {working_dir}",
                    "output": "",
                    "usage": {}
                }

            # プロンプトを構築
            full_prompt = self._build_prompt(working_dir, prompt)

            print(f"🤖 Claude APIにリクエスト送信中...")
            print(f"   モデル: {self.model}")
            print(f"   作業ディレクトリ: {working_dir}")

            # APIリクエスト
            message = self.client.messages.create(
                model=self.model,
                max_tokens=self.max_tokens,
                messages=[{
                    "role": "user",
                    "content": full_prompt
                }]
            )

            # レスポンスを処理
            output_text = self._extract_text(message.content)
            usage_info = {
                "input_tokens": message.usage.input_tokens,
                "output_tokens": message.usage.output_tokens,
                "total_tokens": message.usage.input_tokens + message.usage.output_tokens
            }

            print(f"✅ 実行完了")
            print(f"   入力トークン: {usage_info['input_tokens']}")
            print(f"   出力トークン: {usage_info['output_tokens']}")
            print(f"   合計トークン: {usage_info['total_tokens']}")

            return {
                "success": True,
                "output": output_text,
                "usage": usage_info,
                "error": None
            }

        except anthropic.RateLimitError as e:
            # 使用量制限エラー
            return {
                "success": False,
                "error": f"API使用量制限: {str(e)}",
                "output": "",
                "usage": {},
                "quota_exceeded": True
            }

        except anthropic.APIError as e:
            # その他のAPIエラー
            return {
                "success": False,
                "error": f"APIエラー: {str(e)}",
                "output": "",
                "usage": {}
            }

        except Exception as e:
            # 予期しないエラー
            return {
                "success": False,
                "error": f"予期しないエラー: {str(e)}",
                "output": "",
                "usage": {}
            }

    def _build_prompt(self, working_dir, prompt):
        """
        完全なプロンプトを構築

        Args:
            working_dir: 作業ディレクトリ
            prompt: ユーザープロンプト

        Returns:
            str: 構築されたプロンプト
        """
        return f"""あなたは作業ディレクトリ {working_dir} で作業を行います。

以下のタスクを実行してください:

{prompt}

重要な注意事項:
- 作業ディレクトリ内のファイルを読む必要がある場合は、絶対パスを使用してください
- ファイルを作成・編集する場合は、その内容を明確に示してください
- コマンドを実行する必要がある場合は、その結果を記述してください
- 調査結果や分析結果は、明確で構造化された形式で出力してください
"""

    def _extract_text(self, content):
        """
        APIレスポンスからテキストを抽出

        Args:
            content: APIレスポンスのcontentブロック

        Returns:
            str: 抽出されたテキスト
        """
        if isinstance(content, list):
            texts = []
            for block in content:
                if hasattr(block, 'text'):
                    texts.append(block.text)
                elif isinstance(block, dict) and 'text' in block:
                    texts.append(block['text'])
            return '\n'.join(texts)
        elif hasattr(content, 'text'):
            return content.text
        else:
            return str(content)

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
            f.write("=" * 80 + "\n\n")

            if result['success']:
                f.write("【実行結果】\n\n")
                f.write(result['output'])
                f.write("\n\n" + "=" * 80 + "\n")
                f.write(f"使用トークン: {result['usage'].get('total_tokens', 0)}\n")
            else:
                f.write("【エラー】\n\n")
                f.write(result['error'])

        # JSON出力（詳細情報）
        with open(json_file, 'w', encoding='utf-8') as f:
            json.dump({
                'task_id': task_id,
                'timestamp': timestamp,
                'success': result['success'],
                'usage': result['usage'],
                'error': result.get('error'),
                'output_file': output_file
            }, f, ensure_ascii=False, indent=2)

        return output_file


def load_api_key():
    """
    APIキーを様々なソースから読み込む

    優先順位:
    1. 環境変数 ANTHROPIC_API_KEY
    2. .env ファイル
    3. config.json ファイル

    Returns:
        str: APIキー
    """
    # 1. 環境変数
    api_key = os.environ.get("ANTHROPIC_API_KEY")
    if api_key:
        return api_key

    # 2. .env ファイル
    script_dir = Path(__file__).parent
    env_file = script_dir / ".env"
    if env_file.exists():
        with open(env_file) as f:
            for line in f:
                line = line.strip()
                if line.startswith("ANTHROPIC_API_KEY="):
                    return line.split("=", 1)[1].strip().strip('"').strip("'")

    # 3. config.json ファイル
    config_file = script_dir / "config.json"
    if config_file.exists():
        with open(config_file) as f:
            config = json.load(f)
            if "api_key" in config:
                return config["api_key"]

    return None


def main():
    """
    コマンドライン実行のエントリーポイント

    使用法:
        python claude_api_executor.py <working_dir> <prompt> [task_id]
    """
    if len(sys.argv) < 3:
        print("使用法: python claude_api_executor.py <working_dir> <prompt> [task_id]")
        sys.exit(1)

    working_dir = sys.argv[1]
    prompt = sys.argv[2]
    task_id = sys.argv[3] if len(sys.argv) > 3 else "unknown"

    # APIキーを読み込む
    api_key = load_api_key()
    if not api_key:
        print("❌ エラー: APIキーが設定されていません")
        print("\n以下のいずれかの方法で設定してください:")
        print("1. 環境変数: export ANTHROPIC_API_KEY='your-api-key'")
        print("2. .env ファイルを作成:")
        print("   echo 'ANTHROPIC_API_KEY=your-api-key' > .env")
        print("3. config.json ファイルを作成:")
        print('   echo \'{"api_key": "your-api-key"}\' > config.json')
        print("\nAPIキーの取得: https://console.anthropic.com/")
        sys.exit(2)  # 終了コード2: 使用量制限（APIキー未設定も含む）

    try:
        # 実行
        executor = ClaudeAPIExecutor(api_key)
        result = executor.execute(working_dir, prompt, task_id)

        # 出力を保存
        script_dir = Path(__file__).parent
        output_dir = script_dir / "state" / "outputs"
        output_file = executor.save_output(output_dir, task_id, result)

        print(f"\n📄 出力ファイル: {output_file}")

        # 終了コード
        if result['success']:
            sys.exit(0)  # 成功
        elif result.get('quota_exceeded'):
            sys.exit(2)  # 使用量制限
        else:
            sys.exit(1)  # エラー

    except ValueError as e:
        print(f"❌ {e}")
        sys.exit(2)
    except Exception as e:
        print(f"❌ 予期しないエラー: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
