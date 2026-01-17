# Phase 9: 自動再接続機能仕様

## 概要

Phase 9では、TCP切断発生時の自動再接続機能を実装し、GS2200Mの制約（重複ACKによるFIN送信）に対応した継続運転を実現する。

## 背景

### 問題

**Phase 8 pcap分析で判明した問題:**
- GS2200MがパケットロスのRTO再送後にFIN送信
- 重複ACK 11回でエラーカウント蓄積 → 閾値超過 → 接続終了
- 48分間の連続運転で切断発生

**原因（25_GS2200M_TCP_STACK_ANALYSIS.mdより）:**
- GS2200MのTCPスタックはファームウェア内実装（変更不可）
- Fast Retransmit未実装（RFC 5681非準拠）
- 重複ACKをエラーとしてカウント

### 要件

1. **自動再接続**: 切断検出 → 自動で再接続
2. **透過的動作**: ユーザーへの影響最小化
3. **再接続制限**: 無限ループ防止
4. **状態表示**: 接続状態をGUIに表示

## 設計

### 1. Spresense側設計

#### 1.1 再接続シーケンス

```
┌────────────────────────────────────────────────────────────────┐
│ TCP Thread 動作フロー                                          │
├────────────────────────────────────────────────────────────────┤
│                                                                │
│  ┌──────────────┐                                              │
│  │ tcp_server   │                                              │
│  │ _init()      │  listen() → accept() 待機                    │
│  └──────┬───────┘                                              │
│         │                                                      │
│         ↓                                                      │
│  ┌──────────────┐                                              │
│  │ accept()     │  クライアント接続待ち                         │
│  └──────┬───────┘                                              │
│         │                                                      │
│         ↓ 接続                                                  │
│  ┌──────────────┐                                              │
│  │ 通常送信     │  tcp_server_send()                           │
│  │ ループ       │                                              │
│  └──────┬───────┘                                              │
│         │                                                      │
│         ↓ 切断検出 (ENOTCONN, ECONNRESET, EPIPE)               │
│  ┌──────────────┐                                              │
│  │ close()      │  client_fd をクローズ                        │
│  │ client_fd    │                                              │
│  └──────┬───────┘                                              │
│         │                                                      │
│         ↓ 再接続カウンタ確認                                    │
│  ┌──────────────┐                                              │
│  │ カウンタ<MAX │───→ accept() へ戻る（再接続待機）            │
│  └──────────────┘                                              │
│                                                                │
└────────────────────────────────────────────────────────────────┘
```

#### 1.2 tcp_server.h 追加定義

```c
/* Phase 9: 自動再接続設定 */
#define TCP_RECONNECT_MAX       10      /* 最大再接続回数 */
#define TCP_RECONNECT_WAIT_MS   1000    /* 再接続待機時間 (1秒) */

/* 接続状態 */
typedef enum {
    TCP_STATE_DISCONNECTED = 0,
    TCP_STATE_LISTENING,
    TCP_STATE_CONNECTED,
    TCP_STATE_RECONNECTING,
} tcp_connection_state_t;

/* tcp_server_t構造体に追加 */
typedef struct {
    int listen_fd;
    int client_fd;
    uint16_t port;
    /* Phase 9: 自動再接続 */
    tcp_connection_state_t state;       /* 接続状態 */
    int reconnect_count;                /* 再接続回数 */
    bool auto_reconnect_enabled;        /* 自動再接続有効/無効 */
} tcp_server_t;
```

#### 1.3 tcp_server.c 実装

