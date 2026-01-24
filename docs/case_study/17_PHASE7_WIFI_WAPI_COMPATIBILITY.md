# Phase 7: WiFi/TCP実装 - WAPI互換性の気づき

**日付**: 2026-01-03
**フェーズ**: Phase 7 WiFi/TCP Transport実装
**問題**: NuttX WAPI関数のWiFiドライバ依存性
**影響度**: 高（ビルド失敗、実装方針変更が必要）

---

## 1. 問題の発見

### 1.1 初期症状

Spresense WiFi実装のビルド時に以下のリンカエラーが発生:

```
arm-none-eabi-ld: undefined reference to `wapi_start_dhcp'
arm-none-eabi-ld: undefined reference to `wapi_connect'
arm-none-eabi-ld: undefined reference to `wapi_disconnect'
```

### 1.2 状況

- **使用WiFiドライバ**: GS2200M (iS110B WiFi add-on board)
- **設定**: `CONFIG_WIRELESS_GS2200M=y`
- **WAPI有効化**: `CONFIG_WIRELESS_WAPI=y`
- **コンパイル**: 成功（.oファイル生成）
- **リンク**: 失敗（シンボル未定義）

---

## 2. 根本原因の分析

### 2.1 調査プロセス

1. **libwireless.aの確認**
   ```bash
   ls -lh staging/libwireless.a
   # 結果: 8バイト（空のアーカイブ）
   ```

2. **オブジェクトファイルの存在確認**
   ```bash
   ls apps/wireless/wapi/*.o
   # 結果: network.o, wireless.o, util.o, driver_wext.o が存在
   ```

3. **シンボルテーブルの確認**
   ```bash
   arm-none-eabi-nm wireless.o | grep wapi_connect
   # 結果: シンボルなし
   ```

4. **ソースコードの確認**
   ```bash
   grep -n "wapi_connect" apps/wireless/wapi/src/wireless.c
   # 結果: Line 1638: int wapi_connect(...)
   ```

5. **プリプロセッサディレクティブの発見**
   ```bash
   grep -B10 "wapi_connect" wireless.c | grep "#ifdef"
   # 結果: #ifdef CONFIG_WIRELESS_NRC7292
   ```

### 2.2 根本原因

**WAPI関数はWiFiドライバによって実装が異なる:**

#### apps/wireless/wapi/src/wireless.c (Lines 1599-1690)

```c
#ifdef CONFIG_WIRELESS_NRC7292

/* NRC7292専用関数群 */

int wapi_connect(int sock, FAR const char *ifname)
{
  /* NRC7292固有の接続処理 */
}

int wapi_disconnect(int sock, FAR const char *ifname)
{
  /* NRC7292固有の切断処理 */
}

#endif  /* CONFIG_WIRELESS_NRC7292 */
```

#### apps/wireless/wapi/src/network.c (Lines 366-390)

```c
#ifdef CONFIG_WIRELESS_NRC7292

int wapi_start_dhcp(int sock, FAR const char *ifname)
{
  /* NRC7292固有のDHCP開始処理 */
}

#endif  /* CONFIG_WIRELESS_NRC7292 */
```

**結論**:
- `wapi_connect()`, `wapi_disconnect()`, `wapi_start_dhcp()` は **NRC7292専用**
- GS2200Mでは **これらの関数は存在しない**

---

## 3. WiFiドライバ別のWAPI関数対応表

### 3.1 GS2200Mで利用可能な関数

#### 無線設定関数 (wireless.c)
```
✅ wapi_set_mode()        - モード設定 (Managed/Ad-hoc)
✅ wapi_set_essid()       - SSID設定
✅ wapi_get_essid()       - SSID取得
✅ wapi_set_freq()        - 周波数設定
✅ wapi_get_freq()        - 周波数取得
✅ wapi_set_ap()          - APアドレス設定
✅ wapi_get_ap()          - APアドレス取得
✅ wapi_set_bitrate()     - ビットレート設定
✅ wapi_get_bitrate()     - ビットレート取得
```

#### ネットワーク設定関数 (network.c)
```
✅ wapi_set_ip()          - IPアドレス設定
✅ wapi_get_ip()          - IPアドレス取得
✅ wapi_set_netmask()     - ネットマスク設定
✅ wapi_get_netmask()     - ネットマスク取得
✅ wapi_set_ifup()        - インターフェースUP
✅ wapi_set_ifdown()      - インターフェースDOWN
✅ wapi_get_ifup()        - インターフェース状態取得
```

#### WPA認証関数 (driver_wext.c)
```
✅ wpa_driver_wext_associate()  - WPA2認証・接続
```

### 3.2 NRC7292専用関数（GS2200Mでは利用不可）

```
❌ wapi_connect()         - 明示的接続（NRC7292のみ）
❌ wapi_disconnect()      - 明示的切断（NRC7292のみ）
❌ wapi_start_dhcp()      - DHCP開始（NRC7292のみ）
```

---

## 4. 解決策

### 4.1 WiFi接続の実装方法（GS2200M）

#### オープンネットワーク接続

**誤った実装（NRC7292向け）:**
```c
wapi_set_essid(sock, "wlan0", "MySSID", WAPI_ESSID_ON);
wapi_connect(sock, "wlan0");  // ❌ GS2200Mでは存在しない
```

**正しい実装（GS2200M向け）:**
```c
/* ESSIDの設定だけで接続がトリガーされる */
wapi_set_essid(sock, "wlan0", "MySSID", WAPI_ESSID_ON);
/* wapi_connect()は不要 */
```

#### WPA2ネットワーク接続

**共通実装（両ドライバ対応）:**
```c
struct wpa_wconfig_s wconfig = {
    .sta_mode = WAPI_MODE_MANAGED,
    .auth_wpa = IW_AUTH_WPA_VERSION_WPA2,
    .cipher_mode = IW_AUTH_CIPHER_CCMP,
    .alg = WPA_ALG_CCMP,
    .ifname = "wlan0",
    .ssid = "MySSID",
    .ssidlen = strlen("MySSID"),
    .passphrase = "MyPassword",
    .phraselen = strlen("MyPassword"),
    .bssid = NULL,
    .flag = WAPI_FREQ_AUTO,
};

