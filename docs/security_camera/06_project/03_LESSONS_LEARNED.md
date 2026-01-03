# Phase 1 で学んだ教訓

**作成日**: 2025-12-20
**対象**: Phase 1A + Phase 1B の実装過程で得られた重要な知見

---

## 最も重要な発見

### 🔴 TTY Raw モードの必要性

**問題**: USB CDC 経由でバイナリデータを受信すると破損する

**原因**: Linux の TTY デバイスはデフォルトで **canonical mode (cooked mode)** で動作

**影響**:
- バイナリデータ内の制御文字 (0x0A, 0x0D など) が変換される
- MJPEG プロトコルヘッダーが消失
- JPEG データが破損

**解決策**:
```bash
stty -F /dev/ttyACM0 raw -echo 115200
```

**教訓**: USB CDC でバイナリデータを扱う際は、**必ず raw モードに設定すること!**

---

## ハードウェア・設定関連

### 1. Spresense の USB 構成

**学んだこと**: Spresense には 2つの USB インターフェースがある

1. **CP2102** (Main Board USB)
   - 用途: コンソール、デバッグ、フラッシュ
   - デバイス: `/dev/ttyUSB0` (WSL2)
   - ドライバー: `cp210x`

2. **CXD5602 USB Device** (Extension Board USB)
   - 用途: アプリケーションデータ転送 (CDC ACM)
   - デバイス: `/dev/ttyACM0` (WSL2)
   - ドライバー: `cdc_acm`
   - **重要**: 拡張ボード必須!

**教訓**: データ転送には拡張ボードの USB コネクタを使用する。

---

### 2. NuttX USB CDC 設定

**必須設定**:
```
CONFIG_CXD56_USBDEV=y        # ← 最重要! CXD56 USB ハードウェア有効化
CONFIG_SYSTEM_CDCACM=y       # CDC ACM システムコマンド (sercon/serdis)
CONFIG_CDCACM=y              # CDC ACM ドライバー (既に有効の場合が多い)
```

**学んだこと**:
- `CONFIG_CXD56_USBDEV` がないと、汎用 USB ドライバーがあっても動作しない
- ハードウェア固有の設定を見落とさないこと

---

### 3. WSL2 + usbipd の構成

**必要なツール**:
- Windows 側: `usbipd-win`
- WSL2 側: `cdc-acm`, `cp210x` カーネルモジュール

**手順**:
```powershell
# Windows PowerShell
usbipd attach --wsl --busid 1-1   # CDC ACM
usbipd attach --wsl --busid 1-11  # CP2102
```

```bash
# WSL2
sudo modprobe cdc-acm
sudo modprobe cp210x
```

**学んだこと**: WSL2 では USB デバイスが自動認識されない。手動アタッチが必要。

---

## カメラ・V4L2 関連

### 4. V4L2 Buffer Type の選択

**誤った設定**: `V4L2_BUF_TYPE_STILL_CAPTURE`
- 用途: 単発撮影
- 問題: 連続キャプチャでタイムアウト

**正しい設定**: `V4L2_BUF_TYPE_VIDEO_CAPTURE`
- 用途: 連続ストリーミング
- 必須: `V4L2_BUF_MODE_RING` と組み合わせ

**教訓**: カメラの用途に応じて適切な buffer type を選択する。

---

### 5. Buffer Mode の選択

**誤った設定**: `V4L2_BUF_MODE_FIFO`
- 問題: 連続ストリームで使い勝手が悪い

**正しい設定**: `V4L2_BUF_MODE_RING`
- 利点: 古いフレームが自動的に上書きされる
- 適用: 連続ストリーミングに最適

---

### 6. JPEG Buffer Size の計算

**問題**: ドライバーが `sizeimage = 0` を返す

**原因**: ISX012 の JPEG は可変長で事前サイズ不明

**解決策**: 手動で妥当なサイズを設定
```c
if (fmt.fmt.pix.sizeimage == 0)
{
    fmt.fmt.pix.sizeimage = 65536;  /* 64 KB */
}
```

**実測値**: QVGA JPEG は 7-22 KB → 64 KB で十分

**教訓**: ドライバーの返り値を盲信せず、検証と対処を行う。

---

## メモリ管理

### 7. メモリ最適化

**初期設計**: 512 KB バッファ × 2 + 512 KB パケット = 1.5 MB
- 問題: Spresense のヒープサイズ超過

**最適化後**: 64 KB × 2 + 64 KB = 192 KB (-87%)
- 理由: 実際の JPEG サイズは 7-22 KB

**教訓**: 実測値に基づいてバッファサイズを決定する。過剰な確保は避ける。

---

### 8. DMA アライメント

**要件**: カメラバッファは 32 バイトアライメント必須

**実装**: `memalign(32, size)`

**教訓**: ハードウェア DMA を使用する際はアライメント要件を確認。

---

## プロトコル設計

### 9. エンディアン問題

**設定**: 同期ワード `0xCAFEBABE`

