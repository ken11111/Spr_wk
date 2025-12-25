/****************************************************************************
 * security_camera/camera_manager.c
 *
 *   Copyright 2025 Security Camera Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <syslog.h>

#include <nuttx/video/video.h>

#include "camera_manager.h"
#include "config.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define VIDEO_DEVICE_PATH  "/dev/video"
#define CAMERA_BUFFER_NUM  3  /* Triple buffering (Phase 1.5: frame drop mitigation) */

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct camera_buffer_s
{
  void *start;                     /* Buffer start address */
  uint32_t length;                 /* Buffer length */
};

struct camera_manager_s
{
  int fd;                          /* Video device file descriptor */
  camera_config_t config;          /* Camera configuration */
  struct v4l2_buffer buffers[CAMERA_BUFFER_NUM];
  struct camera_buffer_s mem[CAMERA_BUFFER_NUM];  /* Allocated buffers */
  uint32_t frame_count;            /* Frame counter */
  bool initialized;                /* Initialization flag */
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct camera_manager_s g_camera_mgr;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: get_timestamp_us
 *
 * Description:
 *   Get current timestamp in microseconds
 *
 ****************************************************************************/

static uint64_t get_timestamp_us(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: camera_manager_init
 *
 * Description:
 *   Initialize camera manager
 *
 ****************************************************************************/

int camera_manager_init(const camera_config_t *config)
{
  int ret;
  struct v4l2_format fmt;
  struct v4l2_requestbuffers req;
  struct v4l2_streamparm parm;
  int i;

  if (config == NULL)
    {
      LOG_ERROR("Invalid camera config");
      return ERR_CAMERA_INIT;
    }

  if (g_camera_mgr.initialized)
    {
      LOG_WARN("Camera already initialized");
      return ERR_OK;
    }

  memset(&g_camera_mgr, 0, sizeof(struct camera_manager_s));
  memcpy(&g_camera_mgr.config, config, sizeof(camera_config_t));

  /* Initialize video driver */

  LOG_INFO("Initializing video driver: %s", VIDEO_DEVICE_PATH);
  ret = video_initialize(VIDEO_DEVICE_PATH);
  if (ret < 0)
    {
      LOG_ERROR("Failed to initialize video driver: %d", ret);
      return ERR_CAMERA_INIT;
    }

  /* Open video device */

  LOG_INFO("Opening video device: %s", VIDEO_DEVICE_PATH);
  g_camera_mgr.fd = open(VIDEO_DEVICE_PATH, O_RDONLY);
  if (g_camera_mgr.fd < 0)
    {
      LOG_ERROR("Failed to open video device: %d", errno);
      return ERR_CAMERA_OPEN;
    }

  /* Set format - Using VIDEO_CAPTURE for continuous JPEG streaming */

  memset(&fmt, 0, sizeof(struct v4l2_format));
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width = config->width;
  fmt.fmt.pix.height = config->height;
  fmt.fmt.pix.pixelformat = config->format;
  fmt.fmt.pix.field = V4L2_FIELD_ANY;

  ret = ioctl(g_camera_mgr.fd, VIDIOC_S_FMT, (uintptr_t)&fmt);
  if (ret < 0)
    {
      LOG_ERROR("Failed to set format: %d", errno);
      close(g_camera_mgr.fd);
      return ERR_CAMERA_CONFIG;
    }

  LOG_INFO("Camera format set: %dx%d", config->width, config->height);
  LOG_INFO("Driver returned sizeimage: %u bytes (format=0x%08x)",
           fmt.fmt.pix.sizeimage, fmt.fmt.pix.pixelformat);

  /* Calculate buffer size if driver didn't provide it */

  if (fmt.fmt.pix.sizeimage == 0)
    {
      /* Manual calculation based on format */

      if (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_JPEG)
        {
          /* JPEG size varies with compression, use 64KB for QVGA */
          fmt.fmt.pix.sizeimage = 65536;  /* 64 * 1024 */
        }
      else if (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_RGB565 ||
               fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_UYVY)
        {
          /* RGB565 and UYVY both use 2 bytes per pixel */
          fmt.fmt.pix.sizeimage = fmt.fmt.pix.width * fmt.fmt.pix.height * 2;
        }
      else
        {
          LOG_ERROR("Unsupported pixel format: 0x%08x",
                    fmt.fmt.pix.pixelformat);
          close(g_camera_mgr.fd);
          return ERR_CAMERA_CONFIG;
        }

      LOG_INFO("Calculated sizeimage: %u bytes (driver returned 0)",
               fmt.fmt.pix.sizeimage);
    }

  /* Set frame rate */

  memset(&parm, 0, sizeof(struct v4l2_streamparm));
  parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  parm.parm.capture.timeperframe.numerator = 1;
  parm.parm.capture.timeperframe.denominator = config->fps;

  ret = ioctl(g_camera_mgr.fd, VIDIOC_S_PARM, (uintptr_t)&parm);
  if (ret < 0)
    {
      LOG_ERROR("Failed to set frame rate: %d", errno);
      close(g_camera_mgr.fd);
      return ERR_CAMERA_CONFIG;
    }

  LOG_INFO("Camera FPS set: %d", config->fps);

  /* Request buffers - Using RING mode for continuous video streaming */

  memset(&req, 0, sizeof(struct v4l2_requestbuffers));
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_USERPTR;
  req.count = CAMERA_BUFFER_NUM;
  req.mode = V4L2_BUF_MODE_RING;

  ret = ioctl(g_camera_mgr.fd, VIDIOC_REQBUFS, (uintptr_t)&req);
  if (ret < 0)
    {
      LOG_ERROR("Failed to request buffers: %d", errno);
      close(g_camera_mgr.fd);
      return ERR_CAMERA_CONFIG;
    }

  LOG_INFO("Camera buffers requested: %d", req.count);

  /* Allocate and queue buffers */

  uint32_t bufsize = fmt.fmt.pix.sizeimage;
  LOG_INFO("Allocating %d buffers of %u bytes each", CAMERA_BUFFER_NUM, bufsize);

  for (i = 0; i < CAMERA_BUFFER_NUM; i++)
    {
      /* Allocate 32-byte aligned buffer */

      g_camera_mgr.mem[i].start = memalign(32, bufsize);
      if (g_camera_mgr.mem[i].start == NULL)
        {
          LOG_ERROR("Failed to allocate buffer %d", i);

          /* Free previously allocated buffers */

          while (i > 0)
            {
              i--;
              free(g_camera_mgr.mem[i].start);
            }

          close(g_camera_mgr.fd);
          return ERR_CAMERA_CONFIG;
        }

      g_camera_mgr.mem[i].length = bufsize;

      /* Queue buffer */

      memset(&g_camera_mgr.buffers[i], 0, sizeof(struct v4l2_buffer));
      g_camera_mgr.buffers[i].type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      g_camera_mgr.buffers[i].memory = V4L2_MEMORY_USERPTR;
      g_camera_mgr.buffers[i].index = i;
      g_camera_mgr.buffers[i].m.userptr = (unsigned long)g_camera_mgr.mem[i].start;
      g_camera_mgr.buffers[i].length = g_camera_mgr.mem[i].length;

      ret = ioctl(g_camera_mgr.fd, VIDIOC_QBUF,
                  (uintptr_t)&g_camera_mgr.buffers[i]);
      if (ret < 0)
        {
          LOG_ERROR("Failed to queue buffer %d: %d", i, errno);

          /* Free all allocated buffers */

          for (int j = 0; j <= i; j++)
            {
              free(g_camera_mgr.mem[j].start);
            }

          close(g_camera_mgr.fd);
          return ERR_CAMERA_CONFIG;
        }
    }

  /* Start streaming */

  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  ret = ioctl(g_camera_mgr.fd, VIDIOC_STREAMON, (uintptr_t)&type);
  if (ret < 0)
    {
      LOG_ERROR("Failed to start streaming: %d", errno);
      close(g_camera_mgr.fd);
      return ERR_CAMERA_INIT;
    }

  LOG_INFO("Camera streaming started");

  g_camera_mgr.initialized = true;
  g_camera_mgr.frame_count = 0;

  return ERR_OK;
}

/****************************************************************************
 * Name: camera_get_frame
 *
 * Description:
 *   Get frame from camera (blocking)
 *
 ****************************************************************************/

int camera_get_frame(camera_frame_t *frame)
{
  int ret;
  struct v4l2_buffer buf;
  struct pollfd fds[1];

  if (!g_camera_mgr.initialized)
    {
      LOG_ERROR("Camera not initialized");
      return ERR_CAMERA_INIT;
    }

  if (frame == NULL)
    {
      LOG_ERROR("Invalid frame pointer");
      return ERR_CAMERA_CAPTURE;
    }

  /* Wait for frame */

  fds[0].fd = g_camera_mgr.fd;
  fds[0].events = POLLIN;

  ret = poll(fds, 1, 1000);  /* 1 second timeout */
  if (ret == 0)
    {
      LOG_WARN("Camera poll timeout");
      return ERR_TIMEOUT;
    }
  else if (ret < 0)
    {
      LOG_ERROR("Camera poll error: %d", errno);
      return ERR_CAMERA_CAPTURE;
    }

  /* Dequeue buffer */

  memset(&buf, 0, sizeof(struct v4l2_buffer));
  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_USERPTR;

  ret = ioctl(g_camera_mgr.fd, VIDIOC_DQBUF, (uintptr_t)&buf);
  if (ret < 0)
    {
      LOG_ERROR("Failed to dequeue buffer: %d", errno);
      return ERR_CAMERA_CAPTURE;
    }

  /* Fill frame structure */

  frame->buf = (uint8_t *)buf.m.userptr;
  frame->size = buf.bytesused;
  frame->timestamp_us = get_timestamp_us();
  frame->frame_num = g_camera_mgr.frame_count++;

  /* Re-queue buffer */

  ret = ioctl(g_camera_mgr.fd, VIDIOC_QBUF, (uintptr_t)&buf);
  if (ret < 0)
    {
      LOG_ERROR("Failed to queue buffer: %d", errno);
      return ERR_CAMERA_CAPTURE;
    }

  return ERR_OK;
}

/****************************************************************************
 * Name: camera_manager_cleanup
 *
 * Description:
 *   Cleanup camera manager
 *
 ****************************************************************************/

int camera_manager_cleanup(void)
{
  enum v4l2_buf_type type;
  int i;

  if (!g_camera_mgr.initialized)
    {
      return ERR_OK;
    }

  /* Stop streaming */

  type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  ioctl(g_camera_mgr.fd, VIDIOC_STREAMOFF, (uintptr_t)&type);

  /* Free allocated buffers */

  for (i = 0; i < CAMERA_BUFFER_NUM; i++)
    {
      if (g_camera_mgr.mem[i].start != NULL)
        {
          free(g_camera_mgr.mem[i].start);
          g_camera_mgr.mem[i].start = NULL;
        }
    }

  /* Close device */

  if (g_camera_mgr.fd >= 0)
    {
      close(g_camera_mgr.fd);
      g_camera_mgr.fd = -1;
    }

  g_camera_mgr.initialized = false;

  LOG_INFO("Camera manager cleaned up");

  return ERR_OK;
}
