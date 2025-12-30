/****************************************************************************
 * security_camera/frame_queue.c
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
#include <errno.h>
#include <syslog.h>

#include "frame_queue.h"
#include "config.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Buffer pool */

static frame_buffer_t *g_buffer_pool = NULL;  /* Array of buffer structures */
static int g_buffer_pool_size = 0;            /* Number of buffers allocated */

/****************************************************************************
 * Public Data
 ****************************************************************************/

/* Frame queues */

frame_buffer_t *g_action_queue = NULL;  /* Filled frames (camera → USB) */
frame_buffer_t *g_empty_queue = NULL;   /* Empty buffers (USB → camera) */

/* Synchronization primitives */

pthread_mutex_t g_queue_mutex;
pthread_cond_t g_queue_cond;
volatile bool g_shutdown_requested = false;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: frame_queue_init
 *
 * Description:
 *   Initialize frame queue system
 *
 ****************************************************************************/

int frame_queue_init(void)
{
  int ret;
  pthread_mutexattr_t mutex_attr;

  /* Initialize queues to empty */

  g_action_queue = NULL;
  g_empty_queue = NULL;
  g_shutdown_requested = false;

  /* Initialize mutex with priority inheritance (if supported) */

  ret = pthread_mutexattr_init(&mutex_attr);
  if (ret != 0)
    {
      LOG_ERROR("Failed to init mutex attributes: %d", ret);
      return -ret;
    }

  /* Try to enable priority inheritance (optional - may not be supported) */

  ret = pthread_mutexattr_setprotocol(&mutex_attr, PTHREAD_PRIO_INHERIT);
  if (ret != 0)
    {
      LOG_WARN("Priority inheritance not supported (error %d), continuing without it", ret);
      LOG_INFO("Thread priorities (110 vs 100) will help prevent priority inversion");
      /* Continue anyway - priority difference should be sufficient */
    }
  else
    {
      LOG_INFO("Priority inheritance mutex enabled");
    }

  ret = pthread_mutex_init(&g_queue_mutex, &mutex_attr);
  pthread_mutexattr_destroy(&mutex_attr);
  if (ret != 0)
    {
      LOG_ERROR("Failed to init queue mutex: %d", ret);
      return -ret;
    }

  /* Initialize condition variable */

  ret = pthread_cond_init(&g_queue_cond, NULL);
  if (ret != 0)
    {
      LOG_ERROR("Failed to init queue condition: %d", ret);
      pthread_mutex_destroy(&g_queue_mutex);
      return -ret;
    }

  LOG_INFO("Frame queue system initialized");
  return 0;
}

/****************************************************************************
 * Name: frame_queue_cleanup
 *
 * Description:
 *   Cleanup frame queue system
 *
 ****************************************************************************/

void frame_queue_cleanup(void)
{
  /* Set shutdown flag */

  pthread_mutex_lock(&g_queue_mutex);
  g_shutdown_requested = true;
  pthread_cond_broadcast(&g_queue_cond);  /* Wake all waiting threads */
  pthread_mutex_unlock(&g_queue_mutex);

  /* Free buffer pool */

  frame_queue_free_buffers();

  /* Destroy synchronization primitives */

  pthread_cond_destroy(&g_queue_cond);
  pthread_mutex_destroy(&g_queue_mutex);

  /* Clear queues */

  g_action_queue = NULL;
  g_empty_queue = NULL;

  LOG_INFO("Frame queue system cleaned up");
}

/****************************************************************************
 * Name: frame_queue_push
 *
 * Description:
 *   Push buffer to queue tail (FIFO)
 *   Caller must hold g_queue_mutex
 *
 ****************************************************************************/

void frame_queue_push(frame_buffer_t **queue, frame_buffer_t *buf)
{
  frame_buffer_t *current;

  if (buf == NULL)
    {
      return;
    }

  buf->next = NULL;

  if (*queue == NULL)
    {
      /* Queue is empty, this becomes the head */

      *queue = buf;
    }
  else
    {
      /* Find tail and append */

      current = *queue;
      while (current->next != NULL)
        {
          current = current->next;
        }

      current->next = buf;
    }
}