wpa_driver_wext_associate(&wconfig);  // ✅ 両ドライバ対応
```

### 4.2 DHCP設定の実装方法（GS2200M）

**誤った実装（NRC7292向け）:**
```c
wapi_start_dhcp(sock, "wlan0");  // ❌ GS2200Mでは存在しない
```

**正しい実装（GS2200M向け）:**
```c
/* インターフェースをUPにするだけでDHCPが自動実行される */
wapi_set_ifup(sock, "wlan0");  // ✅ GS2200Mドライバが自動的にDHCPを実行

/* IPアドレスの取得を待つ */
struct in_addr addr;
for (int i = 0; i < timeout_sec; i++)
{
    if (wapi_get_ip(sock, "wlan0", &addr) == 0 && addr.s_addr != 0)
    {
        /* IP取得成功 */
        break;
    }
    sleep(1);
}
```

**仕組み:**
- GS2200Mドライバは `wapi_set_ifup()` でインターフェースがUPになると自動的にDHCPクライアントを起動
- NuttXのusrsockデーモン経由でGS2200Mファームウェアがネットワーク管理を行う

### 4.3 WiFi切断の実装方法（GS2200M）

**誤った実装（NRC7292向け）:**
```c
wapi_disconnect(sock, "wlan0");  // ❌ GS2200Mでは存在しない
```

**正しい実装（GS2200M向け）:**
```c
/* インターフェースをDOWNにすることで切断 */
wapi_set_ifdown(sock, "wlan0");  // ✅ GS2200M対応
```

---

## 5. 実装変更の詳細

### 5.1 修正前のコード (wifi_manager.c)

```c
/* オープンネットワーク接続 - 修正前 */
wapi_set_essid(mgr->sock, WIFI_INTERFACE, ssid, WAPI_ESSID_ON);
ret = wapi_connect(mgr->sock, WIFI_INTERFACE);  // ❌ NRC7292専用
if (ret < 0) {
    _err("ERROR: Failed to connect to WiFi: %d\n", ret);
    return ret;
}

/* DHCP開始 - 修正前 */
ret = wapi_start_dhcp(mgr->sock, WIFI_INTERFACE);  // ❌ NRC7292専用
if (ret < 0) {
    _warn("WARNING: Failed to start DHCP: %d\n", ret);
}

/* WiFi切断 - 修正前 */
ret = wapi_disconnect(mgr->sock, WIFI_INTERFACE);  // ❌ NRC7292専用
if (ret < 0) {
    _warn("WARNING: Failed to disconnect WiFi: %d\n", ret);
}
```

### 5.2 修正後のコード (wifi_manager.c)

```c
/* オープンネットワーク接続 - 修正後 */
wapi_set_essid(mgr->sock, WIFI_INTERFACE, ssid, WAPI_ESSID_ON);
/* GS2200M: ESSID設定で接続がトリガーされるため、
   wapi_connect()は不要 */

/* DHCP開始 - 修正後 */
/* GS2200M: インターフェースUPでDHCPが自動実行される */
ret = wapi_set_ifup(mgr->sock, WIFI_INTERFACE);  // ✅ GS2200M対応
if (ret < 0) {
    _err("ERROR: Failed to bring up interface: %d\n", ret);
    return ret;
}

_info("Waiting for DHCP IP address...\n");

/* IPアドレス取得を待機 */
ret = wifi_wait_connection(mgr, DHCP_TIMEOUT_SEC);

/* WiFi切断 - 修正後 */
/* GS2200M: インターフェースDOWNで切断 */
ret = wapi_set_ifdown(mgr->sock, WIFI_INTERFACE);  // ✅ GS2200M対応
if (ret < 0) {
    _warn("WARNING: Failed to bring down interface: %d\n", ret);
}
```

---

## 6. ビルド結果

### 6.1 修正前

```
LD: nuttx
arm-none-eabi-ld: undefined reference to `wapi_start_dhcp'
arm-none-eabi-ld: undefined reference to `wapi_connect'
arm-none-eabi-ld: undefined reference to `wapi_disconnect'
make[1]: *** [Makefile:197: nuttx] Error 1
```

