/****************************************************************************
 * security_camera/camera_threads.h
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

#ifndef __SECURITY_CAMERA_CAMERA_THREADS_H
#define __SECURITY_CAMERA_CAMERA_THREADS_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <pthread.h>
#include <stdbool.h>
#include "frame_queue.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define CAMERA_THREAD_PRIORITY  110  /* Higher priority for camera */
#define USB_THREAD_PRIORITY     100  /* Lower priority for USB */
#define THREAD_STACK_SIZE       4096 /* 4KB stack per thread */

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* Thread context structure (shared state between main and threads) */

typedef struct thread_context_s
{
  /* File descriptors */

  int camera_fd;          /* V4L2 camera file descriptor */
  int usb_fd;             /* USB CDC file descriptor */

  /* Buffer pool */

  frame_buffer_t *buffers;  /* Allocated buffer array */
  int buffer_count;         /* Number of buffers (typically 3) */

  /* Packet buffer for MJPEG packing */

  uint8_t *packet_buffer;   /* Single packet buffer (reused) */
  uint32_t packet_buffer_size;  /* Buffer capacity */

  /* Sequence number for MJPEG protocol */

  uint32_t *sequence;       /* Pointer to sequence counter */

} thread_context_t;

/****************************************************************************
 * Public Data
 ****************************************************************************/

#ifdef __cplusplus
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

/* Thread handles (defined in camera_threads.c) */

EXTERN pthread_t g_camera_thread;
EXTERN pthread_t g_usb_thread;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * @brief Initialize threading system and create threads
 * @param ctx Thread context with camera_fd, usb_fd, buffers, etc.
 * @return 0: success, <0: error
 */

int camera_threads_init(thread_context_t *ctx);

/**
 * @brief Cleanup threading system (join threads, free resources)
 */

void camera_threads_cleanup(void);

/**
 * @brief Camera thread function (producer)
 * @param arg Pointer to thread_context_t
 * @return NULL
 */

void *camera_thread_func(void *arg);

/**
 * @brief USB thread function (consumer)
 * @param arg Pointer to thread_context_t
 * @return NULL
 */

void *usb_thread_func(void *arg);

#undef EXTERN
#ifdef __cplusplus
}
#endif

#endif /* __SECURITY_CAMERA_CAMERA_THREADS_H */