/****************************************************************************
 * Name: frame_queue_pull
 *
 * Description:
 *   Pull buffer from queue head (FIFO)
 *   Caller must hold g_queue_mutex
 *
 ****************************************************************************/

frame_buffer_t *frame_queue_pull(frame_buffer_t **queue)
{
  frame_buffer_t *buf;

  if (*queue == NULL)
    {
      return NULL;
    }

  /* Remove head */

  buf = *queue;
  *queue = buf->next;
  buf->next = NULL;

  return buf;
}

/****************************************************************************
 * Name: frame_queue_is_empty
 *
 * Description:
 *   Check if queue is empty
 *   Caller must hold g_queue_mutex
 *
 ****************************************************************************/

bool frame_queue_is_empty(frame_buffer_t *queue)
{
  return (queue == NULL);
}

/****************************************************************************
 * Name: frame_queue_depth
 *
 * Description:
 *   Get number of buffers in queue
 *   Caller must hold g_queue_mutex
 *
 ****************************************************************************/

int frame_queue_depth(frame_buffer_t *queue)
{
  int count = 0;
  frame_buffer_t *current = queue;

  while (current != NULL)
    {
      count++;
      current = current->next;
    }

  return count;
}

/****************************************************************************
 * Name: frame_queue_allocate_buffers
 *
 * Description:
 *   Allocate buffer pool for pipelining (Step 2)
 *
 ****************************************************************************/

int frame_queue_allocate_buffers(uint32_t buffer_size, int buffer_count)
{
  int i;

  if (buffer_count <= 0 || buffer_size == 0)
    {
      LOG_ERROR("Invalid buffer parameters: count=%d, size=%lu",
                buffer_count, (unsigned long)buffer_size);
      return -EINVAL;
    }

  /* Allocate buffer structure array */

  g_buffer_pool = (frame_buffer_t *)malloc(sizeof(frame_buffer_t) * buffer_count);
  if (g_buffer_pool == NULL)
    {
      LOG_ERROR("Failed to allocate buffer pool structures");
      return -ENOMEM;
    }

  memset(g_buffer_pool, 0, sizeof(frame_buffer_t) * buffer_count);
  g_buffer_pool_size = buffer_count;

  /* Allocate data buffers and initialize structures */

  for (i = 0; i < buffer_count; i++)
    {
      /* Allocate 32-byte aligned buffer for better performance */

      g_buffer_pool[i].data = memalign(32, buffer_size);
      if (g_buffer_pool[i].data == NULL)
        {
          LOG_ERROR("Failed to allocate buffer %d", i);

          /* Free previously allocated buffers */

          while (--i >= 0)
            {
              free(g_buffer_pool[i].data);
            }

          free(g_buffer_pool);
          g_buffer_pool = NULL;
          g_buffer_pool_size = 0;
          return -ENOMEM;
        }

      g_buffer_pool[i].length = buffer_size;
      g_buffer_pool[i].used = 0;
      g_buffer_pool[i].id = i;
      g_buffer_pool[i].next = NULL;
    }

  /* Add all buffers to empty queue */

  pthread_mutex_lock(&g_queue_mutex);
  for (i = 0; i < buffer_count; i++)
    {
      frame_queue_push(&g_empty_queue, &g_buffer_pool[i]);
    }

  pthread_mutex_unlock(&g_queue_mutex);

  LOG_INFO("Allocated %d buffers (%lu bytes each, total %lu KB)",
           buffer_count, (unsigned long)buffer_size,
           (unsigned long)(buffer_count * buffer_size) / 1024);

  return 0;
}

/****************************************************************************
 * Name: frame_queue_free_buffers
 *
 * Description:
 *   Free buffer pool
 *
 ****************************************************************************/

void frame_queue_free_buffers(void)
{
  int i;

  if (g_buffer_pool == NULL)
    {
      return;
    }

  /* Free data buffers */

  for (i = 0; i < g_buffer_pool_size; i++)
    {
      if (g_buffer_pool[i].data != NULL)
        {
          free(g_buffer_pool[i].data);
          g_buffer_pool[i].data = NULL;
        }
    }

  /* Free buffer structure array */

  free(g_buffer_pool);
  g_buffer_pool = NULL;
  g_buffer_pool_size = 0;

  LOG_INFO("Buffer pool freed");
}
