# Phase 2: NuttXビルドシステムへの統合

## 目的
開発したアプリケーションをNuttXのビルトインコマンドとして正しく登録する

## 重要な前提知識

このフェーズは**最も難しい**部分です。以下の点を理解しておくことが重要：

1. **NuttXの.configとSDKの.configは別物**
2. **CONFIG変数の命名規則は厳格**
3. **Make.defsがアプリ登録の核心**
4. **既存の動作例との比較が最も有効**

## Claude Codeへの効果的なプロンプト例

### プロンプト1: NuttXビルドシステムの理解

```
Spresense SDK (NuttX 12.3.0)で、カスタムアプリケーションを
ビルトインコマンドとして登録する方法を理解したいです。

【質問】
1. アプリケーションをビルトインコマンドとして登録するために必要なファイルは何ですか？
2. Kconfig, Makefile, Make.defsの役割をそれぞれ説明してください
3. CONFIG変数の命名規則について教えてください
4. builtin_list.hに登録されるメカニズムを説明してください

【参考にしたい既存アプリ】
- sdk/apps/examples/sixaxis/ （BMI160と似たセンサーアプリ）

既存アプリのファイル構成を確認しながら、説明してください。
```

**ポイント**:
- まず仕組みを理解することを最優先
- 既存の動作例を明示的に指定
- メカニズムの説明を求める（表面的な手順だけでなく）

---

### プロンプト2: 既存アプリとの比較分析

```
sixaxisアプリを参考に、bmi160_orientationアプリの
ビルドシステム統合に必要なファイルを設計したいです。

【アプリケーション情報】
- アプリ名: bmi160_orientation
- 配置場所: sdk/apps/examples/bmi160_orientation/
- 依存: SENSORS_BMI160, EXTERNALS_AHRS
- デフォルト設定: priority=100, stacksize=4096

【依頼】
1. sixaxisのKconfig, Makefile, Make.defsを読み取って分析
2. bmi160_orientation用の各ファイルのテンプレートを作成
3. CONFIG変数名の命名規則に従っているか確認
4. 注意すべきポイントを指摘

特に、CONFIG変数名のプレフィックス（EXAMPLES_）について注意して確認してください。
```

**ポイント**:
- 既存アプリの分析から始める（成功パターンを学ぶ）
- アプリ情報を具体的に提供
- 命名規則への注意を明示的に依頼

---

### プロンプト3: .configファイルの構造理解（最重要）

```
アプリをビルドしても、NuttShellでコマンドとして実行できません。
ビルドログには "Register: bmi160_orientation" と表示されていますが、
builtin_list.hにエントリが見つかりません。

【現状】
- ビルド: 成功 (nuttx.spk 生成済み)
- ビルドログ: "Register: bmi160_orientation" 表示あり
- builtin_list.h: bmi160_orientationのエントリなし
- 実行結果: "command not found"

【確認したいこと】
1. .configファイルはどこにありますか？複数存在しますか？
2. ビルドシステムはどの.configを参照していますか？
3. CONFIG_EXAMPLES_BMI160_ORIENTATION は設定されていますか？
4. CONFIGURED_APPSにbmi160_orientationは含まれていますか？

【依頼】
まず、以下のコマンドで状況を調査してください：
- find コマンドで .config ファイルを全て検索
- make --dry-run -p で CONFIGURED_APPS の内容確認
- grep でCONFIG変数の設定状況確認

調査結果から、何が原因か特定してください。
```

**ポイント**:
- 問題の症状を具体的に記述
- 「複数の.configファイルが存在するか」を質問（これが鍵）
- 調査コマンドを提案するよう依頼
- 原因特定を明示的に求める

---

### プロンプト4: CONFIG変数の正しい設定

