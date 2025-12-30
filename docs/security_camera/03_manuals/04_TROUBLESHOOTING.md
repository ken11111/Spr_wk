# Security Camera トラブルシューティングガイド

**作成日**: 2025-12-16
**対象**: Spresense カメラアプリケーション開発

---

## 目次

1. [カメラ初期化エラー](#カメラ初期化エラー)
2. [設定関連の問題](#設定関連の問題)
3. [ビルド関連の問題](#ビルド関連の問題)
4. [実装上の注意点](#実装上の注意点)

---

## カメラ初期化エラー

### エラー 1: `Failed to initialize video driver: -22 (EINVAL)`

**症状**:
```
[CAM] Initializing video driver: /dev/video
[CAM] Failed to initialize video driver: -22
```

**原因**: 必須設定が不足している

**解決策**:

以下の設定を `sdk/configs/default/defconfig` に追加：

```bash
# 必須カメラ設定
CONFIG_VIDEO=y
CONFIG_VIDEO_STREAM=y
CONFIG_VIDEO_ISX012=y
CONFIG_CXD56_CISIF=y
CONFIG_CXD56_I2C2=y              # ⭐ 重要: カメラはI2C2バスに接続
CONFIG_SPECIFIC_DRIVERS=y         # ⭐ 重要: ボード固有ドライバ
CONFIG_DRIVERS_VIDEO=y
CONFIG_VIDEOUTILS_CODEC_H264=y   # H.264エンコーダ
```

設定適用後、**必ず** `config.py` を実行：
```bash
cd /home/ken/Spr_ws/spresense/sdk
./tools/config.py default
make
```

---

### エラー 2: `Failed to initialize video driver: errno = 2 (ENOENT)`

**症状**:
```
ERROR: Failed to initialize video: errno = 2
```

**原因**: `CONFIG_SPECIFIC_DRIVERS=y` が無効

**解決策**:

```bash
# defconfig に追加
CONFIG_SPECIFIC_DRIVERS=y
```

この設定がないと、ボードレベルのカメラ初期化 (`isx012_initialize()`, `cxd56_cisif_initialize()`) が実行されません。

---

### エラー 3: Hard Fault (アンアライメントメモリアクセス)

**症状**:
```
arm_hardfault: PANIC!!! Hard Fault!:
arm_hardfault:  CFSR: 00040000
[CAM] Opening video device: /dev/video
```

**原因**: `V4L2_MEMORY_USERPTR` モードでバッファを正しく割り当てていない

**解決策**:

V4L2 USERPTR モードでは、**32バイトアラインメント**でバッファを割り当てる必要があります：

```c
// ❌ 間違い
struct v4l2_buffer buf;
buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
buf.memory = V4L2_MEMORY_USERPTR;
buf.index = i;
// m.userptr と length が未設定！
ioctl(fd, VIDIOC_QBUF, &buf);

// ✅ 正しい
void *buffer = memalign(32, bufsize);  // 32バイトアラインメント
if (buffer == NULL) {
    // エラー処理
}

struct v4l2_buffer buf;
buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
buf.memory = V4L2_MEMORY_USERPTR;
buf.index = i;
buf.m.userptr = (unsigned long)buffer;  // ⭐ バッファアドレス設定
buf.length = bufsize;                   // ⭐ バッファサイズ設定
ioctl(fd, VIDIOC_QBUF, &buf);
```

**参考実装**: `camera_manager.c` 219-270行目

---

### エラー 4: `/dev/video` デバイスが存在しない

**症状**:
```bash
nsh> ls /dev
/dev:
 console
 i2c0
 i2c2      # ← i2c2 はあるが video がない
 ...
```

**原因チェックリスト**:

1. ✅ `CONFIG_CXD56_CISIF=y` が設定されているか？
2. ✅ `CONFIG_CXD56_I2C2=y` が設定されているか？
3. ✅ `CONFIG_SPECIFIC_DRIVERS=y` が設定されているか？
4. ✅ カメラボードが物理的に正しく接続されているか？

**確認方法**:

```bash
# NuttX .config を確認
grep "CONFIG_CXD56_CISIF\|CONFIG_CXD56_I2C2\|CONFIG_SPECIFIC_DRIVERS" \
  /home/ken/Spr_ws/spresense/nuttx/.config

# 期待される出力:
# CONFIG_CXD56_CISIF=y
# CONFIG_CXD56_I2C2=y
# CONFIG_SPECIFIC_DRIVERS=y
```

---

### エラー 5: `Failed to queue buffer X: 12 (ENOMEM)`

**症状**:
```
[CAM] Camera buffers requested: 3
[CAM] Allocating 5 buffers of 65536 bytes each
[CAM] Failed to queue buffer 3: 12
[CAM] Failed to initialize camera manager: -3
```

**原因**: V4L2ドライバーの制限

SpresenseのISX012/ISX019カメラドライバーは、`V4L2_BUF_MODE_RING`モードで**最大3バッファ**しかサポートしていません。

**詳細**:
1. アプリケーションが `VIDIOC_REQBUFS` で5バッファを要求
2. ドライバーが `req.count = 3` に変更して返す（ドライバーの最大値）
3. アプリケーションが5個のバッファをキューイング（VIDIOC_QBUF）しようとする
4. バッファ#3, #4のキューイングが失敗（ENOMEM エラー12）

**解決策**:

ドライバーが返す実際のバッファ数を使用するようにコードを修正：

```c
// ❌ 間違い: ハードコードされた定数を使用
#define CAMERA_BUFFER_NUM 5
for (i = 0; i < CAMERA_BUFFER_NUM; i++) {
    // バッファをキューイング
}

// ✅ 正しい: ドライバーが返した実際の数を使用
struct v4l2_requestbuffers req;
req.count = 5;  // 希望する数を要求
ioctl(fd, VIDIOC_REQBUFS, &req);

// ドライバーが req.count を実際にサポートする数に変更
uint32_t actual_buffer_count = req.count;  // 3が返される

for (i = 0; i < actual_buffer_count; i++) {  // 3個のみキューイング
    // バッファをキューイング
}
```

**重要**: この制限により、5バッファ設計による性能向上は実現できません。ドライバーレベルの変更が必要です。

**参考実装**: `camera_manager.c:246-254`

**発見日**: 2025-12-29 (Phase 1.5)

---

## 設定関連の問題

### 問題 1: defconfig に設定を追加したが反映されない

**原因**: `./tools/config.py default` を実行していない

**解決策**:

defconfig を編集した後、**必ず** 以下を実行：

```bash
cd /home/ken/Spr_ws/spresense/sdk
./tools/config.py default  # ⭐ これが必須
make
```

`config.py` は defconfig の設定を NuttX/.config に適用します。この手順を省略すると、設定が反映されません。

---

### 問題 2: どの .config ファイルを見るべきか？

Spresense には **2つの .config ファイル**があります：

| ファイル | サイズ | 説明 |
|---------|--------|------|
| `sdk/.config` | 295-400B | **使われない** (小さい) |
| `nuttx/.config` | 60-70KB | **実際に使われる** (大きい) |

**正しい確認方法**:
```bash
# ✅ 正しい
grep "CONFIG_XXX" /home/ken/Spr_ws/spresense/nuttx/.config

# ❌ 間違い
grep "CONFIG_XXX" /home/ken/Spr_ws/spresense/sdk/.config
```

---

## ビルド関連の問題

### 問題 1: アプリが `help` に表示されない

**原因**: `Make.defs` のパス形式が間違っている

**解決策**:

```makefile
# ✅ 正しい
CONFIGURED_APPS += examples/security_camera

# ❌ 間違い
CONFIGURED_APPS += $(APPDIR)/examples/security_camera
CONFIGURED_APPS += apps/examples/security_camera
```

**参考**: `/home/ken/Spr_ws/case_study/prompts/build_rules/01_BUILD_RULES.md`

---

### 問題 2: arm-none-eabi-gcc not found

**原因**: PATH が設定されていない

**解決策**:

```bash
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin:$PATH
```

**確認方法**:
```bash
which arm-none-eabi-gcc
# → /home/ken/spresenseenv/usr/bin/arm-none-eabi-gcc
```

---

## 実装上の注意点

### 注意 1: video_initialize() の必須性

**必ず** カメラデバイスをオープンする前に `video_initialize()` を呼び出す：

```c
// ✅ 正しい順序
ret = video_initialize("/dev/video");  // 1. デバイス初期化
if (ret < 0) {
    // エラー処理
}

fd = open("/dev/video", O_RDONLY);     // 2. デバイスオープン
if (fd < 0) {
    // エラー処理
}
```

---

### 注意 2: カメラバッファのアラインメント

Spresense のカメラドライバは **32バイトアラインメント**を要求します：

```c
// ✅ 正しい
void *buf = memalign(32, size);

// ❌ 間違い
void *buf = malloc(size);  // アラインメント保証なし
```

---

### 注意 3: カメラボード検出

使用しているカメラボード: **CXD5602PWBCAM2W** (ISX012 センサー)

**設定**:
- `CONFIG_VIDEO_ISX012=y` を使用
- `CONFIG_VIDEO_ISX019=y` は不要（別のセンサー）

---

### 注意 4: V4L2バッファ数のネゴシエーション

**重要**: `VIDIOC_REQBUFS` で要求したバッファ数は、必ずしもドライバーに受け入れられるとは限りません。

**正しい実装パターン**:

```c
struct v4l2_requestbuffers req;
req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
req.memory = V4L2_MEMORY_USERPTR;
req.count = 5;  // 希望する数を要求
req.mode = V4L2_BUF_MODE_RING;

ret = ioctl(fd, VIDIOC_REQBUFS, &req);
if (ret < 0) {
    // エラー処理
}

// ✅ ドライバーが実際に許可したバッファ数を使用
uint32_t actual_count = req.count;  // ドライバーが変更した値
LOG_INFO("Requested: 5, Driver returned: %u", actual_count);

// actual_count を使ってバッファを割り当て
for (int i = 0; i < actual_count; i++) {
    // バッファ処理
}
```

**Spresense制限**: ISX012/ISX019ドライバーはRINGモードで最大3バッファまで

---

## デバッグ手順

### 手順 1: 公式 camera サンプルでテスト

まず、公式の camera サンプルが動作するか確認：

```bash
# defconfig に追加
CONFIG_EXAMPLES_CAMERA=y

# ビルド・フラッシュ
./tools/config.py examples/camera
make
sudo tools/flash.sh -c /dev/ttyUSB0 nuttx.spk

# テスト
nsh> camera
```

これが動作すれば、ハードウェアと基本設定は正常です。

---

### 手順 2: /dev/ デバイス確認

```bash
nsh> ls /dev
```

**期待される出力**:
- `i2c2` - I2C2バスが有効
- `video` - カメラドライバが初期化成功

---

### 手順 3: ビルド内容確認

```bash
# アプリがバイナリに含まれているか確認
arm-none-eabi-nm /home/ken/Spr_ws/spresense/nuttx/nuttx | grep security_camera
# → security_camera_main が表示されること
```

---

## よくある質問

### Q1: カメラボードは接続しているのに認識されない

**A**: 以下を確認：

1. カメラボードの接続方向は正しいか？
2. Spresense メインボードの電源は十分か？（カメラは電力を消費）
3. `CONFIG_CXD56_I2C2=y` が有効か？
4. `CONFIG_SPECIFIC_DRIVERS=y` が有効か？

---

### Q2: camera サンプルは動くが、security_camera は動かない

**A**: 実装の違いを確認：

1. バッファ割り当て方法（32バイトアライメント）
2. V4L2 ioctl の引数設定
3. ログ出力で初期化の進行状況を確認

---

### Q3: ビルドは成功するが、フラッシュ後にクラッシュする

**A**: 以下をチェック：

1. スタックサイズは十分か？（`CONFIG_EXAMPLES_SECURITY_CAMERA_STACKSIZE=8192`）
2. メモリリークはないか？
3. バッファの境界チェックは正しいか？

---

## まとめ

### 必須設定チェックリスト

- [ ] `CONFIG_CXD56_CISIF=y`
- [ ] `CONFIG_CXD56_I2C2=y` ⭐
- [ ] `CONFIG_SPECIFIC_DRIVERS=y` ⭐
- [ ] `CONFIG_VIDEO=y`
- [ ] `CONFIG_VIDEO_ISX012=y`
- [ ] `CONFIG_VIDEO_STREAM=y`
- [ ] `CONFIG_DRIVERS_VIDEO=y`
- [ ] `./tools/config.py default` を実行 ⭐

### 実装チェックリスト

- [ ] `video_initialize()` を呼び出している
- [ ] バッファを `memalign(32, size)` で割り当てている
- [ ] `buf.m.userptr` と `buf.length` を設定している
- [ ] クリーンアップ時にバッファを `free()` している

---

**作成者**: Claude Code (Sonnet 4.5)
**最終更新**: 2025-12-16
