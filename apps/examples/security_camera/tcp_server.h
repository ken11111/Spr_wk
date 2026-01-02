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

#endif /* __APPS_EXAMPLES_SECURITY_CAMERA_TCP_SERVER_H */
