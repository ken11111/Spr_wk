# ADR 004: CRC Lookup Table Optimization for High-Performance Packet Processing

**作成日**: 2026-02-10
**バージョン**: 1.0
**ステータス**: 受諾済み
**対象システム**: Spresenseセキュリティカメラ Phase 1.5
**技術影響度**: 高

## 1. 決定概要

### 背景
Phase 1.5 VGAモード（640×480）ストリーミングにおいて、パケット処理時間が23.3msと総レイテンシの39.8%を占める深刻なボトルネックが発生していた。プロファイリングの結果、MJPEGプロトコルのCRC-16-CCITT計算（ビットバイビット方式）が15-18msを消費し、30fpsストリーミング目標達成の阻害要因となっていた。

### 決定内容
**テーブルルックアップ方式によるCRC-16-CCITT計算の最適化**を実装し、PC側との互換性を維持したまま大幅な性能向上を実現する。

**実装方針**:
- 256エントリ（512バイト）のCRCテーブル事前計算
- MSB-first、多項式0x1021、初期値0xFFFFの完全互換性維持
- ビットバイビット演算からO(n)テーブル参照への変更

### 影響範囲
- MJPEGプロトコル全体のパケット処理性能
- リアルタイムストリーミングのFPS向上
- システム全体のレイテンシ削減
- Phase 1.5以降の全バイナリプロトコル

## 2. 技術的根拠

### 課題分析
**ボトルネック詳細分析**:
```
総レイテンシ内訳（最適化前 58.6ms）:
├─ カメラ遅延:     5.1ms   (8.7%)
├─ JPEG圧縮:       0.05ms  (0.1%)
├─ パケット処理:   23.3ms  (39.8%) ← 🔴 最大ボトルネック
│  └─ CRC計算:    ~15-18ms (推定75-80%)
└─ USB書き込み:    30.2ms  (51.5%)
```

**根本原因**: 64KBフレームに対するCRC計算の計算量過多
- 計算量: 64KB × 8 bits × 8 iterations = 4,194,304 ビット演算
- 処理時間: 15-18ms（Cortex-M4F @ 156MHz）
- CPUサイクル: 約2.3M-2.8Mサイクル

### 代替案検討

| 選択肢 | メリット | デメリット | 採用理由 |
|-------|---------|-----------|----------|
| **テーブルルックアップ方式** | ・77%性能改善実証<br>・完全互換性維持<br>・実装コスト低 | ・512バイトメモリ使用<br>・初期化オーバーヘッド | ✅ **採用**：劇的効果と互換性 |
| ハードウェアCRC使用 | ・最高性能 | ・Spresense CRC32のみ対応<br>・CRC16非対応<br>・実装複雑 | ❌ ハードウェア制約 |
| SIMD最適化 | ・並列処理効果 | ・Cortex-M4F SIMD限定的<br>・実装複雑度高 | ❌ 効果対実装比不良 |
| CRC省略・簡易チェック | ・CPU負荷ゼロ | ・データ完全性喪失<br>・プロトコル信頼性低下 | ❌ 機能品質劣化 |
| 外部CRCプロセッサ使用 | ・CPU負荷軽減 | ・ハードウェア追加<br>・複雑性増大 | ❌ 過剰設計 |

### 選択理由
1. **実測効果**: パケット処理時間23.3ms → 8.7ms（62.7%削減）
2. **FPS向上**: 9.8fps → 11.0fps（12.2%向上）
3. **完全互換性**: PC側コード変更不要、同一CRC値生成保証
4. **実装シンプル**: 既存アーキテクチャへの低侵襲統合

## 3. 実装詳細

### 技術仕様
**CRCテーブル実装**:
```c
/* 256エントリのCRC16テーブル（512バイト） */
static const uint16_t crc16_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, /* ... */
    /* MSB-first, 多項式0x1021で事前計算 */
};

/* 最適化後のCRC計算（O(n)） */
uint16_t mjpeg_crc16_ccitt_optimized(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    size_t i;

    for (i = 0; i < len; i++) {
        uint8_t index = (uint8_t)((crc >> 8) ^ data[i]);
        crc = (crc << 8) ^ crc16_table[index];  // ← 1回のテーブル参照
    }
    return crc;
}
```

**従来方式との比較**:
```c
/* 従来方式（ビットバイビット、O(8n)） */
uint16_t mjpeg_crc16_ccitt_bitwise(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    size_t i;
    int j;

    for (i = 0; i < len; i++) {
        crc ^= (data[i] << 8);
        for (j = 0; j < 8; j++) {          // ← 8回の条件分岐
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc;
}
```

### 互換性保証策
**CRC-16-CCITT標準パラメータ完全準拠**:
| パラメータ | 値 | 目的 |
|-----------|-----|------|
| 多項式 | 0x1021 | CRC-16-CCITT標準 |
| 初期値 | 0xFFFF | MSB-first方式 |
| アルゴリズム | MSB-first（非反射） | ビッグエンディアン |
| 最終XOR | なし（0x0000） | 標準仕様準拠 |

**重要**: NuttXの`crc16ccitt()`は初期値0x0000・LSB-firstのため使用不可