```
調査の結果、以下のことがわかりました：

【発見事項】
1. .configファイルが2つ存在
   - ~/Spr_ws/spresense/sdk/.config (295 bytes)
   - ~/Spr_ws/spresense/nuttx/.config (68KB)

2. ビルドシステムはnuttx/.configを参照している

3. nuttx/.configにCONFIG_EXAMPLES_BMI160_ORIENTATIONの設定がない

【質問】
1. なぜ2つの.configファイルが存在するのですか？
2. どちらに設定を追加すべきですか？
3. 追加する場所（行番号）はどこが適切ですか？
4. 追加すべき設定内容を正確に教えてください

【依頼】
nuttx/.configに追加すべきCONFIG変数を、
既存のEXAMPLES設定（例: SIXAXIS）の近くに配置する形で提案してください。
```

**ポイント**:
- 発見事項を整理して共有（Claude Codeの理解を助ける）
- 「なぜ」を問うことで深い理解を得る
- 具体的な追加場所を求める（既存設定の近く）

---

### プロンプト5: Make.defsの正しい記述

```
Make.defsファイルの記述に問題があるようです。

【現在のMake.defs】
```makefile
ifneq ($(CONFIG_EXAMPLES_BMI160_ORIENTATION),)
CONFIGURED_APPS += bmi160_orientation
endif
```

【確認したいこと】
1. このパス指定は正しいですか？
2. "bmi160_orientation" だけでいいのか、"examples/bmi160_orientation" とすべきか
3. sixaxisなど、動作している既存アプリのMake.defsと比較してください

【依頼】
既存の動作例と比較して、正しいMake.defsの記述を提案してください。
```

**ポイント**:
- 現在の記述を明示
- 既存アプリとの比較を依頼（成功パターンから学ぶ）
- パス指定の正確さを確認

---

### プロンプト6: Makefileの最適化

```
Makefileに不要な記述があるかもしれません。

【現在のMakefile（抜粋）】
```makefile
MODULE = $(CONFIG_EXAMPLES_BMI160_ORIENTATION)
PROGNAME  = $(CONFIG_EXAMPLES_BMI160_ORIENTATION_PROGNAME)
PRIORITY  = $(CONFIG_EXAMPLES_BMI160_ORIENTATION_PRIORITY)
STACKSIZE = $(CONFIG_EXAMPLES_BMI160_ORIENTATION_STACKSIZE)
```

【質問】
1. MODULE変数は必要ですか？
2. 現代のNuttX/Spresenseで推奨されるMakefileの書き方は？
3. sixaxisアプリのMakefileと比較して、違いを指摘してください

【依頼】
不要な記述を削除し、最適化されたMakefileを提案してください。
```

**ポイント**:
- 疑わしい部分を明示
- 現代の推奨方法を確認（古い情報に惑わされない）
- 既存アプリとの差分に着目

---

### プロンプト7: ビルド確認と検証

```
設定を修正したので、再ビルドして確認したいです。

【修正内容】
1. nuttx/.configにCONFIG_EXAMPLES_BMI160_ORIENTATION追加
2. Make.defsをexamples/bmi160_orientationに修正
3. MakefileからMODULE変数を削除

【確認したいこと】
1. ビルドログで "Register: bmi160_orientation" が表示されるか
2. builtin_list.hにエントリが生成されるか
3. CONFIGURED_APPSに正しく含まれているか

【依頼】
以下のコマンドで確認してください：
```bash
# クリーンビルド
make clean && make -j4

# Register確認
make 2>&1 | grep "Register:"

# builtin_list.h確認
grep -i "bmi160" apps/builtin/builtin_list.h

# CONFIGURED_APPS確認
make --dry-run -p 2>&1 | grep CONFIGURED_APPS | grep bmi160
```

各確認の結果を教えてください。全て成功すれば、登録完了です。
```

**ポイント**:
- 修正内容を要約（コンテキストを共有）
- 確認すべき項目を列挙
- 具体的な確認コマンドを記載
- 成功の判断基準を明示

---

## このフェーズで達成すべきゴール

✅ Kconfig, Makefile, Make.defsの正しい作成
✅ nuttx/.configへのCONFIG変数追加
✅ ビルドログに "Register: bmi160_orientation" 表示
✅ builtin_list.hにエントリ生成
✅ CONFIGURED_APPSに含まれることを確認

