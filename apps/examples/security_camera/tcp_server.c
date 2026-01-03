/****************************************************************************
 * apps/examples/security_camera/tcp_server.c
 *
 * TCP Server Implementation (Phase 7)
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <debug.h>
#include <time.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>  /* For TCP_NODELAY */
#include <arpa/inet.h>

#include "tcp_server.h"

/****************************************************************************
 * Public Data
 ****************************************************************************/

/* Global TCP statistics */

tcp_stats_t g_tcp_stats = {0};

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/**
 * Initialize TCP server
 */

int tcp_server_init(tcp_server_t *server, uint16_t port)
{
  struct sockaddr_in servaddr;
  int ret;
  int optval = 1;

  if (server == NULL)
    {
      return -EINVAL;
    }

  memset(server, 0, sizeof(tcp_server_t));
  server->listen_fd = -1;
  server->client_fd = -1;
  server->port = port;

  /* Create TCP socket */
  server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server->listen_fd < 0)
    {
      _err("ERROR: Failed to create socket: %d\n", errno);
      return -errno;
    }

  /* Set SO_REUSEADDR to allow quick restart */
  ret = setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR,
                   &optval, sizeof(optval));
  if (ret < 0)
    {
      _warn("WARNING: Failed to set SO_REUSEADDR: %d\n", errno);
    }

  /* Bind to port */
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(port);

  ret = bind(server->listen_fd, (struct sockaddr *)&servaddr,
             sizeof(servaddr));
  if (ret < 0)
    {
      _err("ERROR: Failed to bind to port %d: %d\n", port, errno);
      close(server->listen_fd);
      server->listen_fd = -1;
      return -errno;
    }

  /* Start listening */
  ret = listen(server->listen_fd, 1);  /* Backlog = 1 (single client) */
  if (ret < 0)
    {
      _err("ERROR: Failed to listen: %d\n", errno);
      close(server->listen_fd);
      server->listen_fd = -1;
      return -errno;
    }

  server->is_running = true;

  _info("TCP server initialized on port %d\n", port);
  return OK;
}

/**
 * Wait for client connection (blocking)
 */

int tcp_server_accept(tcp_server_t *server)
{
  struct sockaddr_in cliaddr;
  socklen_t clilen;
  int connfd;
  int ret;

  if (server == NULL || server->listen_fd < 0)
    {
      return -EINVAL;
    }

  /* If already have a client, disconnect it first */
  if (server->client_fd >= 0)
    {
      tcp_server_disconnect_client(server);
    }

  _info("Waiting for client connection...\n");

  clilen = sizeof(cliaddr);
  connfd = accept(server->listen_fd, (struct sockaddr *)&cliaddr, &clilen);

  if (connfd < 0)
    {
      _err("ERROR: Failed to accept connection: %d\n", errno);
      return -errno;
    }

  server->client_fd = connfd;

  /* Optimize TCP for low latency and high throughput */

  /* 1. Disable Nagle algorithm (TCP_NODELAY) for low latency */
  int nodelay = 1;
  ret = setsockopt(connfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
  if (ret < 0)
    {
      _warn("WARNING: Failed to set TCP_NODELAY: %d\n", errno);
    }
  else
    {
      _info("TCP_NODELAY enabled (low latency mode)\n");
    }

  /* 2. Increase send buffer size (256KB for MJPEG streaming) */
  int sndbuf = 262144;  /* 256KB */
  ret = setsockopt(connfd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
  if (ret < 0)
    {
      _warn("WARNING: Failed to set SO_SNDBUF: %d\n", errno);
    }
  else
    {
      /* Read back actual buffer size */
      socklen_t optlen = sizeof(sndbuf);
      getsockopt(connfd, SOL_SOCKET, SO_SNDBUF, &sndbuf, &optlen);
      _info("TCP send buffer: %d bytes\n", sndbuf);
    }

  _info("Client connected from %s:%d\n",
        inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));

  return OK;
}

/**
 * Send data to connected client
 */

int tcp_server_send(tcp_server_t *server, const void *data, size_t len)
{
  ssize_t sent;
  size_t total_sent = 0;
  const uint8_t *ptr = (const uint8_t *)data;
  int retry_count = 0;
  const int MAX_RETRIES = 3;
  struct timespec start, end;
  uint64_t send_time_us;

  if (server == NULL || data == NULL)
    {
      return -EINVAL;
    }

  if (server->client_fd < 0)
    {
      return -ENOTCONN;
    }

  /* Start timing */
  clock_gettime(CLOCK_MONOTONIC, &start);

  /* Loop until all data is sent (handle partial writes) */
  while (total_sent < len)
    {
      sent = write(server->client_fd, ptr + total_sent, len - total_sent);

      if (sent < 0)
        {
          /* Handle temporary errors (buffer full) */
          if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
              if (retry_count++ < MAX_RETRIES)
                {
                  /* Wait briefly for TCP buffer to drain */
                  usleep(10000);  /* 10ms */
                  continue;
                }
              else
                {
                  _err("ERROR: TCP send timeout (buffer full)\n");
                  return -ETIMEDOUT;
                }
            }

          /* Fatal error - disconnect client */
          _err("ERROR: Failed to send data: %d\n", errno);
          tcp_server_disconnect_client(server);
          return -errno;
        }
      else if (sent == 0)
        {
          /* Connection closed by peer */
          _warn("WARNING: Connection closed by peer\n");
          tcp_server_disconnect_client(server);
          return -ENOTCONN;
        }

      total_sent += sent;
      retry_count = 0;  /* Reset retry counter on successful send */
    }

  /* End timing */
  clock_gettime(CLOCK_MONOTONIC, &end);

  /* Calculate send time in microseconds
   * Convert both timespec to microseconds first to avoid negative nsec_diff
   * Bug fix: (end.tv_nsec - start.tv_nsec) can be negative when second rolls over
   */
  uint64_t start_us = (uint64_t)start.tv_sec * 1000000ULL +
                      (uint64_t)start.tv_nsec / 1000ULL;
  uint64_t end_us = (uint64_t)end.tv_sec * 1000000ULL +
                    (uint64_t)end.tv_nsec / 1000ULL;
  send_time_us = end_us - start_us;

  /* Update statistics */
  g_tcp_stats.total_send_time_us += send_time_us;
  g_tcp_stats.send_count++;
  if (send_time_us > g_tcp_stats.max_send_time_us)
    {
      g_tcp_stats.max_send_time_us = send_time_us;
    }

  /* Log warning for slow sends (>100ms) */
  if (send_time_us > 100000)
    {
      _warn("WARNING: TCP send took %lu ms (%lu bytes)\n",
            (unsigned long)(send_time_us / 1000), (unsigned long)len);
    }

  return total_sent;
}

