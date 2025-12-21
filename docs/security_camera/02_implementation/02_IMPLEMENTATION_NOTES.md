# Security Camera 実装ノート

**作成日**: 2025-12-16
**Phase**: Phase 1A 完了（カメラ初期化・バッファ管理）

---

## 実装の進捗

### 完了した項目

- [x] Phase 1A: 基本カメラ初期化とUSB転送準備
  - [x] カメラマネージャ実装
  - [x] エンコーダマネージャ実装
  - [x] USB転送実装
  - [x] プロトコルハンドラ実装
  - [x] メインアプリケーション実装

### 現在の状態

- **カメラ初期化**: 動作確認済み（公式cameraサンプルで確認）
- **バッファ管理**: 32バイトアライメント対応完了
- **次のステップ**: 実機でのsecurity_cameraテスト

---

## 重要な発見と学習事項

### 1. 必須設定の発見

Phase 1の実装中に、以下の設定が**絶対に必要**であることが判明：

#### CONFIG_CXD56_I2C2=y の重要性

**発見経緯**:
1. 最初は `CONFIG_CXD56_I2C0=y` のみ有効
2. カメラ初期化時に Hard Fault が発生
3. 公式 camera サンプルの defconfig を確認
4. **I2C2** が必須であることを発見

**理由**:
CXD5602PWBCAM2W カメラボードは **I2C2 バス**に接続されています。I2C0 では通信できません。

```bash
# 必須設定
CONFIG_CXD56_I2C2=y
```

**確認方法**:
```bash
nsh> ls /dev
/dev:
 i2c0
 i2c2    # ← これが表示されること
```

---

#### CONFIG_SPECIFIC_DRIVERS=y の重要性

**発見経緯**:
1. VIDEO系の設定は全て有効だが、`errno = 2 (ENOENT)` エラー
2. `/dev/video` デバイスが作成されない
3. ボードレベルの初期化が実行されていないことが判明
4. `CONFIG_SPECIFIC_DRIVERS=y` が不足していることを発見

**理由**:
この設定がないと、ボードレベルのカメラ初期化関数が呼ばれません：
- `isx012_initialize()`
- `cxd56_cisif_initialize()`

```bash
# 必須設定
CONFIG_SPECIFIC_DRIVERS=y
```

**影響**:
- この設定がないと、`video_initialize()` は `-ENOENT` を返す
- ボード固有のドライバが初期化されない

---

### 2. V4L2 バッファ管理の実装

#### USERPTR モードの罠

**問題**:
最初の実装では、バッファポインタを設定せずに `VIDIOC_QBUF` を呼び出していました：

```c
// ❌ 初期実装（間違い）
struct v4l2_buffer buf;
buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
buf.memory = V4L2_MEMORY_USERPTR;
buf.index = i;
// buf.m.userptr が未設定！
// buf.length が未設定！
ioctl(fd, VIDIOC_QBUF, &buf);
```

**結果**:
Hard Fault (CFSR: 00040000 = アンアライメントメモリアクセス)

**解決策**:
公式 camera サンプルを調査し、正しい実装を発見：

```c
// ✅ 修正後の実装
void *buffer = memalign(32, bufsize);  // 32バイトアラインメント

struct v4l2_buffer buf;
buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
buf.memory = V4L2_MEMORY_USERPTR;
buf.index = i;
buf.m.userptr = (unsigned long)buffer;  // ⭐ 重要
buf.length = bufsize;                   // ⭐ 重要
ioctl(fd, VIDIOC_QBUF, &buf);
```

#### 32バイトアラインメントの必要性

**なぜ必要か**:
- Spresense のカメラドライバは DMA 転送を使用
- DMA は特定のアラインメントを要求
- 32バイトアラインメントでないと、アクセス違反が発生

**実装**:
```c
#include <malloc.h>

void *buffer = memalign(32, size);
if (buffer == NULL) {
    // メモリ不足エラー
}

// 使用後は必ず解放
free(buffer);
```

---

### 3. video_initialize() の役割

**発見**:
カメラデバイスをオープンする前に、`video_initialize()` を呼び出す必要があります。

**理由**:
1. `/dev/video` デバイスノードを作成
2. ビデオドライバを登録
3. 画像センサーとデータインターフェースを接続

**実装例**:
```c
// camera_manager.c 139行目
ret = video_initialize(VIDEO_DEVICE_PATH);
if (ret < 0)
{
    LOG_ERROR("Failed to initialize video driver: %d", ret);
    return ERR_CAMERA_INIT;
}

// その後でデバイスをオープン
g_camera_mgr.fd = open(VIDEO_DEVICE_PATH, O_RDONLY);
```

**注意**:
- `video_initialize()` は1回だけ呼び出す
- すでに初期化されている場合でも、通常はエラーにならない

---

### 4. ビルドシステムの理解

#### config.py の必要性

**発見経緯**:
1. `defconfig` に設定を追加
2. `make` を実行
3. アプリが `help` に表示されない
4. `./tools/config.py default` を実行していないことが判明

**理由**:
- `defconfig` は設定の**テンプレート**
- `config.py` が defconfig の内容を `nuttx/.config` に適用
- `make` は `nuttx/.config` を参照

**正しい手順**:
```bash
# 1. defconfig を編集
nano sdk/configs/default/defconfig

# 2. config.py で適用 ⭐ 重要
./tools/config.py default

# 3. ビルド
make
```

---

#### 2重 .config 構造の罠

Spresense には2つの `.config` ファイルがあります：

| ファイル | サイズ | 用途 | 確認すべきか |
|---------|--------|------|------------|
| `sdk/.config` | 295-400B | 使われない | ❌ |
| `nuttx/.config` | 60-70KB | 実際に使われる | ✅ |

