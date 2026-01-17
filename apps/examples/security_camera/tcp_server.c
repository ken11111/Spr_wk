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

/* Phase 9.2: Global health monitor */

tcp_health_monitor_t g_tcp_health = {0};

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

  /* Phase 9: Initialize auto-reconnect fields */

  server->state = TCP_STATE_DISCONNECTED;
  server->reconnect_count = 0;
  server->auto_reconnect_enabled = true;  /* Enabled by default */

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
  server->state = TCP_STATE_LISTENING;  /* Phase 9 */

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

  /* 2. Increase send buffer size (256KB for MJPEG streaming, Phase 7.2a: Increased for batch packets) */
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

  server->state = TCP_STATE_CONNECTED;  /* Phase 9 */

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

  /* Phase 7.2b: Reduced logging for better performance */
  _info("TCP send: %zu bytes (client_fd=%d)\n", len, server->client_fd);

  /* Track progress for logging (every 10%) */
  size_t last_logged_progress = 0;

  /* Loop until all data is sent (handle partial writes) */
  while (total_sent < len)
    {
      sent = write(server->client_fd, ptr + total_sent, len - total_sent);

      /* Phase 7.2b: Log only every 10% progress to reduce overhead */
      if (sent > 0)
        {
          size_t progress_pct = (total_sent + sent) * 100 / len;
          if (progress_pct >= last_logged_progress + 10 || (total_sent + sent) == len)
            {
              _info("TCP progress: %zu/%zu (%zu%%)\n",
                    total_sent + sent, len, progress_pct);
              last_logged_progress = progress_pct;
            }
        }

      if (sent < 0)
        {
          /* Handle temporary errors (buffer full) */
          if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
              if (retry_count++ < MAX_RETRIES)
                {
                  _warn("TCP buffer full, retrying (%d/%d)...\n",
                        retry_count, MAX_RETRIES);
                  /* Wait briefly for TCP buffer to drain */
                  usleep(10000);  /* 10ms */
                  continue;
                }
              else
                {
                  _err("TCP send timeout after %d retries\n", MAX_RETRIES);
                  return -ETIMEDOUT;
                }
            }

          /* Fatal error - disconnect client */
          _err("TCP write error %d (sent %zu/%zu bytes)\n",
               errno, total_sent, len);
          tcp_server_disconnect_client(server);
          return -errno;
        }
      else if (sent == 0)
        {
          /* Connection closed by peer */
          _warn("TCP connection closed by peer (sent %zu/%zu)\n",
                total_sent, len);
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

  /* Phase 9.2: Update health monitor */
  tcp_health_update(send_time_us);

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

/****************************************************************************
 * Phase 9: Auto-reconnect Functions
 ****************************************************************************/

/**
 * Handle TCP disconnection and prepare for reconnect
 */

int tcp_server_handle_disconnect(tcp_server_t *server)
{
  if (server == NULL)
    {
      return -EINVAL;
    }

  if (!server->auto_reconnect_enabled)
    {
      _info("Auto-reconnect disabled, not reconnecting\n");
      server->state = TCP_STATE_DISCONNECTED;
      return -1;
    }

  if (server->reconnect_count >= TCP_RECONNECT_MAX)
    {
      _err("ERROR: Max reconnect attempts (%d) reached\n",
           TCP_RECONNECT_MAX);
      server->state = TCP_STATE_DISCONNECTED;
      return -1;
    }

  /* Close client socket */

  if (server->client_fd >= 0)
    {
      close(server->client_fd);
      server->client_fd = -1;
    }

  server->state = TCP_STATE_RECONNECTING;
  server->reconnect_count++;

  /* Phase 9.1: Exponential backoff wait
   * GS2200Mがリソース枯渇状態に入るのを防ぐため、
   * 再接続回数に応じてウェイト時間を増加させる
   * wait_ms = base + (attempt - 1) * backoff
   */

  uint32_t wait_ms = TCP_RECONNECT_WAIT_MS +
                     (server->reconnect_count - 1) * TCP_RECONNECT_BACKOFF_MS;

  _info("TCP disconnected, waiting %lu ms before reconnect (%d/%d)...\n",
        (unsigned long)wait_ms, server->reconnect_count, TCP_RECONNECT_MAX);

  /* Cooldown wait with backoff */

  usleep(wait_ms * 1000);

  server->state = TCP_STATE_LISTENING;

  return 0;
}

/**
 * Wait for client reconnection (blocking)
 */

int tcp_server_wait_reconnect(tcp_server_t *server)
{
  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(client_addr);
  int optval = 1;
  int ret;

  if (server == NULL || server->listen_fd < 0)
    {
      return -EINVAL;
    }

  _info("Waiting for client reconnection on port %d...\n", server->port);

  /* accept() waits for client connection */

  server->client_fd = accept(server->listen_fd,
                             (struct sockaddr *)&client_addr,
                             &addr_len);
  if (server->client_fd < 0)
    {
      _err("ERROR: accept() failed: %d\n", errno);
      return -errno;
    }

  /* Set TCP_NODELAY */

  ret = setsockopt(server->client_fd, IPPROTO_TCP, TCP_NODELAY,
                   &optval, sizeof(optval));
  if (ret < 0)
    {
      _warn("WARNING: Failed to set TCP_NODELAY: %d\n", errno);
    }

  /* Set send buffer size */

  int sndbuf = 262144;  /* 256KB */
  ret = setsockopt(server->client_fd, SOL_SOCKET, SO_SNDBUF,
                   &sndbuf, sizeof(sndbuf));
  if (ret < 0)
    {
      _warn("WARNING: Failed to set SO_SNDBUF: %d\n", errno);
    }

  server->state = TCP_STATE_CONNECTED;

  _info("Client reconnected from %s:%d (attempt %d)\n",
        inet_ntoa(client_addr.sin_addr),
        ntohs(client_addr.sin_port),
        server->reconnect_count);

  return 0;
}

/**
 * Send data with automatic reconnect on disconnect
 */

int tcp_server_send_with_reconnect(tcp_server_t *server,
                                   const void *data, size_t len)
{
  int ret;

  if (server == NULL || data == NULL)
    {
      return -EINVAL;
    }

  ret = tcp_server_send(server, data, len);

  if (ret < 0)
    {
      int err = -ret;

      /* Check for disconnect errors */

      if (err == ENOTCONN || err == ECONNRESET || err == EPIPE)
        {
          _warn("TCP disconnect detected (error %d), attempting reconnect...\n",
                err);

          /* Handle disconnect */

          if (tcp_server_handle_disconnect(server) == 0)
            {
              /* Wait for reconnect */

              if (tcp_server_wait_reconnect(server) == 0)
                {
                  /* Reconnect successful, skip current frame */

                  _info("Reconnect successful, resuming streaming\n");
                  return -EAGAIN;
                }
            }

          /* Reconnect failed or disabled */

          _err("ERROR: Reconnect failed, stopping\n");
        }
    }

  return ret;
}

/**
 * Enable or disable auto-reconnect
 */

void tcp_server_set_auto_reconnect(tcp_server_t *server, bool enabled)
{
  if (server != NULL)
    {
      server->auto_reconnect_enabled = enabled;
      _info("Auto-reconnect %s\n", enabled ? "enabled" : "disabled");
    }
}

/**
 * Get current connection state
 */

tcp_connection_state_t tcp_server_get_state(tcp_server_t *server)
{
  if (server == NULL)
    {
      return TCP_STATE_DISCONNECTED;
    }

  return server->state;
}

/****************************************************************************
 * Phase 9.2: TCP Health Monitoring Implementation
 ****************************************************************************/

/**
 * Initialize TCP health monitor
 */

void tcp_health_init(void)
{
  memset(&g_tcp_health, 0, sizeof(g_tcp_health));
  _info("TCP health monitor initialized\n");
}

/**
 * Update health monitor with new send time
 *
 * Returns:
 *   true if connection is healthy, false if degradation detected
 */

bool tcp_health_update(uint64_t send_time_us)
{
  uint32_t send_time_ms = (uint32_t)(send_time_us / 1000);
  uint32_t sum = 0;
  bool is_spike = false;
  bool is_healthy = true;

  /* Store in circular buffer */

  g_tcp_health.send_times_ms[g_tcp_health.window_index] = send_time_ms;
  g_tcp_health.window_index = (g_tcp_health.window_index + 1) % TCP_HEALTH_WINDOW_SIZE;

  if (g_tcp_health.window_filled < TCP_HEALTH_WINDOW_SIZE)
    {
      g_tcp_health.window_filled++;
    }

  /* Calculate moving average */

  for (int i = 0; i < g_tcp_health.window_filled; i++)
    {
      sum += g_tcp_health.send_times_ms[i];
    }

  g_tcp_health.moving_avg_ms = sum / g_tcp_health.window_filled;

  /* Skip spike detection until we have enough samples */

  if (g_tcp_health.window_filled < 4)
    {
      return true;  /* Healthy during warm-up */
    }

  /* Check for spike: current > moving_avg * threshold_ratio */

  if (g_tcp_health.moving_avg_ms > 0)
    {
      uint32_t spike_threshold = g_tcp_health.moving_avg_ms * TCP_SPIKE_THRESHOLD_RATIO;

      if (send_time_ms > spike_threshold)
        {
          is_spike = true;
          g_tcp_health.consecutive_spikes++;
          g_tcp_health.total_spikes++;
          g_tcp_health.recovery_count = 0;

          _warn("TCP SPIKE detected: %lu ms (avg=%lu ms, ratio=%.1f)\n",
                (unsigned long)send_time_ms,
                (unsigned long)g_tcp_health.moving_avg_ms,
                (float)send_time_ms / g_tcp_health.moving_avg_ms);
        }
    }

  /* Check for critical send time (absolute threshold) */

  if (send_time_ms > TCP_CRITICAL_SEND_TIME_MS)
    {
      is_spike = true;
      g_tcp_health.consecutive_spikes++;

      /* Only increment total_spikes if not already counted above */

      if (g_tcp_health.moving_avg_ms > 0 &&
          send_time_ms <= g_tcp_health.moving_avg_ms * TCP_SPIKE_THRESHOLD_RATIO)
        {
          g_tcp_health.total_spikes++;
        }

      g_tcp_health.recovery_count = 0;

      _warn("TCP CRITICAL: %lu ms exceeds %d ms threshold\n",
            (unsigned long)send_time_ms, TCP_CRITICAL_SEND_TIME_MS);
    }

  /* Handle spike state */

  if (is_spike)
    {
      /* Check for consecutive spike threshold */

      if (g_tcp_health.consecutive_spikes >= TCP_CONSECUTIVE_SPIKE_MAX)
        {
          g_tcp_health.degradation_alert = true;
          g_tcp_health.preventive_reconnect_needed = true;
          is_healthy = false;

          _err("TCP DEGRADATION ALERT: %d consecutive spikes, recommending reconnect\n",
               g_tcp_health.consecutive_spikes);
        }
    }
  else
    {
      /* Normal send - count toward recovery */

      g_tcp_health.recovery_count++;

      if (g_tcp_health.recovery_count >= TCP_SPIKE_RECOVERY_COUNT)
        {
          /* Recovered - clear spike state */

          if (g_tcp_health.consecutive_spikes > 0)
            {
              _info("TCP health recovered after %d normal sends\n",
                    g_tcp_health.recovery_count);
            }

          g_tcp_health.consecutive_spikes = 0;
          g_tcp_health.degradation_alert = false;
        }
    }

  return is_healthy;
}

/**
 * Check if preventive reconnect is recommended
 */

bool tcp_health_should_reconnect(void)
{
  return g_tcp_health.preventive_reconnect_needed;
}

/**
 * Get health metrics for reporting
 */

void tcp_health_get_metrics(uint32_t *moving_avg_ms,
                            uint8_t *consecutive_spikes,
                            uint32_t *total_spikes,
                            bool *degradation_alert)
{
  if (moving_avg_ms != NULL)
    {
      *moving_avg_ms = g_tcp_health.moving_avg_ms;
    }

  if (consecutive_spikes != NULL)
    {
      *consecutive_spikes = g_tcp_health.consecutive_spikes;
    }

  if (total_spikes != NULL)
    {
      *total_spikes = g_tcp_health.total_spikes;
    }

  if (degradation_alert != NULL)
    {
      *degradation_alert = g_tcp_health.degradation_alert;
    }
}

/**
 * Clear preventive reconnect flag after reconnection
 */

void tcp_health_clear_reconnect_flag(void)
{
  g_tcp_health.preventive_reconnect_needed = false;
  _info("TCP health: preventive reconnect flag cleared\n");
}

/**
 * Reset health monitor (after successful reconnect)
 */

void tcp_health_reset(void)
{
  uint32_t saved_total_spikes = g_tcp_health.total_spikes;

  memset(&g_tcp_health, 0, sizeof(g_tcp_health));

  /* Preserve total spike count for session metrics */

  g_tcp_health.total_spikes = saved_total_spikes;

  _info("TCP health monitor reset (total_spikes preserved: %lu)\n",
        (unsigned long)saved_total_spikes);
}
