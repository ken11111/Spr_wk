# Spresense カメラ設定 クイックリファレンス

**対象**: CXD5602PWBCAM2W (ISX012 センサー)
**最終更新**: 2025-12-16

---

## ⚡ 必須設定 (コピペ用)

### defconfig に追加する設定

```bash
# === カメラ必須設定 ===
CONFIG_VIDEO=y
CONFIG_VIDEO_STREAM=y
CONFIG_VIDEO_ISX012=y
CONFIG_CXD56_CISIF=y
CONFIG_CXD56_I2C2=y              # ⭐ 重要: カメラはI2C2に接続
CONFIG_SPECIFIC_DRIVERS=y         # ⭐ 重要: ボード固有ドライバ
CONFIG_DRIVERS_VIDEO=y

# === エンコーダ設定 (H.264使用時) ===
CONFIG_VIDEOUTILS_CODEC_H264=y

# === USB CDC設定 (USB転送使用時) ===
CONFIG_CDCACM=y
CONFIG_USBDEV=y
```

### 設定適用とビルド

```bash
cd /home/ken/Spr_ws/spresense/sdk
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin:$PATH

# 1. 設定適用 (⭐ 必須!)
./tools/config.py default

# 2. ビルド
make

# 3. フラッシュ
sudo tools/flash.sh -c /dev/ttyUSB0 nuttx.spk
```

---

## 📋 設定チェックリスト

### 必須設定確認

```bash
# NuttX .config を確認 (実際に使われる設定)
grep "CONFIG_CXD56_CISIF\|CONFIG_CXD56_I2C2\|CONFIG_SPECIFIC_DRIVERS" \
  /home/ken/Spr_ws/spresense/nuttx/.config

# 期待される出力:
# CONFIG_CXD56_CISIF=y
# CONFIG_CXD56_I2C2=y
# CONFIG_SPECIFIC_DRIVERS=y
```

### デバイス確認

```bash
# NuttShell で確認
nsh> ls /dev

# 期待される出力:
# i2c2    ← I2C2が有効
# video   ← カメラドライバが初期化成功
```

---

## 🎛️ サポートされる設定値

### 解像度 (ISX012)

| 解像度 | 幅 | 高さ | 定数 | サポート |
|--------|-----|------|------|---------|
| QVGA | 320 | 240 | VIDEO_HSIZE/VSIZE_QVGA | ✅ |
| VGA | 640 | 480 | VIDEO_HSIZE/VSIZE_VGA | ✅ |
| HD | 1280 | 720 | VIDEO_HSIZE/VSIZE_HD | ✅ |
| QUADVGA | 1280 | 960 | VIDEO_HSIZE/VSIZE_QUADVGA | ✅ |
| FULLHD | 1920 | 1080 | VIDEO_HSIZE/VSIZE_FULLHD | ✅ |

### ピクセルフォーマット

| フォーマット | 定数 | サポート | 用途 |
|------------|------|---------|------|
| RGB565 | V4L2_PIX_FMT_RGB565 | ✅ | ビデオストリーム |
| UYVY | V4L2_PIX_FMT_UYVY | ✅ | ビデオストリーム |
| JPEG | V4L2_PIX_FMT_JPEG | ✅ | 静止画キャプチャ |

**推奨**:
- ビデオ: RGB565 (動作実績あり)
- H.264エンコード用: UYVY → YUV420変換が必要

### フレームレート

| FPS | サポート | 備考 |
|-----|---------|------|
| 5 | ✅ | |
| 6 | ✅ | |
| 7.5 | ✅ | |
| 15 | ✅ | |
| 30 | ✅ | 推奨 |
| 60 | ❌ | 低解像度のみ |
| 120 | ❌ | 低解像度のみ |

---

## 💻 実装パターン

### パターン 1: カメラ初期化

