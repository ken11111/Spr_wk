# GS2200M TCPスタック実装分析と対策

**Date**: 2026-01-16
**関連**: 24_PHASE8_PCAP_DISCONNECT_ANALYSIS.md, 23_PHASE8_BUFFER_QUEUE_ANALYSIS.md

---

## 1. GS2200Mアーキテクチャ

### 1.1 構成

```
┌─────────────────────────────────────────────────────────────────┐
│ Spresense Application                                           │
│ (security_camera)                                               │
└─────────────────────────────────┬───────────────────────────────┘
                                  │ socket API
┌─────────────────────────────────┴───────────────────────────────┐
│ NuttX usrsock                                                   │
│ (apps/wireless/gs2200m/gs2200m_main.c)                         │
└─────────────────────────────────┬───────────────────────────────┘
                                  │ ioctl / ATコマンド
┌─────────────────────────────────┴───────────────────────────────┐
│ NuttX GS2200M Driver                                            │
│ (drivers/wireless/gs2200m.c)                                   │
└─────────────────────────────────┬───────────────────────────────┘
                                  │ SPI通信
┌─────────────────────────────────┴───────────────────────────────┐
│ GS2200M WiFi Module (Hardware)                                  │
│ ┌─────────────────────────────────────────────────────────────┐ │
│ │ Firmware (Telit/GainSpan)                                   │ │
│ │ ┌───────────────────────────────────────────────────────┐   │ │
│ │ │ TCP/IP Stack (モジュール内部実装)                      │   │ │
│ │ │ - TCP再送処理                                          │   │ │
│ │ │ - ACK処理                                              │   │ │
│ │ │ - エラー検出・回復                                     │   │ │
│ │ │ ※ ソースコード非公開、変更不可                        │   │ │
│ │ └───────────────────────────────────────────────────────┘   │ │
│ └─────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

### 1.2 主要ファイル

| ファイル | 役割 |
|---------|------|
| `apps/wireless/gs2200m/gs2200m_main.c` | usrsockデーモン（ユーザ空間） |
| `drivers/wireless/gs2200m.c` | カーネルドライバ（ATコマンド送受信） |
| GS2200Mファームウェア | TCP/IPスタック実装（**変更不可**） |

### 1.3 ATコマンドインターフェース

GS2200Mは以下のATコマンドで制御される:

```
AT+NCTCP=<ip>,<port>    # TCP接続
AT+NSTCP=<port>         # TCPサーバ開始
AT+NCLOSE=<cid>         # 接続クローズ
AT+NCLOSEALL            # 全接続クローズ
```

**重要**: TCPの再送、重複ACK処理、FIN送信はすべてモジュール内部のファームウェアで実行される。
ホスト（Spresense）からは制御できない。

---

## 2. pcap分析から判明した問題

### 2.1 GS2200M TCPスタックの問題点

| 問題 | 期待される動作 | 実際の動作 |
|------|--------------|----------|
| Fast Retransmit | 3回重複ACKで即再送 | 1.5秒RTOまで待機 |
| 重複ACK処理 | 正常応答として処理 | エラーとしてカウント? |
| 再送後の動作 | 通信継続 | FIN送信して切断 |

### 2.2 ファームウェアの制約

```
GS2200Mファームウェアの特性（推定）:

1. 軽量TCP/IPスタック
   - IoTデバイス向けに最適化
   - 高スループット通信は想定外

2. エラー耐性の低さ
   - 重複ACKをエラーとしてカウント
   - 閾値超過で接続を強制終了

3. Fast Retransmit未実装
   - RFC 5681非準拠
   - RTOベースの再送のみ
```

---

## 3. 対策検討

### 3.1 対策の分類

| レベル | 対策 | 実現性 | 効果 |
|--------|------|--------|------|
| ファームウェア | GS2200Mファームウェア更新 | △ 困難 | ◎ 根本解決 |
| ドライバ | NuttXドライバ修正 | × 不可 | - |
| アプリケーション | 自動再接続機能 | ◎ 可能 | ○ 回避策 |
| アプリケーション | フレームレート制限 | ◎ 可能 | ○ 発生抑制 |
| ハードウェア | WiFiモジュール変更 | △ 中程度 | ◎ 根本解決 |

### 3.2 対策詳細

#### 対策A: ファームウェア更新（困難）

```
問題点:
- GS2200Mはレガシー製品（Telit買収前のGainSpan製品）
- ファームウェアソースは非公開
- カスタムファームウェアの提供は期待できない

確認方法:
- Telit技術サポートに問い合わせ
- 現在のファームウェアバージョン確認: AT+VER=??
```

#### 対策B: 自動再接続機能（推奨、Phase 9候補）

```c
// tcp_server.c への追加案

static int auto_reconnect_count = 0;
#define MAX_AUTO_RECONNECT 10