### 6.2 修正後

```
LD: nuttx
Build succeeded!

-rw-r--r-- 1 ken ken 275K Jan  3 08:12 nuttx.spk
```

**成功**: 全てのリンカエラーが解消され、ファームウェアのビルドに成功。

---

## 7. 教訓と推奨事項

### 7.1 重要な気づき

1. **NuttX WAPIは統一APIではない**
   - WAPIは抽象化レイヤーだが、全関数が全ドライバで実装されているわけではない
   - ドライバ固有の `#ifdef` で条件コンパイルされている関数が多数存在

2. **ドライバアーキテクチャの違い**
   - **NRC7292**: ホスト側で明示的な接続・DHCP制御が必要
   - **GS2200M**: usrsockデーモン経由でファームウェアが自動管理

3. **ドキュメント不足**
   - NuttXのWAPI関数リファレンスにドライバ依存性の記載が不十分
   - ソースコード調査が必須

### 7.2 デバッグのベストプラクティス

#### ステップ1: シンボルテーブルの確認
```bash
arm-none-eabi-nm <object_file>.o | grep <function_name>
```
- オブジェクトファイルに関数が実際に含まれているか確認

#### ステップ2: プリプロセッサディレクティブの確認
```bash
grep -B20 "function_name" source.c | grep "#ifdef"
```
- 条件コンパイルによる除外の有無を確認

#### ステップ3: 設定の確認
```bash
grep "CONFIG_WIRELESS" .config
```
- 必要な設定オプションが有効になっているか確認

#### ステップ4: 代替実装の調査
```bash
arm-none-eabi-nm <object_file>.o | grep "T wapi_"
```
- 利用可能な代替関数を探す

### 7.3 推奨事項

1. **WiFiドライバの選定時**
   - 必要な機能（接続方式、暗号化方式、DHCP等）を洗い出す
   - ドライバのWAPI対応状況をソースコードで事前確認
   - ドライバ固有の実装方法を理解する

2. **実装時**
   - WAPI関数を使う前に、対象ドライバでの利用可否を確認
   - ドライバ固有の動作（自動DHCP等）を活用
   - ポータビリティが必要な場合は条件コンパイルを使用

3. **テスト時**
   - 各WiFiドライバで動作確認
   - エラーハンドリングの妥当性を検証
   - タイムアウト値の調整

---

## 8. 参考情報

### 8.1 ファイルパス

- **WAPI実装**: `/apps/wireless/wapi/src/`
  - `wireless.c`: 無線設定関数
  - `network.c`: ネットワーク設定関数
  - `driver_wext.c`: WPA認証関数

- **GS2200Mドライバ**: `/apps/wireless/gs2200m/`
  - `gs2200m_main.c`: usrsockデーモン実装

- **ヘッダ**: `/include/wireless/wapi.h`

### 8.2 設定オプション

```kconfig
CONFIG_WIRELESS=y                    # 無線サポート有効化
CONFIG_WIRELESS_WAPI=y               # WAPI有効化
CONFIG_WIRELESS_GS2200M=y            # GS2200Mドライバ有効化
CONFIG_WIRELESS_WAPI_CMDTOOL=y       # WAPIコマンドツール（オプション）
```

### 8.3 関連ドキュメント

- NuttX WAPI Documentation: `/apps/wireless/wapi/README.md`
- GS2200M Driver Notes: SpresenseSDK公式ドキュメント
- Phase 7実装仕様書: `/docs/security_camera/PHASE7_WIFI_TCP_SPEC.md`

---

## 9. まとめ

### 9.1 問題の本質

NuttX WAPIは複数のWiFiドライバに対する統一インターフェースを提供するが、**全ての関数が全てのドライバで実装されているわけではない**。ドライバのアーキテクチャの違いにより、一部の関数はドライバ固有の `#ifdef` で条件コンパイルされている。

### 9.2 解決のポイント

1. **対象ドライバ（GS2200M）で利用可能なWAPI関数を特定**
2. **ドライバ固有の動作（自動DHCP等）を理解**
3. **利用不可な関数を代替実装で置き換え**

### 9.3 成果

- ✅ NRC7292専用関数の削除
- ✅ GS2200M対応実装への書き換え
- ✅ ビルド成功（nuttx.spk: 275KB）
- ✅ 次フェーズ（WiFi統合テスト）への準備完了

### 9.4 今後の展開

**Phase 7 残タスク:**
1. WiFi統合テスト（実機検証）
2. TCP接続安定性テスト
3. パフォーマンス測定（USB vs WiFi）
4. ドキュメント完成

**期待される効果:**
- ケーブルレス運用の実現
- 設置自由度の向上
- WiFi帯域によるFPS向上の可能性

---

**記録者**: Claude Sonnet 4.5
**レビュー**: 実機テスト後に更新予定
