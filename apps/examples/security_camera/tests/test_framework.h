/****************************************************************************
 * tests/test_framework.h
 *
 * Minimal unit test framework for host-based testing of embedded C modules.
 * No external dependencies required.
 *
 ****************************************************************************/

#ifndef __TEST_FRAMEWORK_H
#define __TEST_FRAMEWORK_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int g_test_pass = 0;
static int g_test_fail = 0;
static int g_test_total = 0;

#define TEST_ASSERT(cond)                                               \
  do {                                                                  \
    g_test_total++;                                                     \
    if (!(cond)) {                                                      \
      printf("  FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond);        \
      g_test_fail++;                                                    \
    } else {                                                            \
      g_test_pass++;                                                    \
    }                                                                   \
  } while (0)

#define TEST_ASSERT_EQUAL(expected, actual)                             \
  do {                                                                  \
    g_test_total++;                                                     \
    if ((expected) != (actual)) {                                       \
      printf("  FAIL: %s:%d: expected %d, got %d\n",                   \
             __FILE__, __LINE__, (int)(expected), (int)(actual));       \
      g_test_fail++;                                                    \
    } else {                                                            \
      g_test_pass++;                                                    \
    }                                                                   \
  } while (0)

#define TEST_ASSERT_NOT_NULL(ptr)                                      \
  do {                                                                  \
    g_test_total++;                                                     \
    if ((ptr) == NULL) {                                                \
      printf("  FAIL: %s:%d: expected non-NULL\n", __FILE__, __LINE__);\
      g_test_fail++;                                                    \
    } else {                                                            \
      g_test_pass++;                                                    \
    }                                                                   \
  } while (0)

#define TEST_ASSERT_NULL(ptr)                                          \
  do {                                                                  \
    g_test_total++;                                                     \
    if ((ptr) != NULL) {                                                \
      printf("  FAIL: %s:%d: expected NULL\n", __FILE__, __LINE__);    \
      g_test_fail++;                                                    \
    } else {                                                            \
      g_test_pass++;                                                    \
    }                                                                   \
  } while (0)

#define TEST_ASSERT_MEM_EQUAL(expected, actual, len)                   \
  do {                                                                  \
    g_test_total++;                                                     \
    if (memcmp((expected), (actual), (len)) != 0) {                    \
      printf("  FAIL: %s:%d: memory not equal (%zu bytes)\n",          \
             __FILE__, __LINE__, (size_t)(len));                        \
      g_test_fail++;                                                    \
    } else {                                                            \
      g_test_pass++;                                                    \
    }                                                                   \
  } while (0)

#define RUN_TEST(func)                                                 \
  do {                                                                  \
    printf("  [TEST] %s ... ", #func);                                  \
    func();                                                             \
    printf("done\n");                                                   \
  } while (0)

#define TEST_SUITE_BEGIN(name)                                         \
  printf("\n=== %s ===\n", name);                                      \
  g_test_pass = 0;                                                      \
  g_test_fail = 0;                                                      \
  g_test_total = 0

#define TEST_SUITE_END()                                               \
  printf("\nResults: %d/%d passed", g_test_pass, g_test_total);        \
  if (g_test_fail > 0) printf(", %d FAILED", g_test_fail);            \
  printf("\n");                                                         \
  return g_test_fail

#endif /* __TEST_FRAMEWORK_H */
