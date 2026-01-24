/****************************************************************************
 * apps/examples/security_camera/wifi_manager.h
 *
 * WiFi Manager API (Phase 7)
 *
 ****************************************************************************/

#ifndef __APPS_EXAMPLES_SECURITY_CAMERA_WIFI_MANAGER_H
#define __APPS_EXAMPLES_SECURITY_CAMERA_WIFI_MANAGER_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <netinet/in.h>

/****************************************************************************
 * Public Types
 ****************************************************************************/

/**
 * WiFi connection state
 */

typedef enum wifi_state_e
{
  WIFI_STATE_DISCONNECTED,  /* WiFi disconnected */
  WIFI_STATE_CONNECTING,    /* WiFi connection in progress */
  WIFI_STATE_CONNECTED,     /* WiFi connected, no IP */
  WIFI_STATE_READY,         /* WiFi connected with IP address */
  WIFI_STATE_ERROR          /* WiFi error state */
} wifi_state_t;

/**
 * WiFi manager context
 */

typedef struct wifi_manager_s
{
  int sock;                  /* WAPI socket */
  wifi_state_t state;        /* Current WiFi state */
  struct in_addr ip_addr;    /* Assigned IP address */
  char ssid[33];             /* Connected SSID */
} wifi_manager_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/**
 * Initialize WiFi manager
 *
 * Returns:
 *   0 on success, negative errno on failure
 */

int wifi_manager_init(wifi_manager_t *mgr);

/**
 * Connect to WiFi network
 *
 * Parameters:
 *   mgr      - WiFi manager context
 *   ssid     - WiFi SSID
 *   password - WiFi password (NULL for open network)
 *   auth     - Authentication type (WAPI_CRYPT_*)
 *
 * Returns:
 *   0 on success, negative errno on failure
 */

int wifi_manager_connect(wifi_manager_t *mgr, const char *ssid,
                         const char *password, int auth);

/**
 * Disconnect from WiFi network
 *
 * Parameters:
 *   mgr - WiFi manager context
 *
 * Returns:
 *   0 on success, negative errno on failure
 */

int wifi_manager_disconnect(wifi_manager_t *mgr);

/**
 * Get WiFi connection state
 *
 * Parameters:
 *   mgr - WiFi manager context
 *
 * Returns:
 *   Current WiFi state
 */

wifi_state_t wifi_manager_get_state(wifi_manager_t *mgr);

/**
 * Get assigned IP address
 *
 * Parameters:
 *   mgr - WiFi manager context
 *   addr - Pointer to store IP address
 *
 * Returns:
 *   0 on success, negative errno on failure
 */

int wifi_manager_get_ip(wifi_manager_t *mgr, struct in_addr *addr);

/**
 * Cleanup WiFi manager
 *
 * Parameters:
 *   mgr - WiFi manager context
 */

void wifi_manager_cleanup(wifi_manager_t *mgr);

#endif /* __APPS_EXAMPLES_SECURITY_CAMERA_WIFI_MANAGER_H */