```c
/**
 * Phase 9: 切断検出と再接続待機
 *
 * @return 0: 再接続成功, -1: 再接続上限到達
 */
int tcp_server_handle_disconnect(tcp_server_t *server)
{
    if (!server->auto_reconnect_enabled) {
        return -1;
    }

    if (server->reconnect_count >= TCP_RECONNECT_MAX) {
        _err("ERROR: Max reconnect attempts (%d) reached\n",
             TCP_RECONNECT_MAX);
        return -1;
    }

    /* クライアントソケットをクローズ */
    if (server->client_fd >= 0) {
        close(server->client_fd);
        server->client_fd = -1;
    }

    server->state = TCP_STATE_RECONNECTING;
    server->reconnect_count++;

    _info("TCP disconnected, waiting for reconnect (%d/%d)...\n",
          server->reconnect_count, TCP_RECONNECT_MAX);

    /* クールダウン待機 */
    usleep(TCP_RECONNECT_WAIT_MS * 1000);

    server->state = TCP_STATE_LISTENING;

    return 0;
}

/**
 * Phase 9: クライアント再接続待機
 */
int tcp_server_wait_reconnect(tcp_server_t *server)
{
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int optval = 1;

    _info("Waiting for client reconnection on port %d...\n", server->port);

    /* accept()でクライアント接続待ち */
    server->client_fd = accept(server->listen_fd,
                               (struct sockaddr *)&client_addr,
                               &addr_len);
    if (server->client_fd < 0) {
        _err("ERROR: accept() failed: %d\n", errno);
        return -errno;
    }

    /* TCP_NODELAY設定 */
    setsockopt(server->client_fd, IPPROTO_TCP, TCP_NODELAY,
               &optval, sizeof(optval));

    server->state = TCP_STATE_CONNECTED;
    _info("Client reconnected from %s:%d\n",
          inet_ntoa(client_addr.sin_addr),
          ntohs(client_addr.sin_port));

    /* 再接続成功 → カウンタリセットはしない (累積でカウント) */
    return 0;
}

/**
 * Phase 9: 送信時の自動再接続対応
 */
ssize_t tcp_server_send_with_reconnect(tcp_server_t *server,
                                       const void *data, size_t len)
{
    ssize_t ret;

retry:
    ret = tcp_server_send(server, data, len);

    if (ret < 0) {
        int err = errno;
        if (err == ENOTCONN || err == ECONNRESET || err == EPIPE) {
            /* 切断検出 → 再接続処理 */
            if (tcp_server_handle_disconnect(server) == 0) {
                if (tcp_server_wait_reconnect(server) == 0) {
                    /* 再接続成功 → 再送信は行わない (フレームは破棄) */
                    return -EAGAIN;  /* 次のフレームで再試行 */
                }
            }
        }
    }

    return ret;
}
```

#### 1.4 camera_threads.c 修正

```c
/* USB Thread (WiFiモード) */
static void *usb_thread_entry(void *arg)
{
    /* ... existing code ... */

    while (!g_stop_request) {
        /* フレーム取得 */
        ret = frame_queue_pull(&g_action_queue, &frame, ...);
        if (ret < 0) continue;

        /* Phase 9: 自動再接続対応送信 */
        ret = tcp_server_send_with_reconnect(&g_tcp_server,
                                             frame->data, frame->used);

        if (ret == -EAGAIN) {
            /* 再接続完了、次フレームで継続 */
            g_tcp_stats.reconnect_count++;
            continue;
        }

        if (ret < 0) {
            /* 再接続上限到達または致命的エラー */
            consecutive_errors++;
            if (consecutive_errors >= 10) {
                _err("Too many errors, stopping...\n");
                g_stop_request = true;
            }
        } else {
            consecutive_errors = 0;
        }

        /* ... existing code ... */
    }
}
```

### 2. PC側設計

#### 2.1 再接続シーケンス

```
┌────────────────────────────────────────────────────────────────┐
│ TCP Reader Thread 動作フロー                                    │
├────────────────────────────────────────────────────────────────┤
│                                                                │
│  ┌──────────────┐                                              │
│  │ TcpConnection│                                              │
│  │ ::new()      │  connect() → 接続確立                        │
│  └──────┬───────┘                                              │
│         │                                                      │
│         ↓                                                      │
│  ┌──────────────┐                                              │
│  │ read_packet()│  パケット読み込みループ                       │
│  │ ループ       │                                              │
│  └──────┬───────┘                                              │
│         │                                                      │
│         ↓ 切断検出 (UnexpectedEof, ConnectionReset)            │
│  ┌──────────────┐                                              │
│  │ reconnect()  │  接続再試行                                  │
│  │              │  - 最大10回                                   │
│  │              │  - 2秒間隔                                   │
│  └──────┬───────┘                                              │
│         │                                                      │
│         ↓ 成功                                                  │
│  ┌──────────────┐                                              │
│  │ sync再確立   │  find_initial_sync()                         │
│  └──────┬───────┘                                              │
│         │                                                      │
│         ↓                                                      │
│  ┌──────────────┐                                              │
│  │ read_packet()│  読み込み再開                                │
│  │ ループ再開   │                                              │
│  └──────────────┘                                              │
│                                                                │
└────────────────────────────────────────────────────────────────┘
```

#### 2.2 tcp_connection.rs 追加

