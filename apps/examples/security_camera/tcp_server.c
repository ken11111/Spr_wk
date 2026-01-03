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

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "tcp_server.h"

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

  if (server == NULL || data == NULL)
    {
      return -EINVAL;
    }

  if (server->client_fd < 0)
    {
      return -ENOTCONN;
    }

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
