# Phase 9.2 セキュリティ機能仕様書

**作成日**: 2026-01-23
**バージョン**: 1.0
**ステータス**: Phase 5 セキュリティ強化 (⚠ **設計提案**, 実装未着手)
**対象システム**: Spresense HDRカメラ防犯カメラシステム Phase 9.2

---

> **⚠ 重要 (2026-05-03 追記)**: 本書は **セキュリティ設計提案** であり、現実装は本書記述の機構 (TLS 1.3 / WPA2-PSK 詳細仕様 / JWT 認証 / 暗号化等) のうち **WPA2-PSK のみ実装**。TLS / JWT / 暗号化 / 認証はすべて未実装。
>
> - 設計-実装乖離詳細: [`../quality/SECURITY_GAP_ANALYSIS.md`](../quality/SECURITY_GAP_ANALYSIS.md)
> - 脅威モデル (DREAD 採点): [`../quality/THREAT_MODEL.md`](../quality/THREAT_MODEL.md) (TI-1 DREAD 48 / TS-1 DREAD 46 / TI-2 DREAD 45)
> - 整合性監査: [`../quality/FUNCTIONAL_SPEC_AUDIT.md`](../quality/FUNCTIONAL_SPEC_AUDIT.md) §5
>
> 本書を読む際は、記述内容が「実装済」ではなく「設計提案」である点に注意。Phase 12 で SECURITY_GAP_ANALYSIS.md §5 Option A〜D の判断が必要。

---

## 1. セキュリティ概要

### 1.1 セキュリティ目的

Phase 9.2システムにおける多層防御セキュリティアーキテクチャ：

- **通信セキュリティ**: WiFi TCP接続の暗号化・認証
- **データ保護**: 映像データ・設定データの機密性・完全性
- **アクセス制御**: 認証済みユーザーのみシステムアクセス許可
- **脆弱性対策**: 既知脅威に対する防御機構
- **監視・ログ**: セキュリティイベントの検知・記録

### 1.2 脅威モデル

| 脅威レベル | 攻撃者タイプ | 想定攻撃手法 | 対策優先度 |
|------------|--------------|--------------|------------|
| **高** | 悪意のあるローカルユーザー | ネットワーク盗聴、認証回避 | 🔴 最高 |
| **中** | 外部攻撃者 | WiFi侵入、脆弱性悪用 | 🟡 高 |
| **低** | 物理アクセス攻撃者 | デバイス改ざん、データ抽出 | 🟢 中 |

---

## 2. 通信セキュリティ

### 2.1 WiFi接続セキュリティ

#### WPA2-PSK設定
```yaml
WiFiSecurityConfig:
  protocol: "WPA2-PSK"
  encryption: "AES-CCMP"
  key_length: 256
  passphrase_policy:
    min_length: 12
    complexity: "英数字+記号"
    rotation_period: "90日"
```

#### 接続認証フロー
```c
// Phase 9.2 セキュア接続手順
typedef struct {
    char ssid[32];
    uint8_t psk_hash[32];        // SHA-256ハッシュ
    uint32_t auth_timeout_ms;    // 認証タイムアウト: 10s
    uint8_t retry_count;         // 再試行回数: 3回
} secure_wifi_config_t;

// セキュア接続状態監視
typedef enum {
    WIFI_SEC_DISCONNECTED = 0,
    WIFI_SEC_AUTHENTICATING,
    WIFI_SEC_CONNECTED_INSECURE,    // WEP/Open検出時
    WIFI_SEC_CONNECTED_SECURE,      // WPA2確認済み
    WIFI_SEC_AUTH_FAILURE          // 認証失敗
} wifi_security_state_t;
```

### 2.2 TCP通信セキュリティ