```rust
/// Phase 9: 自動再接続設定
const RECONNECT_MAX_ATTEMPTS: u32 = 10;
const RECONNECT_WAIT_SECS: u64 = 2;

/// 接続状態
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum ConnectionState {
    Connected,
    Disconnected,
    Reconnecting,
}

impl TcpConnection {
    /// Phase 9: 再接続を試行
    ///
    /// # Returns
    /// 成功時はOk(()), 失敗時はエラー（上限到達等）
    pub fn reconnect(&mut self) -> io::Result<()> {
        for attempt in 1..=RECONNECT_MAX_ATTEMPTS {
            warn!("TCP disconnected, attempting reconnect ({}/{})",
                  attempt, RECONNECT_MAX_ATTEMPTS);

            // 待機
            std::thread::sleep(Duration::from_secs(RECONNECT_WAIT_SECS));

            // 接続試行
            match TcpStream::connect_timeout(
                &format!("{}:{}", self.host, self.port)
                    .to_socket_addrs()?
                    .next()
                    .ok_or_else(|| io::Error::new(
                        io::ErrorKind::InvalidInput,
                        "Invalid address"
                    ))?,
                Duration::from_secs(10)
            ) {
                Ok(stream) => {
                    // 設定適用
                    stream.set_read_timeout(Some(Duration::from_secs(30)))?;
                    stream.set_write_timeout(Some(Duration::from_secs(30)))?;
                    stream.set_nodelay(true)?;

                    self.stream = stream;
                    self.peer_addr = self.stream.peer_addr()?;

                    // sync状態リセット（再同期が必要）
                    self.sync_established = false;
                    self.internal_buffer.clear();
                    self.buffer_pos = 0;

                    info!("Reconnected to Spresense: {}", self.peer_addr);
                    return Ok(());
                }
                Err(e) => {
                    error!("Reconnect attempt {} failed: {}", attempt, e);
                }
            }
        }

        Err(io::Error::new(
            io::ErrorKind::ConnectionRefused,
            format!("Failed to reconnect after {} attempts", RECONNECT_MAX_ATTEMPTS)
        ))
    }

    /// Phase 9: 自動再接続対応パケット読み込み
    pub fn read_packet_with_reconnect(&mut self, buffer: &mut [u8]) -> io::Result<usize> {
        loop {
            match self.read_packet(buffer) {
                Ok(n) => return Ok(n),
                Err(e) => {
                    // 切断検出
                    if matches!(e.kind(),
                        io::ErrorKind::UnexpectedEof |
                        io::ErrorKind::ConnectionReset |
                        io::ErrorKind::ConnectionAborted |
                        io::ErrorKind::BrokenPipe
                    ) {
                        // 再接続試行
                        self.reconnect()?;
                        // 再接続成功 → ループ継続
                        continue;
                    }
                    return Err(e);
                }
            }
        }
    }
}
```

#### 2.3 GUI表示追加

```rust
/// Phase 9: 接続状態表示
impl CameraApp {
    fn render_connection_status(&self, ui: &mut egui::Ui) {
        let (status_text, color) = match self.connection_state {
            ConnectionState::Connected => ("🟢 Connected", egui::Color32::GREEN),
            ConnectionState::Disconnected => ("🔴 Disconnected", egui::Color32::RED),
            ConnectionState::Reconnecting => ("🟡 Reconnecting...", egui::Color32::YELLOW),
        };

        ui.horizontal(|ui| {
            ui.label("TCP Status:");
            ui.colored_label(color, status_text);
            if self.reconnect_count > 0 {
                ui.label(format!("(reconnects: {})", self.reconnect_count));
            }
        });
    }
}
```

### 3. Metricsパケット拡張

#### 3.1 再接続統計の追加

```c
/* mjpeg_protocol.h に追加 */
typedef struct __attribute__((packed)) {
    /* ... existing fields ... */

    /* Phase 9: 再接続統計 */
    uint8_t tcp_reconnect_count;        /* 再接続回数 */
    uint8_t tcp_connection_state;       /* 接続状態 */
} mjpeg_metrics_packet_t;
```

### 4. シーケンス図