/**
 * Check if client is connected
 */

bool tcp_server_has_client(tcp_server_t *server)
{
  if (server == NULL)
    {
      return false;
    }

  return (server->client_fd >= 0);
}

/**
 * Disconnect current client
 */

void tcp_server_disconnect_client(tcp_server_t *server)
{
  if (server == NULL)
    {
      return;
    }

  if (server->client_fd >= 0)
    {
      _info("Disconnecting client...\n");
      close(server->client_fd);
      server->client_fd = -1;
    }
}

/**
 * Cleanup TCP server
 */

void tcp_server_cleanup(tcp_server_t *server)
{
  if (server == NULL)
    {
      return;
    }

  tcp_server_disconnect_client(server);

  if (server->listen_fd >= 0)
    {
      close(server->listen_fd);
      server->listen_fd = -1;
    }

  server->is_running = false;

  _info("TCP server cleanup complete\n");
}

/**
 * Get TCP send statistics
 */

uint32_t tcp_server_get_stats(uint32_t *avg_us, uint32_t *max_us)
{
  if (avg_us != NULL)
    {
      if (g_tcp_stats.send_count > 0)
        {
          *avg_us = (uint32_t)(g_tcp_stats.total_send_time_us / g_tcp_stats.send_count);
        }
      else
        {
          *avg_us = 0;
        }
    }

  if (max_us != NULL)
    {
      *max_us = g_tcp_stats.max_send_time_us;
    }

  return g_tcp_stats.send_count;
}

/**
 * Reset TCP send statistics
 */

void tcp_server_reset_stats(void)
{
  g_tcp_stats.total_send_time_us = 0;
  g_tcp_stats.send_count = 0;
  g_tcp_stats.max_send_time_us = 0;
}

/**
 * Send data directly to connected client (bypass queue, for shutdown metrics)
 *
 * Phase 7.1: This function is used to send final metrics during shutdown
 * when the normal queue mechanism is not available.
 */

int tcp_server_send_direct(tcp_server_t *server, const void *data, size_t len)
{
  ssize_t sent;
  size_t total_sent = 0;
  const uint8_t *ptr = (const uint8_t *)data;
  int retry_count = 0;
  const int MAX_RETRIES = 5;  /* More retries for shutdown scenario */

  if (server == NULL || data == NULL)
    {
      return -EINVAL;
    }

  if (server->client_fd < 0)
    {
      /* No client connected - not an error during shutdown */
      return 0;
    }

  _info("Sending final metrics directly (%zu bytes)...\n", len);

  /* Loop until all data is sent (handle partial writes) */
  while (total_sent < len)
    {
      sent = write(server->client_fd, ptr + total_sent, len - total_sent);

      if (sent < 0)
        {
          /* Handle temporary errors (buffer full) */
          if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
              if (retry_count++ < MAX_RETRIES)
                {
                  /* Wait longer for TCP buffer to drain during shutdown */
                  usleep(20000);  /* 20ms */
                  continue;
                }
              else
                {
                  _warn("WARNING: TCP direct send timeout after %d retries\n", MAX_RETRIES);
                  return total_sent;  /* Return partial send */
                }
            }

          /* Fatal error - just log and return */
          _warn("WARNING: TCP direct send failed: %d\n", errno);
          return total_sent > 0 ? (int)total_sent : -errno;
        }
      else if (sent == 0)
        {
          /* Connection closed by peer */
          _warn("WARNING: Connection closed during direct send\n");
          return total_sent;
        }

      total_sent += sent;
      retry_count = 0;  /* Reset retry counter on successful send */
    }

  _info("Final metrics sent successfully (%zu bytes)\n", total_sent);
  return total_sent;
}
