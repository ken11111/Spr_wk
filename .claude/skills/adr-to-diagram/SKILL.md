---
name: adr-to-diagram
description: ADR (Architecture Decision Record) markdown ファイルを読み取り、PlantUML アーキテクチャ図を生成する。design skill ではなく PlantUML を直接出力する。引数で対象 ADR ファイルパスを受け取る。
---

# ADR → PlantUML Diagram

ADR markdown を解析して PlantUML 図を生成するスキル。

## 入力

- 引数: ADR ファイルのパス (例: `docs/security_camera/03_achievements/architecture_decisions/system_architecture/ADR_005_*.md`)
- 引数省略時: 現在 IDE で開いている ADR、または `docs/security_camera/03_achievements/architecture_decisions/` 配下を一覧してユーザーに確認

## 手順

### 1. プラン提示 (書き込み前に必ず)

- 対象 ADR を Read で読み取り、以下を箇条書きで提示してユーザーの「go」を待つ:
  - 抽出したコンポーネント一覧 (例: Camera Manager, Frame Queue, Encoder Thread)
  - 関係性 (依存・呼び出し・データフロー)
  - 採用する図種 (C4 Context / Container / Component / Sequence / State のいずれか)
  - 出力ファイルパス (デフォルト: ADR と同じディレクトリ、`<adr-stem>.puml`)
- ユーザーが承認するまで Write しない。

### 2. PlantUML ソース生成

- プロジェクトの既存ライブラリを優先的に活用:
  - `docs/plantuml-libs/C4.puml`, `C4_Component.puml`, `C4_Container.puml`, `C4_Context.puml`
  - 相対 include で参照: `!include ../../../plantuml-libs/C4_Container.puml` など
- スタイル方針:
  - C4 モデル (Context / Container / Component) を第一候補
  - シーケンスや状態遷移が主旨の ADR は標準 PlantUML
  - `skinparam` で日本語フォント (`defaultFontName "Noto Sans CJK JP"`) を指定
- ADR の本文・図表・「Decision」「Consequences」セクションから関係性を抽出

### 3. 出力と検証

- `.puml` ファイルを Write
- 可能なら `plantuml -tpng <file>.puml` でレンダリング検証 (失敗時はエラーをユーザーに報告)
- 生成後、PNG 出力先と簡単な構成サマリ (要素数・関係数) を報告

## 禁止事項

- design skill (UX/UI 系) は呼び出さない
- 既存の `.puml` を上書きする場合は事前に diff を提示して確認
- ADR の内容を勝手に解釈拡張しない (本文に書かれている範囲で図示)

## 既存リソース

- `docs/plantuml-libs/` - C4 / FontAwesome ライブラリ
- `docs/security_camera/02_specifications/architecture/phase11_*.puml` - 参考スタイル
- `docs/security_camera/02_specifications/architecture/MODELING_LIBRARIES_REFERENCE.md` - 利用方針