```
┌─────────┐    ┌──────────┐    ┌─────────┐    ┌────────┐
│ Camera  │    │ Spresense│    │ GS2200M │    │   PC   │
└────┬────┘    └────┬─────┘    └────┬────┘    └────┬───┘
     │              │               │              │
     │   frame      │               │              │
     │─────────────>│               │              │
     │              │    send()     │              │
     │              │──────────────>│              │
     │              │               │    stream    │
     │              │               │─────────────>│
     │              │               │              │
     │              │  ┌────────────────────────┐  │
     │              │  │ パケットロス発生       │  │
     │              │  │ → 重複ACK → FIN送信   │  │
     │              │  └────────────────────────┘  │
     │              │               │              │
     │              │    FIN        │              │
     │              │<──────────────│              │
     │              │               │    EOF       │
     │              │               │─────────────>│
     │              │               │              │
     │              │   ENOTCONN    │              │
     │              │<──────────────│              │
     │              │               │              │
     │              │ ┌───────────────────────────┐│
     │              │ │ Phase 9: 再接続処理      ││
     │              │ │ - client_fd close        ││
     │              │ │ - accept() 待機          ││
     │              │ └───────────────────────────┘│
     │              │               │              │
     │              │               │   ┌─────────────────┐
     │              │               │   │ reconnect()     │
     │              │               │   │ - 2秒待機       │
     │              │               │   │ - connect()     │
     │              │               │   └─────────────────┘
     │              │               │              │
     │              │               │   connect    │
     │              │               │<─────────────│
     │              │   accept()    │              │
     │              │<──────────────│              │
     │              │               │              │
     │              │ ┌───────────────────────────┐│
     │              │ │ 通常動作再開             ││
     │              │ └───────────────────────────┘│
     │              │               │              │
     │   frame      │               │              │
     │─────────────>│               │              │
     │              │    send()     │              │
     │              │──────────────>│              │
     │              │               │    stream    │
     │              │               │─────────────>│
     │              │               │              │
```

## 実装計画

### Step 1: Spresense側実装

1. `tcp_server.h` に再接続関連定義追加
2. `tcp_server.c` に `tcp_server_handle_disconnect()` 実装
3. `tcp_server.c` に `tcp_server_wait_reconnect()` 実装
4. `tcp_server.c` に `tcp_server_send_with_reconnect()` 実装
5. `camera_threads.c` でUSB Threadを修正

### Step 2: PC側実装

1. `tcp_connection.rs` に `reconnect()` メソッド追加
2. `tcp_connection.rs` に `read_packet_with_reconnect()` 追加
3. `pipeline.rs` でTCP Readerスレッドを修正
4. `gui_main.rs` で接続状態表示追加

### Step 3: 統合テスト

1. 手動切断テスト（PCアプリ終了→再起動）
2. 長時間運転テスト（1時間以上）
3. 再接続回数の統計確認

## 期待される効果

| 項目 | Before (Phase 8) | After (Phase 9) |
|------|------------------|-----------------|
| 切断時の動作 | 完全停止 | 自動再接続 |
| 連続運転 | ~48分 (切断で終了) | 無制限 (再接続で継続) |
| ユーザー操作 | 手動再起動必要 | 自動復旧 |
| 切断影響 | 数秒～手動復旧 | ~3秒の映像停止 |

## 既知の制約

1. **再接続中の映像停止**: 再接続に2-3秒かかる
2. **フレームロス**: 再接続中のフレームは破棄
3. **再接続上限**: 10回の累積再接続でシャットダウン

## リスクと対策

| リスク | 対策 |
|--------|------|
| 無限再接続ループ | 上限10回で停止 |
| accept()ブロック | シャットダウン要求でタイムアウト設定 |
| メモリリーク | ソケット確実にクローズ |

---

# Phase 9.1: バックオフ付き再接続

## 背景

### Phase 9テスト結果で発見された新たな問題

**27_PHASE9_RECONNECT_FAILURE_ANALYSIS.mdより:**

2.5時間のテスト中、以下のパターンを発見:

1. **正常な再接続** (12:28-14:54): 79分、52分の安定運転 → 自動回復成功
2. **連続短命接続** (14:54-14:55): 4回の接続が4-21秒で切断
3. **RST拒否状態** (14:55:21-): 全SYNがRSTで拒否、再接続不可能

**根本原因:**
- GS2200Mが短時間（約1分）に複数回の接続/切断サイクルを経験
- ソケットリソース枯渇状態に陥る
- 新規接続を全てRSTで拒否
- Phase 9の1秒ウェイトでは不十分

### pcapエビデンス

```
時刻          イベント           結果
14:54:28      接続#4 確立       10秒で切断
14:54:40      接続#5 確立       4秒で切断
14:54:46      接続#6 確立       10秒で切断
14:54:58      接続#7 確立       21秒で切断
14:55:21-52   SYN x40回        全てRST拒否
```

## Phase 9.1 設計

### 目的

GS2200Mのソケットリソース解放時間を確保し、RST拒否状態を防止する。

### 変更内容

#### 1. Spresense側 (tcp_server.h)

```c
/* Phase 9.1: バックオフ付き再接続設定 */
#define TCP_RECONNECT_MAX         10    /* 最大再接続回数 */
#define TCP_RECONNECT_WAIT_MS   5000    /* 基本待機時間 (5秒) */
#define TCP_RECONNECT_BACKOFF_MS 2000   /* 追加待機時間/試行 (2秒) */
```

#### 2. Spresense側 (tcp_server.c)