```c
#include <nuttx/video/video.h>

// 1. ビデオドライバ初期化
int ret = video_initialize("/dev/video");
if (ret < 0) {
    printf("Failed to initialize video: %d\n", ret);
    return ERROR;
}

// 2. デバイスオープン
int fd = open("/dev/video", O_RDONLY);
if (fd < 0) {
    printf("Failed to open video: %d\n", errno);
    return ERROR;
}

// 3. フォーマット設定
struct v4l2_format fmt;
memset(&fmt, 0, sizeof(fmt));
fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
fmt.fmt.pix.width = 1280;
fmt.fmt.pix.height = 720;
fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
fmt.fmt.pix.field = V4L2_FIELD_ANY;

ret = ioctl(fd, VIDIOC_S_FMT, (unsigned long)&fmt);
if (ret < 0) {
    printf("Failed to set format: %d\n", errno);
    close(fd);
    return ERROR;
}
```

### パターン 2: バッファ割り当て

```c
#include <malloc.h>

#define BUFFER_NUM 2

struct camera_buffer_s {
    void *start;
    uint32_t length;
};

struct camera_buffer_s buffers[BUFFER_NUM];

// バッファサイズ取得
uint32_t bufsize = fmt.fmt.pix.sizeimage;

// バッファ割り当て (32バイトアライメント必須!)
for (int i = 0; i < BUFFER_NUM; i++) {
    buffers[i].start = memalign(32, bufsize);
    if (buffers[i].start == NULL) {
        printf("Failed to allocate buffer %d\n", i);
        // エラー処理: 既に割り当てたバッファを解放
        while (i > 0) {
            i--;
            free(buffers[i].start);
        }
        return ERROR;
    }
    buffers[i].length = bufsize;
}
```

### パターン 3: バッファキュー

```c
// バッファ要求
struct v4l2_requestbuffers req;
memset(&req, 0, sizeof(req));
req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
req.memory = V4L2_MEMORY_USERPTR;
req.count = BUFFER_NUM;
req.mode = V4L2_BUF_MODE_RING;

ret = ioctl(fd, VIDIOC_REQBUFS, (unsigned long)&req);

// バッファキュー
for (int i = 0; i < BUFFER_NUM; i++) {
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_USERPTR;
    buf.index = i;
    buf.m.userptr = (unsigned long)buffers[i].start;  // ⭐ 重要
    buf.length = buffers[i].length;                   // ⭐ 重要

    ret = ioctl(fd, VIDIOC_QBUF, (unsigned long)&buf);
    if (ret < 0) {
        printf("Failed to queue buffer %d: %d\n", i, errno);
        return ERROR;
    }
}
```

### パターン 4: ストリーミング開始

```c
enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
ret = ioctl(fd, VIDIOC_STREAMON, (unsigned long)&type);
if (ret < 0) {
    printf("Failed to start streaming: %d\n", errno);
    return ERROR;
}
```

### パターン 5: フレーム取得

```c
#include <poll.h>

// ポーリングで待機
struct pollfd fds[1];
fds[0].fd = fd;
fds[0].events = POLLIN;

ret = poll(fds, 1, 1000);  // 1秒タイムアウト
if (ret == 0) {
    printf("Timeout\n");
    return ERROR;
}

// バッファデキュー
struct v4l2_buffer buf;
memset(&buf, 0, sizeof(buf));
buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
buf.memory = V4L2_MEMORY_USERPTR;

ret = ioctl(fd, VIDIOC_DQBUF, (unsigned long)&buf);
if (ret < 0) {
    printf("Failed to dequeue buffer: %d\n", errno);
    return ERROR;
}

// フレームデータ使用
uint8_t *frame_data = (uint8_t *)buf.m.userptr;
uint32_t frame_size = buf.bytesused;

// 処理...

// バッファ再キュー
ret = ioctl(fd, VIDIOC_QBUF, (unsigned long)&buf);
```

### パターン 6: クリーンアップ

```c
// ストリーミング停止
enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
ioctl(fd, VIDIOC_STREAMOFF, (unsigned long)&type);

// バッファ解放
for (int i = 0; i < BUFFER_NUM; i++) {
    if (buffers[i].start != NULL) {
        free(buffers[i].start);
        buffers[i].start = NULL;
    }
}

// デバイスクローズ
close(fd);
```