**間違いやすいポイント**:
```bash
# ❌ 間違い: SDK側を見てしまう
grep "CONFIG_XXX" sdk/.config

# ✅ 正しい: NuttX側を見る
grep "CONFIG_XXX" nuttx/.config
```

---

## カメラハードウェア仕様

### 使用カメラボード

**型番**: CXD5602PWBCAM2W

**仕様**:
- センサー: ISX012 (Sony製)
- 解像度: 最大 Full HD (1920x1080)
- HDR: 対応
- インターフェース: I2C2 バス
- 電源: Spresense メインボードから供給

**接続**:
- カメラボードをメインボードに直接接続
- 向きに注意（コネクタの形状を確認）

---

## 現在の実装構成

### ファイル構成

```
apps/examples/security_camera/
├── Kconfig                      # ビルド設定
├── Makefile                     # ビルドルール
├── Make.defs                    # アプリ登録
├── camera_manager.c/h           # カメラ制御
├── encoder_manager.c/h          # H.264エンコーダ
├── usb_transport.c/h            # USB CDC転送
├── protocol_handler.c/h         # 通信プロトコル
├── camera_app_main.c            # メインエントリ
└── config.h                     # 設定とログマクロ
```

### 主要な設定値

```c
// config.h

// カメラ設定
#define CONFIG_CAMERA_WIDTH       1280      // HD解像度
#define CONFIG_CAMERA_HEIGHT      720
#define CONFIG_CAMERA_FPS         30
#define CONFIG_CAMERA_FORMAT      V4L2_PIX_FMT_UYVY

// エンコーダ設定
#define CONFIG_ENCODER_BITRATE    2000000   // 2 Mbps
#define CONFIG_ENCODER_GOP_SIZE   30

// バッファ設定
#define CAMERA_BUFFER_NUM         2         // ダブルバッファ
```

---

## 実装パターン

### パターン 1: カメラ初期化

```c
int camera_manager_init(const camera_config_t *config)
{
    // 1. ビデオドライバ初期化
    ret = video_initialize("/dev/video");

    // 2. デバイスオープン
    fd = open("/dev/video", O_RDONLY);

    // 3. フォーマット設定
    struct v4l2_format fmt = {...};
    ioctl(fd, VIDIOC_S_FMT, &fmt);

    // 4. フレームレート設定
    struct v4l2_streamparm parm = {...};
    ioctl(fd, VIDIOC_S_PARM, &parm);

    // 5. バッファ要求
    struct v4l2_requestbuffers req = {...};
    ioctl(fd, VIDIOC_REQBUFS, &req);

    // 6. バッファ割り当てとキュー
    for (i = 0; i < BUFFER_NUM; i++) {
        void *buf = memalign(32, bufsize);
        buf.m.userptr = (unsigned long)buf;
        buf.length = bufsize;
        ioctl(fd, VIDIOC_QBUF, &buf);
    }

    // 7. ストリーミング開始
    ioctl(fd, VIDIOC_STREAMON, &type);
}
```

---

### パターン 2: フレーム取得

```c
int camera_get_frame(camera_frame_t *frame)
{
    // 1. ポーリングで待機
    poll(fds, 1, timeout);

    // 2. バッファデキュー
    ioctl(fd, VIDIOC_DQBUF, &buf);

    // 3. フレーム情報設定
    frame->buf = (uint8_t *)buf.m.userptr;
    frame->size = buf.bytesused;
    frame->timestamp_us = get_timestamp_us();

    // 4. バッファ再キュー
    ioctl(fd, VIDIOC_QBUF, &buf);
}
```

---

### パターン 3: クリーンアップ

```c
int camera_manager_cleanup(void)
{
    // 1. ストリーミング停止
    ioctl(fd, VIDIOC_STREAMOFF, &type);

    // 2. バッファ解放
    for (i = 0; i < BUFFER_NUM; i++) {
        free(buffers[i]);
    }

    // 3. デバイスクローズ
    close(fd);
}
```

---

## 次のステップ

### Phase 1B: MP4保存機能

**TODO**:
1. H.264エンコーダの統合テスト
2. USB CDC経由でのデータ送信確認
3. PC側Rustプログラムでの受信テスト
4. MP4ファイル生成

### Phase 2: WiFi対応

**TODO**:
1. WiFi拡張ボードの接続
2. TCP/IP通信の実装
3. RTSPプロトコル対応

---

## 参考資料

### 公式ドキュメント

- Spresense SDK ガイド
- NuttX V4L2 ドライバAPI
- ISX012 データシート

### 実装例

- `/home/ken/Spr_ws/spresense/sdk/apps/examples/camera/camera_main.c`
- Camera example defconfig: `sdk/configs/examples/camera/defconfig`

### 作成したドキュメント

- `FUNCTIONAL_SPEC.md` - 機能仕様
- `SOFTWARE_SPEC_SPRESENSE.md` - ソフトウェア仕様
- `PROTOCOL_SPEC.md` - 通信プロトコル
- `IMPLEMENTATION_PLAN.md` - 実装計画
- `TROUBLESHOOTING.md` - トラブルシューティング

---

## 開発環境

### ハードウェア

- **メインボード**: Spresense CXD5602
- **カメラボード**: CXD5602PWBCAM2W (ISX012 センサー)
- **開発PC**: Ubuntu on WSL2

### ソフトウェア

- **SDK**: Spresense SDK v3.4.5
- **NuttX**: v12.3.0
- **ツールチェーン**: arm-none-eabi-gcc 13.2.1
- **ビルドシステム**: GNU Make

---

**作成者**: Claude Code (Sonnet 4.5)
**最終更新**: 2025-12-16