#### TLS暗号化 (Phase 9.2拡張)
```c
// TLS設定構造体
typedef struct {
    uint16_t tls_version;        // TLS 1.3推奨
    uint8_t cipher_suite;        // AES-256-GCM-SHA384
    uint32_t handshake_timeout;  // 5秒タイムアウト
    uint8_t cert_validation;     // 証明書検証レベル
} tls_config_t;

// セキュア健全性メトリクス
typedef struct {
    uint32_t tls_handshake_time; // TLSハンドシェイク時間
    uint16_t cipher_strength;    // 暗号強度スコア
    uint8_t cert_status;         // 証明書状態
    uint32_t encrypted_bytes;    // 暗号化バイト数
} secure_metrics_t;
```

---

## 3. データ保護

### 3.1 映像データセキュリティ

#### MJPEG暗号化拡張
```c
// Phase 9.2 暗号化MJPEG構造
typedef struct __attribute__((packed)) {
    // 標準MJPEGヘッダ
    uint8_t soi[2];              // Start of Image
    uint8_t app0[16];            // JFIF Application Segment

    // Phase 9.2 セキュリティ拡張
    uint8_t sec_marker[2];       // セキュリティマーカ: 0xFF, 0xE9
    uint16_t sec_length;         // セキュリティセグメント長
    uint8_t encryption_type;     // 暗号化方式: AES-128-CBC
    uint8_t key_id;             // キーID（ローテーション対応）
    uint8_t iv[16];             // 初期化ベクター
    uint8_t integrity_hash[32]; // SHA-256完全性ハッシュ

    // 暗号化されたJPEGデータ
    uint8_t encrypted_data[];   // AES暗号化画像データ
} secure_mjpeg_frame_t;
```

#### データ完全性検証
```c
// フレーム完全性検証
typedef enum {
    INTEGRITY_OK = 0,
    INTEGRITY_HASH_MISMATCH,
    INTEGRITY_DECRYPT_ERROR,
    INTEGRITY_KEY_INVALID
} integrity_status_t;

integrity_status_t verify_frame_integrity(
    const secure_mjpeg_frame_t* frame,
    const uint8_t* decrypt_key
);
```

### 3.2 設定データセキュリティ

#### 設定暗号化保存
```c
// Phase 9.2 セキュア設定管理
typedef struct {
    uint8_t magic[4];           // "SEC1"
    uint32_t version;           // 設定バージョン
    uint8_t salt[16];          // ソルト値
    uint8_t config_hash[32];   // 設定ハッシュ
    uint8_t encrypted_config[]; // 暗号化設定データ
} secure_config_t;

// 設定保存・読み込みAPI
int secure_config_save(const camera_config_t* config, const char* password);
int secure_config_load(camera_config_t* config, const char* password);
```

---

## 4. アクセス制御

### 4.1 認証システム

#### JWT認証 (PC側)
```json
{
  "header": {
    "alg": "HS256",
    "typ": "JWT"
  },
  "payload": {
    "sub": "spresense_device_001",
    "exp": 1706918400,
    "iat": 1706832000,
    "permissions": [
      "camera:stream",
      "config:read",
      "health:monitor"
    ],
    "device_id": "spresense_001",
    "security_level": "high"
  }
}
```

#### デバイス認証フロー
```c
// Spresenseデバイス認証
typedef struct {
    char device_id[16];         // デバイス固有ID
    uint8_t device_key[32];     // デバイス秘密鍵
    uint32_t auth_token;        // 認証トークン
    uint32_t token_expiry;      // トークン有効期限
    uint8_t permission_mask;    // 権限マスク
} device_auth_t;

// 認証状態
typedef enum {
    AUTH_PENDING = 0,
    AUTH_SUCCESS,
    AUTH_EXPIRED,
    AUTH_INVALID_TOKEN,
    AUTH_PERMISSION_DENIED
} auth_status_t;
```

### 4.2 権限管理

