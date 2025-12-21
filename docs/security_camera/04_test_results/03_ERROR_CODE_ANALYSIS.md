# エラーコード分析

**作成日**: 2025-12-16

## 観察されているエラー

### 1. camera サンプル: `errno = 2` (ENOENT)
```
ERROR: Failed to initialize video: errno = 2
```

**意味**: ENOENT = "No such file or directory"
**発生条件**: `/dev/video` デバイスファイルが存在しない

### 2. security_camera: `video_initialize() returns -22` (EINVAL)
```
[CAM] Failed to initialize video driver: -22
```

**意味**: EINVAL = "Invalid argument"
**発生条件**: `g_video_data` が NULL（センサーが登録されていない）

---

## 初期化フロー

### 正常な初期化シーケンス

```
1. ボート起動
   ↓
2. cxd56_bringup.c が実行される
   ↓
3. #ifndef CONFIG_CXD56_CAMERA_LATE_INITIALIZE
   ↓
4. isx012_initialize() が呼ばれる
   ↓
5. imgsensor_register() でセンサーを登録
   ↓
6. cxd56_cisif_initialize() で CISIF を初期化
   ↓
7. アプリケーションが video_initialize() を呼ぶ
   ↓
8. video_register() が g_video_data をチェック
   ↓
9. /dev/video デバイスが作成される
```

### 現在の状態（失敗）

```
1. ボート起動
   ↓
2. cxd56_bringup.c が実行される
   ↓
3. カメラ初期化がスキップされる ❌
   （理由: 条件が満たされていない）
   ↓
4. g_video_data が NULL のまま
   ↓
5. アプリケーションが video_initialize() を呼ぶ
   ↓
6. video_register() が g_video_data == NULL を検出
   ↓
7. return -EINVAL (-22) ❌
```

---

## video_register() のエラー条件

**ソース**: `/home/ken/Spr_ws/spresense/nuttx/drivers/video/video.c:3437-3460`

```c
int video_register(FAR const char *devpath,
                   FAR struct imgdata_s *data,
                   FAR struct imgsensor_s **sensors,
                   size_t sensor_num)
{
  /* Input devpath Error Check */

  if (devpath == NULL || data == NULL)  // ← data が NULL なら -EINVAL
    {
      return -EINVAL;
    }
```

**現在の状況**:
- `devpath` = "/dev/video" ✓ (正常)
- `data` = `g_video_data` = NULL ❌ (センサー未登録)

---

## 根本原因

**cxd56_bringup.c の条件分岐**:

```c
#ifndef CONFIG_CXD56_CAMERA_LATE_INITIALIZE
#ifdef CONFIG_VIDEO_ISX012
  ret = isx012_initialize();  // ← これが実行されていない
#endif
#ifdef CONFIG_CXD56_CISIF
  ret = cxd56_cisif_initialize();
#endif
#endif
```

**問題点**:
1. `CONFIG_CXD56_CAMERA_LATE_INITIALIZE` が定義されているか？
2. `CONFIG_VIDEO_ISX012` が定義されているか？
3. 条件が複雑で、どれか1つでも満たされないと初期化がスキップされる

---

## 次のアクション

### 確認すべき設定

1. `CONFIG_CXD56_CAMERA_LATE_INITIALIZE` の状態
   - **期待**: 未設定 (is not set)
   - **理由**: 設定されていると初期化がスキップされる

2. `CONFIG_VIDEO_ISX012` の状態
   - **期待**: =y
   - **理由**: ISX012 センサーを使用している

3. `CONFIG_CXD56_CISIF` の状態
   - **期待**: =y
   - **理由**: カメラインターフェース必須

4. ビルドログの確認
   - `isx012_initialize()` が実際にコンパイルされているか
   - `cxd56_bringup.c` に初期化コードが含まれているか

---

**作成者**: Claude Code (Sonnet 4.5)
**最終更新**: 2025-12-16