---

## トラブルシューティングのプロンプト

### ケース1: ビルドは成功するが登録されない

```
ビルドは成功しますが、アプリがNuttShellで実行できません。

【状況】
- ビルド: 成功
- nuttx.spk: 生成済み
- ビルドログ: "Register:" の表示なし
- builtin_list.h: エントリなし

【依頼】
以下を段階的に調査してください：
1. Make.defsが正しく読み込まれているか
2. CONFIG変数名が正しいか（プレフィックス確認）
3. .configファイルの設定が反映されているか
4. CONFIGURED_APPSに含まれているか

各調査結果を報告し、問題箇所を特定してください。
```

### ケース2: CONFIG変数名の誤り

```
CONFIG変数名を間違えていたかもしれません。

【現在の命名】
- CONFIG_MYPRO_BMI160_ORIENTATION

【質問】
1. この命名は正しいですか？
2. examplesディレクトリのアプリは特定のプレフィックスが必要ですか？
3. 既存のexamplesアプリのCONFIG変数名を確認してください

【依頼】
正しい命名規則を教え、全ての関連ファイルの修正箇所をリストアップしてください。
```

### ケース3: パス指定の誤り

```
Make.defsのパス指定が間違っているかもしれません。

【現在の記述】
CONFIGURED_APPS += bmi160_orientation

【質問】
1. このパスは正しいですか？
2. "examples/" プレフィックスは必要ですか？
3. 絶対パスと相対パスのどちらを使うべきですか？

【依頼】
sixaxis等の動作例を確認し、正しいパス指定方法を教えてください。
```

---

## デバッグのベストプラクティス

### 1. 既存アプリとの比較
```
「動作しているsixaxisアプリと、私のbmi160_orientationアプリの
以下のファイルをdiffで比較してください：
- Kconfig
- Makefile
- Make.defs

相違点を指摘し、問題がありそうな箇所を教えてください。」
```

### 2. ビルドシステムの内部状態確認
```
「ビルドシステムが実際に認識している設定を確認したいです。
以下のコマンドを実行して、結果を分析してください：

make --dry-run -p 2>&1 | grep -A5 CONFIGURED_APPS

この出力から、bmi160_orientationが正しく登録されているか確認してください。」
```

### 3. .configファイルの完全調査
```
「システム内の全ての.configファイルを探し、
CONFIG_EXAMPLES_BMI160_ORIENTATIONが設定されているか確認してください。

find ~/Spr_ws/spresense -name ".config" -exec grep -H "BMI160_ORIENTATION" {} \;

どのファイルに設定すべきか、明確に教えてください。」
```

---

## よくある間違いと対策

| 間違い | 症状 | 対策プロンプト |
|--------|------|----------------|
| CONFIG変数名の誤り | ビルド成功だが登録されない | 「CONFIG命名規則を確認し、既存アプリと比較してください」 |
| SDK側.configのみ設定 | CONFIGURED_APPSに含まれない | 「.configファイルを全て検索し、ビルドシステムが参照するファイルを特定してください」 |
| Make.defsのパス誤り | ビルド成功だが登録されない | 「Make.defsのパス指定を既存アプリと比較してください」 |
| MODULE変数の使用 | ビルドエラーまたは警告 | 「現代のNuttXで推奨されるMakefile記述を教えてください」 |

---

## 効果的なプロンプトのコツ

1. **症状を具体的に**: 「動かない」ではなく「ビルドは成功するが実行時にcommand not found」
2. **既存アプリを活用**: 「sixaxisと比較して」と必ず指定
3. **段階的デバッグ**: 一度に全てを疑わず、1つずつ確認
4. **ログを共有**: ビルドログ、エラーメッセージは省略せず全文
5. **発見を報告**: 「.configが2つありました」など、発見事項を共有

---

**作成日**: 2025-12-14
**最重要ポイント**: NuttX側.configへの設定が最も重要
**想定時間**: 2-4時間（トラブル解決含む）
