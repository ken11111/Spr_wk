/****************************************************************************
 * security_camera/encoder_manager.h
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

#ifndef __SECURITY_CAMERA_ENCODER_MANAGER_H
#define __SECURITY_CAMERA_ENCODER_MANAGER_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include "camera_manager.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* NAL Unit type definitions */

#define NAL_TYPE_SPS      7    /* Sequence Parameter Set */
#define NAL_TYPE_PPS      8    /* Picture Parameter Set */
#define NAL_TYPE_IDR      5    /* IDR (I-frame) */
#define NAL_TYPE_SLICE    1    /* P-frame */

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* Encoder configuration structure */

typedef struct encoder_config_s
{
  uint16_t width;              /* Encode width */
  uint16_t height;             /* Encode height */
  uint32_t bitrate;            /* Bitrate (e.g., 2000000 = 2Mbps) */
  uint8_t  fps;                /* Frame rate */
  uint8_t  gop_size;           /* GOP size (e.g., 30) */
  uint8_t  profile;            /* H.264 profile (Baseline) */
} encoder_config_t;

/* H.264 NAL Unit structure */

typedef struct h264_nal_unit_s
{
  uint8_t  *data;              /* NAL Unit data */
  uint32_t size;               /* NAL Unit size */
  uint8_t  type;               /* NAL Unit type (I/P/SPS/PPS) */
  uint64_t timestamp_us;       /* Timestamp */
  uint32_t frame_num;          /* Frame number */
} h264_nal_unit_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Initialize encoder manager
 * @param config Encoder configuration
 * @return 0: success, <0: error
 */

int encoder_manager_init(const encoder_config_t *config);

/**
 * @brief Encode YUV frame to H.264
 * @param yuv_frame Input YUV frame
 * @param nal_units Output NAL Unit array (caller must provide)
 * @param max_nal_count Maximum NAL Unit count
 * @return Number of NAL Units encoded, <0: error
 */

int encoder_encode_frame(const camera_frame_t *yuv_frame,
                         h264_nal_unit_t *nal_units,
                         int max_nal_count);

/**
 * @brief Cleanup encoder manager
 * @return 0: success, <0: error
 */

int encoder_manager_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* __SECURITY_CAMERA_ENCODER_MANAGER_H */
