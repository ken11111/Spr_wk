/****************************************************************************
 * tests/mock_nuttx.h
 *
 * Mock NuttX headers for host-based unit testing.
 * Provides stubs for NuttX-specific types and functions so that
 * application modules can compile with a standard host gcc.
 *
 ****************************************************************************/

#ifndef __MOCK_NUTTX_H
#define __MOCK_NUTTX_H

/* Stub out nuttx/config.h — define nothing by default */
#define __NUTTX_CONFIG_H

/* Provide V4L2 pixel format used by config.h */
#ifndef V4L2_PIX_FMT_JPEG
#define V4L2_PIX_FMT_JPEG 0x4745504A
#endif

/* Stub video types header */
#ifndef __NUTTX_VIDEO_VIDEO_H
#define __NUTTX_VIDEO_VIDEO_H
#define VIDEO_CODEC_TYPE_H264 1
#endif

/* syslog stubs — redirect to printf */
#include <stdio.h>
#define LOG_INFO(fmt, ...)    ((void)0)
#define LOG_WARN(fmt, ...)    ((void)0)
#define LOG_ERROR(fmt, ...)   ((void)0)
#define LOG_DEBUG(fmt, ...)   ((void)0)

/* memalign stub for non-NuttX systems */
#include <stdlib.h>
#ifndef memalign
static inline void *memalign(size_t alignment, size_t size)
{
  void *ptr = NULL;
  (void)alignment;
  ptr = malloc(size);
  return ptr;
}
#endif

#endif /* __MOCK_NUTTX_H */