int tcp_send_with_reconnect(const void *data, size_t len)
{
    int ret = tcp_server_send(data, len);

    if (ret < 0 && (errno == ENOTCONN || errno == ECONNRESET)) {
        // 切断検出 → 自動再接続
        LOG_WARN("TCP disconnected, attempting reconnect (%d/%d)",
                 auto_reconnect_count + 1, MAX_AUTO_RECONNECT);

        if (auto_reconnect_count < MAX_AUTO_RECONNECT) {
            tcp_server_close();
            sleep(1);  // クールダウン
            ret = tcp_server_init();
            if (ret == 0) {
                auto_reconnect_count++;
                // 再送信を試行
                ret = tcp_server_send(data, len);
            }
        }
    } else if (ret >= 0) {
        // 成功時はカウントリセット
        auto_reconnect_count = 0;
    }

    return ret;
}
```

**メリット:**
- ソフトウェアのみで実装可能
- 既存コードへの影響が小さい
- ユーザー体験の改善

**デメリット:**
- 切断は依然として発生
- 再接続中の数秒間は映像が止まる

#### 対策C: フレームレート制限（発生抑制）

```c
// camera_threads.c への追加案

#define ADAPTIVE_FPS_ENABLED 1
#define MIN_FPS 5
#define MAX_FPS 15

static int adaptive_fps = MAX_FPS;
static int consecutive_errors = 0;

void adjust_fps_on_error(void)
{
    consecutive_errors++;
    if (consecutive_errors >= 3 && adaptive_fps > MIN_FPS) {
        adaptive_fps--;
        LOG_WARN("Reducing FPS to %d due to errors", adaptive_fps);
    }
}

void adjust_fps_on_success(void)
{
    if (consecutive_errors > 0) {
        consecutive_errors--;
    }
    // 一定時間成功が続いたらFPSを上げる
}
```

**メリット:**
- パケットロス発生確率を低下
- GS2200Mの負荷を軽減

**デメリット:**
- FPSが低下
- 完全な解決策ではない

#### 対策D: WiFiモジュール変更（根本解決）

```
代替モジュール候補:

1. ESP32
   - lwIPベースのTCP/IPスタック
   - Fast Retransmit対応
   - 高い実績と安定性
   - 開発者向けドキュメント充実

2. ESP32-C3/S3
   - RISC-Vベース（C3）/ Xtensaデュアルコア（S3）
   - BLE 5.0対応
   - より新しいSDK

3. 802.11n/ac対応モジュール
   - 高帯域（150Mbps以上）
   - 5GHz帯対応で干渉低減

変更に必要な作業:
- ハードウェア設計変更
- ドライバ開発/移植
- プロトコル互換性確認
```

---

## 4. 検証方法

### 4.1 GS2200Mファームウェアバージョン確認

```c
// ATコマンドで確認
AT+VER=??

// 応答例
APP VERSION= xx.xx.xx
WLAN VERSION= xx.xx.xx
```

### 4.2 TCP設定パラメータ確認

```c
// 現在の接続状態確認
AT+NSTAT=?

// CID（接続ID）確認
AT+CID=?
```

### 4.3 pcapによる継続監視

```bash
# 次回テスト時にpcapを取得
tcpdump -i <interface> -w capture.pcap host 192.168.137.106

# 重複ACK回数の集計
tcpdump -r capture.pcap | grep "ack" | sort | uniq -c | sort -rn
```

---

## 5. 推奨アクションプラン

### 5.1 短期（1-2週間）

1. **自動再接続機能の実装** (Phase 9)
   - tcp_server.cに再接続ロジック追加
   - PC側GUIにも再接続機能追加
   - テスト実施

2. **フレームレート制限の検討**
   - 現在30fps → 10-15fpsへ制限
   - パケットロス発生頻度を確認

### 5.2 中期（1-2ヶ月）

1. **Telit技術サポートへの問い合わせ**
   - ファームウェア更新の可否
   - TCP設定パラメータの有無
   - 既知の問題の確認

2. **代替モジュールの評価**
   - ESP32での概念実証
   - 性能比較テスト

### 5.3 長期（3ヶ月以上）

1. **WiFiモジュールの変更**（必要に応じて）
   - ESP32への移行
   - ハードウェア・ソフトウェア両面での対応

---

## 6. 結論

### 6.1 調査結果

GS2200MのTCPスタックはモジュール内部のファームウェアに実装されており、
ホスト（Spresense/NuttX）からは直接制御できない。

pcap分析で判明した問題（Fast Retransmit未実装、重複ACKのエラーカウント）は
ファームウェアレベルの問題であり、ソフトウェアでの根本解決は困難。

### 6.2 推奨対策

| 優先度 | 対策 | 理由 |
|--------|------|------|
| 1 | 自動再接続機能 | 実装容易、効果高い |
| 2 | フレームレート制限 | 発生抑制に有効 |
| 3 | ESP32への移行 | 根本解決だが工数大 |

### 6.3 Phase 9提案

Phase 9として「自動再接続機能 + 適応型フレームレート」の実装を提案する。

---

**Document Version**: 1.0
**Last Updated**: 2026-01-16
**Author**: Claude Opus 4.5
**Status**: COMPLETED
