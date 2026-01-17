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
 * Pre-processor Definitions
 ****************************************************************************/

/* Phase 9: Auto-reconnect configuration */

#define TCP_RECONNECT_MAX         10    /* Maximum reconnect attempts */
#define TCP_RECONNECT_WAIT_MS   5000    /* Base wait time between reconnects (ms) */
#define TCP_RECONNECT_BACKOFF_MS 2000   /* Additional wait per attempt (ms) */

/* Phase 9.2: TCP health monitoring configuration */

#define TCP_HEALTH_WINDOW_SIZE     8    /* Moving average window size */
#define TCP_SPIKE_THRESHOLD_RATIO  3    /* Spike threshold: ratio to moving average (3x = 300%) */
#define TCP_CRITICAL_SEND_TIME_MS  1000 /* Critical send time threshold (ms) */
#define TCP_CONSECUTIVE_SPIKE_MAX  2    /* Max consecutive spikes before preventive reconnect */
#define TCP_SPIKE_RECOVERY_COUNT   3    /* Normal sends required to clear spike state */

/****************************************************************************
 * Public Types
 ****************************************************************************/

/**
 * Phase 9: TCP connection state
 */

typedef enum tcp_connection_state_e
{
  TCP_STATE_DISCONNECTED = 0,
  TCP_STATE_LISTENING,
  TCP_STATE_CONNECTED,
  TCP_STATE_RECONNECTING,
} tcp_connection_state_t;

/**
 * TCP server context
 */

typedef struct tcp_server_s
{
  int listen_fd;       /* Listening socket */
  int client_fd;       /* Connected client socket (-1 if no client) */
  uint16_t port;       /* Server port number */
  bool is_running;     /* Server running state */

  /* Phase 9: Auto-reconnect support */

  tcp_connection_state_t state;         /* Connection state */
  int reconnect_count;                  /* Number of reconnect attempts */
  bool auto_reconnect_enabled;          /* Auto-reconnect enabled flag */
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

/**
 * Phase 9.2: TCP health monitor for GS2200M resource exhaustion detection
 *
 * TCP送信時間のスパイクを監視し、GS2200Mリソース枯渇を予測する。
 * 連続スパイクを検出した場合、予防的に再接続を行う。
 */

typedef struct tcp_health_monitor_s
{
  uint32_t send_times_ms[TCP_HEALTH_WINDOW_SIZE];  /* Recent send times (circular buffer) */
  uint8_t  window_index;                           /* Current position in circular buffer */
  uint8_t  window_filled;                          /* How many entries are filled (0-WINDOW_SIZE) */
  uint32_t moving_avg_ms;                          /* Current moving average (ms) */
  uint8_t  consecutive_spikes;                     /* Consecutive spike counter */
  uint8_t  recovery_count;                         /* Normal sends since last spike */
  uint32_t total_spikes;                           /* Total spike count for metrics */
  bool     degradation_alert;                      /* True if health is degraded */
  bool     preventive_reconnect_needed;            /* True if preventive reconnect recommended */
} tcp_health_monitor_t;

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

/* Phase 9.2: Global health monitor */

EXTERN tcp_health_monitor_t g_tcp_health;

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

/****************************************************************************
 * Phase 9: Auto-reconnect Functions
 ****************************************************************************/

/**
 * Handle TCP disconnection and prepare for reconnect
 *
 * Parameters:
 *   server - TCP server context
 *
 * Returns:
 *   0 if ready for reconnect, -1 if max attempts reached or disabled
 */

int tcp_server_handle_disconnect(tcp_server_t *server);

/**
 * Wait for client reconnection (blocking)
 *
 * Parameters:
 *   server - TCP server context
 *
 * Returns:
 *   0 on successful reconnect, negative errno on failure
 */

int tcp_server_wait_reconnect(tcp_server_t *server);

/**
 * Send data with automatic reconnect on disconnect
 *
 * Parameters:
 *   server - TCP server context
 *   data   - Data buffer to send
 *   len    - Data length
 *
 * Returns:
 *   Number of bytes sent on success
 *   -EAGAIN if reconnected (caller should skip current frame)
 *   Negative errno on unrecoverable error
 */

int tcp_server_send_with_reconnect(tcp_server_t *server,
                                   const void *data, size_t len);

/**
 * Enable or disable auto-reconnect
 *
 * Parameters:
 *   server  - TCP server context
 *   enabled - true to enable, false to disable
 */

void tcp_server_set_auto_reconnect(tcp_server_t *server, bool enabled);

/**
 * Get current connection state
 *
 * Parameters:
 *   server - TCP server context
 *
 * Returns:
 *   Current connection state
 */

tcp_connection_state_t tcp_server_get_state(tcp_server_t *server);

/****************************************************************************
 * Phase 9.2: TCP Health Monitoring Functions
 ****************************************************************************/

/**
 * Initialize TCP health monitor
 */

void tcp_health_init(void);

/**
 * Update health monitor with new send time
 *
 * Parameters:
 *   send_time_us - Send time in microseconds
 *
 * Returns:
 *   true if connection is healthy, false if degradation detected
 */

bool tcp_health_update(uint64_t send_time_us);

/**
 * Check if preventive reconnect is recommended
 *
 * Returns:
 *   true if preventive reconnect should be triggered
 */

bool tcp_health_should_reconnect(void);

/**
 * Get health metrics for reporting
 *
 * Parameters:
 *   moving_avg_ms       - Output: Current moving average (ms)
 *   consecutive_spikes  - Output: Current consecutive spike count
 *   total_spikes        - Output: Total spike count
 *   degradation_alert   - Output: True if health degraded
 */

void tcp_health_get_metrics(uint32_t *moving_avg_ms,
                            uint8_t *consecutive_spikes,
                            uint32_t *total_spikes,
                            bool *degradation_alert);

/**
 * Clear preventive reconnect flag after reconnection
 */

void tcp_health_clear_reconnect_flag(void);

/**
 * Reset health monitor (after successful reconnect)
 */

void tcp_health_reset(void);

#endif /* __APPS_EXAMPLES_SECURITY_CAMERA_TCP_SERVER_H */
