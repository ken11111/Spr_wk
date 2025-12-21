/****************************************************************************
 * security_camera/encoder_manager.c
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
#include <errno.h>
#include <sys/ioctl.h>
#include <syslog.h>

#include <nuttx/video/video.h>

#include "encoder_manager.h"
#include "config.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define VIDEO_ENCODER_PATH  "/dev/video1"
#define MAX_NAL_UNITS       4  /* SPS, PPS, IDR/SLICE */

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct encoder_manager_s
{
  int fd;                          /* Video encoder device file descriptor */
  encoder_config_t config;         /* Encoder configuration */
  bool initialized;                /* Initialization flag */
  uint32_t frame_count;            /* Frame counter */
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct encoder_manager_s g_encoder_mgr;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: get_nal_type
 *
 * Description:
 *   Get NAL unit type from the first byte
 *
 ****************************************************************************/

static uint8_t get_nal_type(const uint8_t *data, uint32_t size)
{
  if (data == NULL || size < 1)
    {
      return 0;
    }

  /* NAL unit type is in the lower 5 bits of the first byte */
  return data[0] & 0x1F;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: encoder_manager_init
 *
 * Description:
 *   Initialize encoder manager
 *
 ****************************************************************************/

int encoder_manager_init(const encoder_config_t *config)
{
  int ret;
  struct v4l2_format fmt;

  if (config == NULL)
    {
      LOG_ERROR("Invalid encoder config");
      return ERR_ENCODER_INIT;
    }

  if (g_encoder_mgr.initialized)
    {
      LOG_WARN("Encoder already initialized");
      return ERR_OK;
    }

  memset(&g_encoder_mgr, 0, sizeof(struct encoder_manager_s));
  memcpy(&g_encoder_mgr.config, config, sizeof(encoder_config_t));

  /* Open video encoder device */

  LOG_INFO("Opening video encoder device: %s", VIDEO_ENCODER_PATH);
  g_encoder_mgr.fd = open(VIDEO_ENCODER_PATH, O_RDWR);
  if (g_encoder_mgr.fd < 0)
    {
      LOG_ERROR("Failed to open video encoder: %d", errno);
      return ERR_ENCODER_OPEN;
    }

  /* Set encoder format */

  memset(&fmt, 0, sizeof(struct v4l2_format));
  fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  fmt.fmt.pix.width = config->width;
  fmt.fmt.pix.height = config->height;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;  /* YUV422 input */
  fmt.fmt.pix.field = V4L2_FIELD_ANY;

  ret = ioctl(g_encoder_mgr.fd, VIDIOC_S_FMT, (unsigned long)&fmt);
  if (ret < 0)
    {
      LOG_ERROR("Failed to set encoder input format: %d", errno);
      close(g_encoder_mgr.fd);
      return ERR_ENCODER_CONFIG;
    }

  /* Set encoder output format (H.264) */

  memset(&fmt, 0, sizeof(struct v4l2_format));
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width = config->width;
  fmt.fmt.pix.height = config->height;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
  fmt.fmt.pix.field = V4L2_FIELD_ANY;

  ret = ioctl(g_encoder_mgr.fd, VIDIOC_S_FMT, (unsigned long)&fmt);
  if (ret < 0)
    {
      LOG_ERROR("Failed to set encoder output format: %d", errno);
      close(g_encoder_mgr.fd);
      return ERR_ENCODER_CONFIG;
    }

  LOG_INFO("Encoder initialized: %dx%d, H.264, %d bps",
           config->width, config->height, config->bitrate);

  g_encoder_mgr.initialized = true;
  g_encoder_mgr.frame_count = 0;

  return ERR_OK;
}

/****************************************************************************
 * Name: encoder_encode_frame
 *
 * Description:
 *   Encode YUV frame to H.264
 *
 ****************************************************************************/

int encoder_encode_frame(const camera_frame_t *yuv_frame,
                         h264_nal_unit_t *nal_units,
                         int max_nal_count)
{
  int ret;
  ssize_t written;
  ssize_t read_size;
  uint8_t *encode_buf;
  const size_t encode_buf_size = 64 * 1024;  /* 64KB buffer */
  int nal_count = 0;

  if (!g_encoder_mgr.initialized)
    {
      LOG_ERROR("Encoder not initialized");
      return ERR_ENCODER_INIT;
    }

  if (yuv_frame == NULL || nal_units == NULL || max_nal_count < 1)
    {
      LOG_ERROR("Invalid parameters");
      return ERR_ENCODER_ENCODE;
    }

  /* Allocate temporary buffer for encoded data */

  encode_buf = (uint8_t *)malloc(encode_buf_size);
  if (encode_buf == NULL)
    {
      LOG_ERROR("Failed to allocate encode buffer");
      return ERR_NOMEM;
    }

  /* Write YUV data to encoder */

  written = write(g_encoder_mgr.fd, yuv_frame->buf, yuv_frame->size);
  if (written < 0)
    {
      LOG_ERROR("Failed to write to encoder: %d", errno);
      free(encode_buf);
      return ERR_ENCODER_ENCODE;
    }

  /* Read encoded H.264 data */

  read_size = read(g_encoder_mgr.fd, encode_buf, encode_buf_size);
  if (read_size < 0)
    {
      LOG_ERROR("Failed to read from encoder: %d", errno);
      free(encode_buf);
      return ERR_ENCODER_ENCODE;
    }
  else if (read_size == 0)
    {
      /* No data available yet (might happen for first few frames) */
      free(encode_buf);
      return 0;
    }

  /* Parse NAL units from the encoded data
   * Note: This is a simplified version. In reality, we need to properly
   * parse the H.264 stream to extract individual NAL units.
   * For now, we treat the entire encoded buffer as one NAL unit.
   */

  if (nal_count < max_nal_count)
    {
      nal_units[nal_count].data = (uint8_t *)malloc(read_size);
      if (nal_units[nal_count].data == NULL)
        {
          LOG_ERROR("Failed to allocate NAL unit buffer");
          free(encode_buf);
          return ERR_NOMEM;
        }

      memcpy(nal_units[nal_count].data, encode_buf, read_size);
      nal_units[nal_count].size = read_size;
      nal_units[nal_count].type = get_nal_type(encode_buf, read_size);
      nal_units[nal_count].timestamp_us = yuv_frame->timestamp_us;
      nal_units[nal_count].frame_num = yuv_frame->frame_num;

      nal_count++;

      LOG_DEBUG("Encoded NAL unit: type=%d, size=%d",
                nal_units[0].type, nal_units[0].size);
    }

  free(encode_buf);

  g_encoder_mgr.frame_count++;

  return nal_count;
}

/****************************************************************************
 * Name: encoder_manager_cleanup
 *
 * Description:
 *   Cleanup encoder manager
 *
 ****************************************************************************/

int encoder_manager_cleanup(void)
{
  if (!g_encoder_mgr.initialized)
    {
      return ERR_OK;
    }

  /* Close encoder device */

  if (g_encoder_mgr.fd >= 0)
    {
      close(g_encoder_mgr.fd);
      g_encoder_mgr.fd = -1;
    }

  g_encoder_mgr.initialized = false;

  LOG_INFO("Encoder manager cleaned up");

  return ERR_OK;
}