---

## ⚠️ よくある間違い

### 間違い 1: I2C2 を有効にしていない

```bash
# ❌ 間違い
CONFIG_CXD56_I2C0=y  # カメラは I2C0 ではない!

# ✅ 正しい
CONFIG_CXD56_I2C2=y  # カメラは I2C2 に接続
```

### 間違い 2: SPECIFIC_DRIVERS を有効にしていない

```bash
# ❌ 間違い: ボード固有ドライバが初期化されない
# CONFIG_SPECIFIC_DRIVERS is not set

# ✅ 正しい
CONFIG_SPECIFIC_DRIVERS=y
```

### 間違い 3: config.py を実行していない

```bash
# ❌ 間違い
nano sdk/configs/default/defconfig
make  # 設定が反映されない!

# ✅ 正しい
nano sdk/configs/default/defconfig
./tools/config.py default  # ⭐ 必須
make
```

### 間違い 4: バッファポインタを設定していない

```c
// ❌ 間違い
struct v4l2_buffer buf;
buf.memory = V4L2_MEMORY_USERPTR;
buf.index = i;
// buf.m.userptr と buf.length が未設定!
ioctl(fd, VIDIOC_QBUF, &buf);  // → Hard Fault

// ✅ 正しい
buf.m.userptr = (unsigned long)buffer;
buf.length = bufsize;
ioctl(fd, VIDIOC_QBUF, &buf);
```

### 間違い 5: メモリアライメント不足

```c
// ❌ 間違い
void *buffer = malloc(size);  // アライメント保証なし

// ✅ 正しい
void *buffer = memalign(32, size);  // 32バイトアライメント
```

---

## 🔍 デバッグ手順

### 手順 1: 設定確認

```bash
# 必須設定が有効か確認
grep "CONFIG_CXD56_CISIF\|CONFIG_CXD56_I2C2\|CONFIG_SPECIFIC_DRIVERS" \
  /home/ken/Spr_ws/spresense/nuttx/.config
```

### 手順 2: デバイス確認

```bash
nsh> ls /dev
# i2c2 と video が表示されること
```

### 手順 3: 公式サンプルでテスト

```bash
# 公式 camera サンプルで動作確認
./tools/config.py examples/camera
make
sudo tools/flash.sh -c /dev/ttyUSB0 nuttx.spk

nsh> camera
# 成功すれば、ハードウェアと設定は正常
```

### 手順 4: ログ出力確認

```c
// 各ステップでログ出力
LOG_INFO("Initializing video driver");
ret = video_initialize("/dev/video");
LOG_INFO("Video driver initialized: ret=%d", ret);

LOG_INFO("Opening video device");
fd = open("/dev/video", O_RDONLY);
LOG_INFO("Video device opened: fd=%d", fd);

LOG_INFO("Setting format: %dx%d", width, height);
ret = ioctl(fd, VIDIOC_S_FMT, &fmt);
LOG_INFO("Format set: ret=%d, errno=%d", ret, errno);
```

---

## 📚 参考リンク

### ドキュメント

- `/home/ken/Spr_ws/spresense/security_camera/TROUBLESHOOTING.md`
- `/home/ken/Spr_ws/spresense/security_camera/IMPLEMENTATION_NOTES.md`
- `/home/ken/Spr_ws/case_study/prompts/camera_lessons_learned.md`

### サンプルコード

- `/home/ken/Spr_ws/spresense/sdk/apps/examples/camera/camera_main.c`
- `/home/ken/Spr_ws/spresense/sdk/apps/examples/security_camera/camera_manager.c`

### 設定ファイル

- `/home/ken/Spr_ws/spresense/sdk/configs/examples/camera/defconfig`
- `/home/ken/Spr_ws/spresense/sdk/configs/default/defconfig`

---

**作成者**: Claude Code (Sonnet 4.5)
**最終更新**: 2025-12-16
