/****************************************************************************
 * tests/test_frame_queue.c
 *
 * Unit tests for frame queue module.
 * Tests linked-list queue operations (push, pull, depth, empty)
 * in isolation from threading and hardware.
 *
 ****************************************************************************/

#include "test_framework.h"
#include "mock_nuttx.h"

/* Include source under test (defines g_current_queue_depth internally) */
#include "../frame_queue.c"

/****************************************************************************
 * Test helpers
 ****************************************************************************/

static frame_buffer_t g_test_bufs[8];

static void reset_test_buffers(void)
{
  memset(g_test_bufs, 0, sizeof(g_test_bufs));
  for (int i = 0; i < 8; i++)
    {
      g_test_bufs[i].id = i;
      g_test_bufs[i].next = NULL;
    }
}

/****************************************************************************
 * Test: Queue empty checks
 ****************************************************************************/

static void test_empty_queue_is_empty(void)
{
  frame_buffer_t *queue = NULL;
  TEST_ASSERT(frame_queue_is_empty(queue));
}

static void test_nonempty_queue_is_not_empty(void)
{
  reset_test_buffers();
  frame_buffer_t *queue = NULL;
  frame_queue_push(&queue, &g_test_bufs[0]);
  TEST_ASSERT(!frame_queue_is_empty(queue));
}

/****************************************************************************
 * Test: Queue depth
 ****************************************************************************/

static void test_empty_queue_depth_zero(void)
{
  frame_buffer_t *queue = NULL;
  TEST_ASSERT_EQUAL(0, frame_queue_depth(queue));
}

static void test_single_element_depth(void)
{
  reset_test_buffers();
  frame_buffer_t *queue = NULL;
  frame_queue_push(&queue, &g_test_bufs[0]);
  TEST_ASSERT_EQUAL(1, frame_queue_depth(queue));
}

static void test_multiple_elements_depth(void)
{
  reset_test_buffers();
  frame_buffer_t *queue = NULL;
  frame_queue_push(&queue, &g_test_bufs[0]);
  frame_queue_push(&queue, &g_test_bufs[1]);
  frame_queue_push(&queue, &g_test_bufs[2]);
  TEST_ASSERT_EQUAL(3, frame_queue_depth(queue));
}

/****************************************************************************
 * Test: Push and pull FIFO order
 ****************************************************************************/

static void test_fifo_order(void)
{
  reset_test_buffers();
  frame_buffer_t *queue = NULL;

  frame_queue_push(&queue, &g_test_bufs[0]);
  frame_queue_push(&queue, &g_test_bufs[1]);
  frame_queue_push(&queue, &g_test_bufs[2]);

  frame_buffer_t *b;

  b = frame_queue_pull(&queue);
  TEST_ASSERT_NOT_NULL(b);
  TEST_ASSERT_EQUAL(0, b->id);

  b = frame_queue_pull(&queue);
  TEST_ASSERT_NOT_NULL(b);
  TEST_ASSERT_EQUAL(1, b->id);

  b = frame_queue_pull(&queue);
  TEST_ASSERT_NOT_NULL(b);
  TEST_ASSERT_EQUAL(2, b->id);

  TEST_ASSERT(frame_queue_is_empty(queue));
}

static void test_pull_from_empty(void)
{
  frame_buffer_t *queue = NULL;
  frame_buffer_t *b = frame_queue_pull(&queue);
  TEST_ASSERT_NULL(b);
}

static void test_push_null_is_safe(void)
{
  frame_buffer_t *queue = NULL;
  frame_queue_push(&queue, NULL);
  TEST_ASSERT(frame_queue_is_empty(queue));
  TEST_ASSERT_EQUAL(0, frame_queue_depth(queue));
}

/****************************************************************************
 * Test: Interleaved push/pull
 ****************************************************************************/

