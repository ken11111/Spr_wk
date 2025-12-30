/****************************************************************************
 * security_camera/frame_queue.h
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

#ifndef __SECURITY_CAMERA_FRAME_QUEUE_H
#define __SECURITY_CAMERA_FRAME_QUEUE_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define MAX_QUEUE_DEPTH 3  /* Maximum number of buffers in queue */

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* Frame buffer structure for pipelined threading */

typedef struct frame_buffer_s
{
  void *data;              /* Buffer pointer (32-byte aligned) */
  uint32_t length;         /* Buffer capacity */
  uint32_t used;           /* Actual data length */
  int id;                  /* Buffer index */
  struct frame_buffer_s *next;  /* Linked list pointer */
} frame_buffer_t;

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

/* Global queues (defined in frame_queue.c) */

EXTERN frame_buffer_t *g_action_queue;  /* Filled frames (camera → USB) */
EXTERN frame_buffer_t *g_empty_queue;   /* Empty buffers (USB → camera) */

/* Synchronization primitives (defined in frame_queue.c) */

EXTERN pthread_mutex_t g_queue_mutex;
EXTERN pthread_cond_t g_queue_cond;
EXTERN volatile bool g_shutdown_requested;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * @brief Initialize frame queue system
 * @return 0: success, <0: error
 */

int frame_queue_init(void);

/**
 * @brief Cleanup frame queue system
 */

void frame_queue_cleanup(void);

/**
 * @brief Push buffer to queue (caller must hold mutex)
 * @param queue Pointer to queue head
 * @param buf Buffer to push
 */

void frame_queue_push(frame_buffer_t **queue, frame_buffer_t *buf);

/**
 * @brief Pull buffer from queue (caller must hold mutex)
 * @param queue Pointer to queue head
 * @return Buffer pointer, NULL if empty
 */

frame_buffer_t *frame_queue_pull(frame_buffer_t **queue);

/**
 * @brief Check if queue is empty (caller must hold mutex)
 * @param queue Queue head
 * @return true: empty, false: has buffers
 */

bool frame_queue_is_empty(frame_buffer_t *queue);

/**
 * @brief Get queue depth (caller must hold mutex)
 * @param queue Queue head
 * @return Number of buffers in queue
 */

int frame_queue_depth(frame_buffer_t *queue);

/**
 * @brief Allocate buffer pool for pipelining
 * @param buffer_size Size of each buffer
 * @param buffer_count Number of buffers to allocate
 * @return 0: success, <0: error
 */

int frame_queue_allocate_buffers(uint32_t buffer_size, int buffer_count);

/**
 * @brief Free buffer pool
 */

void frame_queue_free_buffers(void);

#undef EXTERN
#ifdef __cplusplus
}
#endif

#endif /* __SECURITY_CAMERA_FRAME_QUEUE_H */