**実際のバイト列** (ARM リトルエンディアン):
```
BE BA FE CA
```

**教訓**: バイナリプロトコルではエンディアンを常に意識する。検索時も考慮。

---

### 10. プロトコルオーバーヘッド

**設計**:
- ヘッダー: 12 bytes (Sync + Seq + Size)
- CRC: 2 bytes
- 合計: 14 bytes/frame

**実測**: 14 bytes / 7,500 bytes average = **0.19% オーバーヘッド**

**教訓**: シンプルなプロトコルでも十分実用的。

---

## デバッグ・トラブルシューティング

### 11. ログ出力の重要性

**実装**: 各段階で詳細ログを出力
```c
LOG_INFO("USB sent: %zd bytes", total_written);
LOG_DEBUG("Packed frame: seq=%u, size=%u, crc=0x%04X", ...);
```

**効果**: 問題の早期発見・切り分けが容易

**教訓**: デバッグログは惜しまず追加する。パフォーマンス影響は後で考える。

---

### 12. 段階的テスト

**アプローチ**:
1. Echo テスト (`echo "TEST" > /dev/ttyACM0`)
2. 既知データパターン送信
3. プロトコルパケット送信
4. 実データ送信

**効果**: 問題箇所を正確に特定

**教訓**: 複雑なシステムは段階的にテストする。

---

### 13. エラーメッセージの解釈

**例**: `Error -17`
- errno: `EEXIST` または `EBUSY`
- 意味: デバイスが使用中

**解決**: `reboot` でリセット

**教訓**: エラーコードを errno.h で確認し、根本原因を理解する。

---

## ツール・環境

### 14. ビルドスクリプトの作成

**問題**: `nuttx.spk` が `nuttx/` ディレクトリに生成される

**解決**: `build.sh` で自動コピー
```bash
cp "${NUTTX_DIR}/nuttx.spk" "${SDK_DIR}/nuttx.spk"
```

**教訓**: 繰り返し作業は早期にスクリプト化する。

---

### 15. hexdump の活用

**基本コマンド**:
```bash
hexdump -C file.bin | head -30
hexdump -C file.bin | grep "be ba fe ca"
```

**教訓**: バイナリデータのデバッグには hexdump が必須。

---

## よくある落とし穴

### ❌ 落とし穴 1: TTY モードの未設定

**症状**: データが破損、同期ワードが見つからない
**原因**: canonical mode のまま
**解決**: `stty raw`

### ❌ 落とし穴 2: CONFIG_CXD56_USBDEV 未設定

**症状**: USB CDC デバイスが認識されない
**原因**: ハードウェア USB が無効
**解決**: Kconfig で有効化

### ❌ 落とし穴 3: 拡張ボードなし

**症状**: CDC ACM デバイスが見えない
**原因**: CXD5602 USB ピンが未接続
**解決**: 拡張ボード使用

### ❌ 落とし穴 4: バッファサイズ過大

**症状**: メモリ割り当て失敗
**原因**: ヒープサイズ超過
**解決**: 実測値ベースでサイズ削減

### ❌ 落とし穴 5: STILL_CAPTURE 使用

**症状**: タイムアウト連発
**原因**: 連続撮影非対応モード
**解決**: VIDEO_CAPTURE + RING mode

---

## ベストプラクティス

### ✅ 推奨事項

1. **USB CDC でバイナリデータ**: 必ず `stty raw` 設定
2. **V4L2 連続撮影**: `VIDEO_CAPTURE` + `RING` mode
3. **メモリ割り当て**: 実測値の 2-3 倍程度に抑える
4. **プロトコル設計**: シンプルさを優先、複雑化は後から
5. **デバッグ**: 段階的テスト、詳細ログ
6. **ドキュメント**: 成功手順を必ず記録

---

## Phase 2 への引き継ぎ事項

### PC 側実装で注意すべき点

1. **シリアルポート設定**
   - raw モード設定 (Rust の `serialport` crate でも必要)
   - ボーレート: 115200

2. **プロトコルパーサー**
   - エンディアン: リトルエンディアン
   - 同期ワード検索: `0xBE, 0xBA, 0xFE, 0xCA`
   - CRC-16-CCITT 検証

3. **JPEG デコード**
   - フレームサイズ: 7-22 KB (可変)
   - 解像度: 320×240

4. **パフォーマンス**
   - 30 fps @ ~7.5 KB/frame = ~224 KB/s
   - バッファリング推奨: 最低 2-3 フレーム分

---

## まとめ

### 最重要項目 (Top 3)

1. **TTY Raw モード** - バイナリデータ受信の必須条件
2. **CONFIG_CXD56_USBDEV** - Spresense USB の必須設定
3. **拡張ボード** - CDC ACM には必須ハードウェア

### 成功の鍵

- 段階的アプローチ
- 詳細なログとデバッグ
- 実測値に基づく設計
- ドキュメント化の徹底

---

**作成者**: Claude Code (Sonnet 4.5)
**最終更新**: 2025-12-20 20:00