static void test_interleaved_operations(void)
{
  reset_test_buffers();
  frame_buffer_t *queue = NULL;

  /* Push 2 */
  frame_queue_push(&queue, &g_test_bufs[0]);
  frame_queue_push(&queue, &g_test_bufs[1]);
  TEST_ASSERT_EQUAL(2, frame_queue_depth(queue));

  /* Pull 1 */
  frame_buffer_t *b = frame_queue_pull(&queue);
  TEST_ASSERT_EQUAL(0, b->id);
  TEST_ASSERT_EQUAL(1, frame_queue_depth(queue));

  /* Push 1 more */
  frame_queue_push(&queue, &g_test_bufs[2]);
  TEST_ASSERT_EQUAL(2, frame_queue_depth(queue));

  /* Pull remaining */
  b = frame_queue_pull(&queue);
  TEST_ASSERT_EQUAL(1, b->id);
  b = frame_queue_pull(&queue);
  TEST_ASSERT_EQUAL(2, b->id);
  TEST_ASSERT(frame_queue_is_empty(queue));
}

/****************************************************************************
 * Test: Buffer pool allocation
 ****************************************************************************/

static void test_allocate_buffers(void)
{
  /* Reset global state */
  g_buffer_pool = NULL;
  g_buffer_pool_size = 0;
  g_action_queue = NULL;
  g_empty_queue = NULL;
  g_shutdown_requested = false;

  pthread_mutex_init(&g_queue_mutex, NULL);
  pthread_cond_init(&g_queue_cond, NULL);

  int ret = frame_queue_allocate_buffers(1024, 3);
  TEST_ASSERT_EQUAL(0, ret);
  TEST_ASSERT_NOT_NULL(g_buffer_pool);
  TEST_ASSERT_EQUAL(3, g_buffer_pool_size);

  /* All buffers should be in empty queue */
  TEST_ASSERT_EQUAL(3, frame_queue_depth(g_empty_queue));
  TEST_ASSERT(frame_queue_is_empty(g_action_queue));

  frame_queue_free_buffers();
  pthread_cond_destroy(&g_queue_cond);
  pthread_mutex_destroy(&g_queue_mutex);
}

static void test_allocate_buffers_invalid(void)
{
  TEST_ASSERT(frame_queue_allocate_buffers(0, 3) < 0);
  TEST_ASSERT(frame_queue_allocate_buffers(1024, 0) < 0);
}

static void test_buffer_pool_healthy(void)
{
  g_buffer_pool = NULL;
  g_buffer_pool_size = 0;
  TEST_ASSERT(!frame_queue_is_buffer_pool_healthy());

  g_action_queue = NULL;
  g_empty_queue = NULL;
  g_shutdown_requested = false;
  pthread_mutex_init(&g_queue_mutex, NULL);
  pthread_cond_init(&g_queue_cond, NULL);

  frame_queue_allocate_buffers(512, 2);
  TEST_ASSERT(frame_queue_is_buffer_pool_healthy());

  frame_queue_free_buffers();
  TEST_ASSERT(!frame_queue_is_buffer_pool_healthy());

  pthread_cond_destroy(&g_queue_cond);
  pthread_mutex_destroy(&g_queue_mutex);
}

/****************************************************************************
 * Main
 ****************************************************************************/

int main(void)
{
  TEST_SUITE_BEGIN("Frame Queue Tests");

  /* Empty/non-empty checks */
  RUN_TEST(test_empty_queue_is_empty);
  RUN_TEST(test_nonempty_queue_is_not_empty);

  /* Depth */
  RUN_TEST(test_empty_queue_depth_zero);
  RUN_TEST(test_single_element_depth);
  RUN_TEST(test_multiple_elements_depth);

  /* FIFO order */
  RUN_TEST(test_fifo_order);
  RUN_TEST(test_pull_from_empty);
  RUN_TEST(test_push_null_is_safe);
  RUN_TEST(test_interleaved_operations);

  /* Buffer pool */
  RUN_TEST(test_allocate_buffers);
  RUN_TEST(test_allocate_buffers_invalid);
  RUN_TEST(test_buffer_pool_healthy);

  TEST_SUITE_END();
}