```c
int tcp_server_handle_disconnect(tcp_server_t *server)
{
    /* ... existing code ... */

    server->state = TCP_STATE_RECONNECTING;
    server->reconnect_count++;

    /* Phase 9.1: バックオフ付き待機
     * wait_ms = base + (attempt - 1) * backoff
     */
    uint32_t wait_ms = TCP_RECONNECT_WAIT_MS +
                       (server->reconnect_count - 1) * TCP_RECONNECT_BACKOFF_MS;

    _info("TCP disconnected, waiting %lu ms before reconnect (%d/%d)...\n",
          (unsigned long)wait_ms, server->reconnect_count, TCP_RECONNECT_MAX);

    /* クールダウン待機（バックオフ付き） */
    usleep(wait_ms * 1000);

    server->state = TCP_STATE_LISTENING;
    return 0;
}
```

#### 3. PC側 (tcp_connection.rs)

```rust
/// Phase 9.1: バックオフ付き再接続設定
const RECONNECT_MAX_ATTEMPTS: u32 = 10;
const RECONNECT_BASE_WAIT_SECS: u64 = 5;    // 基本待機時間
const RECONNECT_BACKOFF_SECS: u64 = 2;      // 試行ごとの追加待機時間

pub fn reconnect(&mut self) -> io::Result<()> {
    for attempt in 1..=RECONNECT_MAX_ATTEMPTS {
        self.state = ConnectionState::Reconnecting;

        // Phase 9.1: バックオフ付き待機
        let wait_secs = RECONNECT_BASE_WAIT_SECS +
                        (attempt as u64 - 1) * RECONNECT_BACKOFF_SECS;

        warn!("TCP disconnected, waiting {} secs before reconnect ({}/{})",
              wait_secs, attempt, RECONNECT_MAX_ATTEMPTS);

        // 待機（GS2200Mのリソース解放を待つ）
        thread::sleep(Duration::from_secs(wait_secs));

        // ... connection attempt ...
    }
}
```

### バックオフスケジュール

| 試行# | Spresense待機 | PC待機 | 備考 |
|-------|---------------|--------|------|
| 1 | 5秒 | 5秒 | 基本待機 |
| 2 | 7秒 | 7秒 | +2秒 |
| 3 | 9秒 | 9秒 | +4秒 |
| 4 | 11秒 | 11秒 | +6秒 |
| 5 | 13秒 | 13秒 | +8秒 |
| 6 | 15秒 | 15秒 | +10秒 |
| 7 | 17秒 | 17秒 | +12秒 |
| 8 | 19秒 | 19秒 | +14秒 |
| 9 | 21秒 | 21秒 | +16秒 |
| 10 | 23秒 | 23秒 | +18秒（最終） |

**合計最大待機時間:** 140秒（約2.3分）

### 期待される効果

| 項目 | Phase 9 | Phase 9.1 |
|------|---------|-----------|
| 基本待機時間 | 1秒 | 5秒 |
| バックオフ | なし | +2秒/試行 |
| RST拒否対策 | なし | リソース解放待機 |
| 連続切断耐性 | 低 | 高 |

### シーケンス図

```
┌─────────┐    ┌──────────┐    ┌─────────┐    ┌────────┐
│ Camera  │    │ Spresense│    │ GS2200M │    │   PC   │
└────┬────┘    └────┬─────┘    └────┬────┘    └────┬───┘
     │              │               │              │
     │              │  ┌────────────────────────┐  │
     │              │  │ 切断検出               │  │
     │              │  └────────────────────────┘  │
     │              │               │              │
     │              │ ┌───────────────────────────┐│
     │              │ │ Phase 9.1: バックオフ待機 ││
     │              │ │ 1回目: 5秒               ││
     │              │ │ 2回目: 7秒               ││
     │              │ │ 3回目: 9秒               ││
     │              │ │ ...                       ││
     │              │ └───────────────────────────┘│
     │              │               │              │
     │              │               │   ┌─────────────────┐
     │              │               │   │ 同期待機        │
     │              │               │   │ (5秒 + backoff) │
     │              │               │   └─────────────────┘
     │              │               │              │
     │              │   accept()    │   connect    │
     │              │<──────────────│<─────────────│
     │              │               │              │
     │              │ ┌───────────────────────────┐│
     │              │ │ GS2200Mリソース回復済み   ││
     │              │ │ → 接続成功               ││
     │              │ └───────────────────────────┘│
     │              │               │              │
```

---

**Document Version**: 1.1
**Last Updated**: 2026-01-17
**Author**: Claude Opus 4.5
**Status**: IMPLEMENTED