#### 権限レベル定義
```c
// Phase 9.2 権限システム
#define PERM_STREAM_READ    (1 << 0)  // 映像ストリーミング読み取り
#define PERM_CONFIG_READ    (1 << 1)  // 設定読み取り
#define PERM_CONFIG_WRITE   (1 << 2)  // 設定変更
#define PERM_HEALTH_MONITOR (1 << 3)  // 健全性監視
#define PERM_SYSTEM_ADMIN   (1 << 4)  // システム管理
#define PERM_SECURITY_AUDIT (1 << 5)  // セキュリティ監査

// 権限チェック関数
bool check_permission(const device_auth_t* auth, uint8_t required_perm);
```

---

## 5. セキュリティ監視・ログ

### 5.1 セキュリティイベント検知

#### 異常検知パターン
```c
// セキュリティイベント
typedef enum {
    SEC_EVENT_AUTH_FAILURE = 1,     // 認証失敗
    SEC_EVENT_UNAUTHORIZED_ACCESS,  // 不正アクセス試行
    SEC_EVENT_ENCRYPTION_ERROR,     // 暗号化エラー
    SEC_EVENT_INTEGRITY_VIOLATION,  // データ完全性違反
    SEC_EVENT_SUSPICIOUS_PATTERN,   // 疑わしいパターン
    SEC_EVENT_BRUTE_FORCE          // ブルートフォース攻撃
} security_event_type_t;

// セキュリティログエントリ
typedef struct {
    uint32_t timestamp;             // Unix timestamp
    security_event_type_t event;    // イベントタイプ
    uint32_t source_ip;            // 送信元IP
    char details[128];             // イベント詳細
    uint8_t severity;              // 重要度 (1-5)
    uint32_t event_count;          // 連続発生回数
} security_log_entry_t;
```

### 5.2 リアルタイム監視

#### 監視メトリクス
```c
// Phase 9.2 セキュリティ監視
typedef struct {
    // 認証監視
    uint32_t auth_attempts_total;   // 総認証試行数
    uint32_t auth_failures_1h;      // 1時間以内の失敗数
    uint32_t failed_source_ips;     // 失敗送信元IP数

    // 通信監視
    uint32_t tls_handshake_errors;  // TLSハンドシェイクエラー
    uint32_t cert_validation_errors; // 証明書検証エラー
    uint32_t encryption_failures;   // 暗号化失敗数

    // データ監視
    uint32_t integrity_violations;  // 完全性違反数
    uint32_t suspicious_patterns;   // 疑わしいパターン検出数

    // システム監視
    uint8_t security_level;         // 現在のセキュリティレベル
    uint32_t last_security_scan;    // 最終セキュリティスキャン時刻
} security_monitor_t;
```

---

## 6. 脆弱性対策

### 6.1 既知脆弱性対応

#### セキュアコーディング実装
```c
// バッファオーバーフロー対策
#define SAFE_STRCPY(dst, src, size) \
    do { \
        strncpy(dst, src, size - 1); \
        dst[size - 1] = '\0'; \
    } while(0)

// 入力値検証
bool validate_frame_size(uint32_t frame_size) {
    return (frame_size > 0 && frame_size <= MAX_FRAME_SIZE);
}

// メモリリーク対策
typedef struct {
    void* ptr;
    size_t size;
    const char* file;
    int line;
} secure_alloc_info_t;

void* secure_malloc(size_t size, const char* file, int line);
void secure_free(void* ptr, const char* file, int line);
```

### 6.2 セキュリティアップデート

#### セキュアブート対応
```c
// Phase 9.2 セキュアブート
typedef struct {
    uint8_t boot_signature[64];     // ブートローダー署名
    uint32_t firmware_version;      // ファームウェアバージョン
    uint8_t integrity_hash[32];     // ファームウェア完全性
    uint32_t security_patch_level;  // セキュリティパッチレベル
} secure_boot_info_t;

// セキュリティパッチ適用状況
typedef struct {
    uint32_t patch_id;              // パッチID
    uint32_t applied_date;          // 適用日時
    uint8_t patch_hash[32];        // パッチハッシュ
    bool requires_reboot;          // 再起動要否
} security_patch_t;
```

