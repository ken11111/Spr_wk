/****************************************************************************
 * apps/examples/security_camera/tcp_server.h
 *
 * TCP Server for MJPEG Streaming (Phase 7)
 *
 ****************************************************************************/

#ifndef __APPS_EXAMPLES_SECURITY_CAMERA_TCP_SERVER_H
#define __APPS_EXAMPLES_SECURITY_CAMERA_TCP_SERVER_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <stdbool.h>

/****************************************************************************
 * Public Types
 ****************************************************************************/

/**
 * TCP server context
 */

typedef struct tcp_server_s
{
  int listen_fd;       /* Listening socket */
  int client_fd;       /* Connected client socket (-1 if no client) */
  uint16_t port;       /* Server port number */
  bool is_running;     /* Server running state */
} tcp_server_t;

/**
 * TCP send statistics (Phase 7 metrics)
 */

typedef struct tcp_stats_s
{
  uint64_t total_send_time_us;  /* Total TCP send time (microseconds) */
  uint32_t send_count;           /* Number of send operations */
  uint32_t max_send_time_us;     /* Maximum send time (microseconds) */
} tcp_stats_t;

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

/* Global TCP statistics */

EXTERN tcp_stats_t g_tcp_stats;

#undef EXTERN
#ifdef __cplusplus
}
#endif

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * Initialize TCP server
 *
 * Parameters:
 *   server - TCP server context
 *   port   - Port number to listen on
 *
 * Returns:
 *   0 on success, negative errno on failure
 */

int tcp_server_init(tcp_server_t *server, uint16_t port);

/**
 * Wait for client connection (blocking)
 *
 * Parameters:
 *   server - TCP server context
 *
 * Returns:
 *   0 on success, negative errno on failure
 */

int tcp_server_accept(tcp_server_t *server);

/**
 * Send data to connected client
 *
 * Parameters:
 *   server - TCP server context
 *   data   - Data buffer to send
 *   len    - Data length
 *
 * Returns:
 *   Number of bytes sent on success, negative errno on failure
 */

int tcp_server_send(tcp_server_t *server, const void *data, size_t len);

/**
 * Check if client is connected
 *
 * Parameters:
 *   server - TCP server context
 *
 * Returns:
 *   true if client is connected, false otherwise
 */

bool tcp_server_has_client(tcp_server_t *server);

/**
 * Disconnect current client
 *
 * Parameters:
 *   server - TCP server context
 */

void tcp_server_disconnect_client(tcp_server_t *server);

/**
 * Cleanup TCP server
 *
 * Parameters:
 *   server - TCP server context
 */

void tcp_server_cleanup(tcp_server_t *server);

/**
 * Get TCP send statistics
 *
 * Parameters:
 *   avg_us - Output: Average send time (microseconds)
 *   max_us - Output: Maximum send time (microseconds)
 *
 * Returns:
 *   Number of send operations
 */

uint32_t tcp_server_get_stats(uint32_t *avg_us, uint32_t *max_us);

/**
 * Reset TCP send statistics
 */

void tcp_server_reset_stats(void);

/**
 * Send data directly to connected client (bypass queue, for shutdown metrics)
 *
 * Parameters:
 *   server - TCP server context
 *   data   - Data buffer to send
 *   len    - Data length
 *
 * Returns:
 *   Number of bytes sent on success, negative errno on failure
 *
 * Note: This function bypasses the normal queue mechanism and is intended
 *       for sending final metrics during shutdown.
 */

int tcp_server_send_direct(tcp_server_t *server, const void *data, size_t len);

#endif /* __APPS_EXAMPLES_SECURITY_CAMERA_TCP_SERVER_H */