### 性能最適化要因
1. **計算量削減**:
   - ループ回数: 524,288回 → 65,536回（1/8削減）
   - 条件分岐: 524,288回 → 0回（完全排除）

2. **CPU効率向上**:
   - 分岐予測ミス排除（パイプラインストール回避）
   - L1キャッシュヒット率向上（512バイトテーブル）
   - メモリアクセスパターンの規則性

## 4. 検証結果

### テスト結果
**Phase 1.5性能測定（VGA 90フレーム）**:
- **テスト条件**: カメラを壁に向けて固定（JPEG圧縮負荷最小化）
- **平均JPEGサイズ**: 40.76KB（36-42KB範囲）
- **測定環境**: 制御されたシーン（変動係数3.7%）

**全体性能改善**:
| 指標 | 最適化前 | 最適化後 | 改善率 |
|------|---------|---------|--------|
| **FPS** | 9.8 fps | 11.0 fps | **+12.2%** |
| **パケット処理時間** | 23.3 ms | 8.7 ms | **-62.7%** |
| **総レイテンシ** | 58.6 ms | 47.9 ms | **-18.3%** |
| **フレーム間隔** | 102 ms | 91 ms | **-10.8%** |

### 測定データ（詳細）
**処理時間安定性**（90フレーム統計）:
| 指標 | 最小値 | 最大値 | 平均値 | 標準偏差 |
|------|--------|--------|--------|----------|
| パケット処理 | 8.6 ms | 8.8 ms | 8.7 ms | ±0.1 ms |
| FPS | 10.88 fps | 11.00 fps | 10.95 fps | ±0.05 fps |
| 総レイテンシ | 47.8 ms | 48.1 ms | 47.9 ms | ±0.15 ms |

**ボトルネックの変化**:
```
最適化前: USB書き込み(51.5%) > パケット処理(39.8%) > カメラ(8.7%)
最適化後: USB書き込み(63.0%) >> パケット処理(18.2%) > カメラ(10.6%)

→ 次の最適化ターゲット: USB書き込み（30.2ms）
```

## 5. 運用考慮事項

### 適用手順
**システム起動時の初期化**:
1. CRC16テーブルの事前計算（起動時1回のみ）
2. 既存CRC関数の置き換え確認
3. PC側受信での互換性検証

**性能確認コード**:
```c
/* CRC最適化効果の確認 */
uint64_t start_time = get_timestamp_us();
uint16_t crc = mjpeg_crc16_ccitt_optimized(jpeg_data, jpeg_size);
uint64_t crc_time = get_timestamp_us() - start_time;

LOG_INFO("CRC calc: %llu us for %zu bytes (%.2f us/KB)",
         crc_time, jpeg_size, (double)crc_time * 1024 / jpeg_size);
```

### 注意点
- **メモリ使用量**: 512バイト追加（全体192KBから0.27%増加）
- **初期化オーバーヘッド**: 起動時のテーブル生成で数ms追加
- **NuttX crc16ccitt()非互換**: 標準関数は使用不可（パラメータ差異）
- **テーブル配置**: const指定でROM配置、RAM節約

### トラブルシューティング
**よくある問題と対処法**:

1. **CRC値が一致しない**
   ```
   原因：MSB/LSB-firstの混在、初期値差異
   対処：PC側とパラメータ完全一致確認
   ```

2. **性能向上が見られない**
   ```
   原因：テーブルがRAMに配置、キャッシュミス
   対処：const修飾子による ROM配置確認
   ```

3. **メモリ不足エラー**
   ```
   原因：512バイトテーブルによるメモリ圧迫
   対処：他のバッファサイズ調整
   ```

## 6. 関連文書

### 証跡文書
- `/home/ken/Spr_ws/bak/04_test_results/11_CRC最適化性能分析.md` - 77%性能改善詳細分析
- `/home/ken/Spr_ws/bak/01_specifications/03_PROTOCOL_SPEC.md` - MJPEGプロトコル仕様
- `/home/ken/Spr_ws/bak/04_test_results/diagrams/crc_optimization_comparison.puml` - 性能比較図表

### 関連ADR
- ADR-001: TTY Raw Mode Requirement（基盤プロトコル通信）
- ADR-002: TCP Health Monitoring（58バイトメトリクスパケット拡張）
- ADR-005: Three-Thread Pipeline Architecture（パケット処理パイプライン）

### 関連仕様書
- `/02_specifications/interface/protocol/MJPEG_PROTOCOL_SPEC.md` - プロトコル詳細
- `/02_specifications/performance/OPTIMIZATION_TARGETS.md` - 性能目標設定

### 実装ファイル
- `mjpeg_protocol.c` - CRC計算関数の最適化実装
- `mjpeg_protocol.h` - インターフェース（互換性維持）

## 7. 改訂履歴

| バージョン | 日付 | 変更内容 |
|-----------|------|---------|
| 1.0 | 2026-02-10 | 初版作成：Phase 1.5 CRC最適化実装成果をADR文書化 |

---

**作成者**: Claude Code Architecture Analyst
**承認者**: Phase 1.5性能最適化チーム
**関連Phase**: Phase 1.5（VGA性能最適化）
**技術分類**: Performance Optimization / Protocol Processing / Algorithmic Optimization