---

## 7. セキュリティテスト要件

### 7.1 ペネトレーションテスト

#### テストシナリオ
1. **WiFi認証バイパステスト**
   - WPA2-PSK総当たり攻撃耐性
   - 認証回避脆弱性チェック

2. **通信傍受テスト**
   - TCP通信暗号化強度確認
   - パケット解析耐性

3. **権限昇格テスト**
   - 認証トークン偽造試行
   - 権限チェック回避テスト

### 7.2 自動セキュリティ監査

#### 監査項目
```c
// セキュリティ監査チェックリスト
typedef struct {
    bool wifi_encryption_enabled;   // WiFi暗号化有効
    bool tls_connection_verified;   // TLS接続確認済み
    bool auth_tokens_valid;         // 認証トークン有効性
    bool config_encrypted;          // 設定暗号化済み
    bool logs_integrity_ok;         // ログ完全性OK
    uint8_t overall_score;          // 総合セキュリティスコア (0-100)
} security_audit_result_t;
```

---

## 8. セキュリティ実装ガイドライン

### 8.1 開発者向けガイドライン

#### セキュリティ開発原則
1. **最小権限の原則**: 必要最小限の権限のみ付与
2. **深層防御**: 複数のセキュリティレイヤーを実装
3. **セキュアデフォルト**: デフォルト設定で高セキュリティを確保
4. **入力値検証**: 全入力データの厳格な検証
5. **エラー情報制限**: セキュリティ関連エラーの詳細情報制限

### 8.2 運用セキュリティ

#### セキュリティ運用プロセス
```yaml
SecurityOperations:
  daily_tasks:
    - security_log_review
    - auth_failure_analysis
    - certificate_expiry_check

  weekly_tasks:
    - vulnerability_scan
    - security_patch_review
    - access_log_audit

  monthly_tasks:
    - penetration_test
    - security_policy_review
    - incident_response_drill
```

---

## 9. 緊急時対応手順

### 9.1 セキュリティインシデント対応

#### インシデント分類
| 重要度 | インシデント例 | 対応時間 | 対応手順 |
|--------|----------------|----------|----------|
| **Critical** | 不正アクセス検知 | 即座 | 接続遮断→調査→復旧 |
| **High** | 認証システム障害 | 1時間以内 | サービス停止→修復→検証 |
| **Medium** | 暗号化エラー | 4時間以内 | ログ解析→原因特定→修正 |

### 9.2 復旧手順

#### セキュリティ復旧プロセス
```bash
# Phase 9.2 セキュリティ緊急復旧
#!/bin/bash

# 1. システム隔離
echo "システムを安全モードに移行..."
systemctl stop security_camera_service

# 2. ログ保存
echo "セキュリティログをバックアップ..."
cp /var/log/security/* /backup/incident_$(date +%Y%m%d)/

# 3. システム検証
echo "システム完全性を検証..."
verify_system_integrity.sh

# 4. セキュリティ設定リセット
echo "セキュリティ設定を初期化..."
reset_security_config.sh

# 5. サービス復旧
echo "サービスを復旧..."
systemctl start security_camera_service
```

---

## 10. コンプライアンス

### 10.1 セキュリティ標準準拠

#### 準拠標準
- **ISO/IEC 27001**: 情報セキュリティマネジメント
- **NIST Cybersecurity Framework**: サイバーセキュリティフレームワーク
- **OWASP Top 10**: Webアプリケーションセキュリティ
- **IEC 62443**: 産業用制御システムセキュリティ

### 10.2 データ保護法規制

#### 個人データ保護
- 映像データの暗号化保存必須
- アクセスログの記録・保管
- データ削除・匿名化手順の整備
- ユーザー同意取得プロセス

---

**Phase 9.2セキュリティ機能仕様書 完成**
**次のステップ**: セキュリティアーキテクチャ設計詳細化、セキュリティテスト計画策定