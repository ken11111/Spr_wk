/****************************************************************************
 * security_camera/camera_manager.h
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

#ifndef __SECURITY_CAMERA_CAMERA_MANAGER_H
#define __SECURITY_CAMERA_CAMERA_MANAGER_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <stdbool.h>

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* Camera configuration structure */

typedef struct camera_config_s
{
  uint16_t width;              /* Image width (e.g., 1280) */
  uint16_t height;             /* Image height (e.g., 720) */
  uint8_t  fps;                /* Frame rate (e.g., 30) */
  uint32_t format;             /* Image format (V4L2 fourcc) */
  bool     hdr_enable;         /* HDR enable/disable */
} camera_config_t;

/* Camera frame structure */

typedef struct camera_frame_s
{
  uint8_t  *buf;               /* Frame buffer pointer */
  uint32_t size;               /* Frame size in bytes */
  uint64_t timestamp_us;       /* Timestamp in microseconds */
  uint32_t frame_num;          /* Frame number */
} camera_frame_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Initialize camera manager
 * @param config Camera configuration
 * @return 0: success, <0: error
 */

int camera_manager_init(const camera_config_t *config);

/**
 * @brief Get frame from camera (blocking)
 * @param frame Output frame structure
 * @return 0: success, <0: error
 */

int camera_get_frame(camera_frame_t *frame);

/**
 * @brief Cleanup camera manager
 * @return 0: success, <0: error
 */

int camera_manager_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* __SECURITY_CAMERA_CAMERA_MANAGER_H */
