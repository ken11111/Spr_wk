/****************************************************************************
 * apps/examples/security_camera/wifi_manager.c
 *
 * WiFi Manager Implementation (Phase 7)
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

#include <arpa/inet.h>
#include <netinet/in.h>

#include <wireless/wapi.h>

#include "wifi_manager.h"
#include "wifi_config.h"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/**
 * Wait for WiFi connection with timeout
 */

static int wifi_wait_connection(wifi_manager_t *mgr, int timeout_sec)
{
  int ret;
  int elapsed = 0;
  struct in_addr addr;

  _info("Waiting for WiFi connection (timeout: %d sec)...\n", timeout_sec);

  while (elapsed < timeout_sec)
    {
      /* Check if we have an IP address */
      ret = wapi_get_ip(mgr->sock, WIFI_INTERFACE, &addr);
      if (ret == 0 && addr.s_addr != 0)
        {
          mgr->ip_addr = addr;
          mgr->state = WIFI_STATE_READY;
          _info("WiFi connected! IP: %s\n", inet_ntoa(addr));
          return OK;
        }

      sleep(1);
      elapsed++;
    }

  _err("ERROR: WiFi connection timeout\n");
  mgr->state = WIFI_STATE_ERROR;
  return -ETIMEDOUT;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/**
 * Initialize WiFi manager
 */

int wifi_manager_init(wifi_manager_t *mgr)
{
  int ret;

  if (mgr == NULL)
    {
      return -EINVAL;
    }

  memset(mgr, 0, sizeof(wifi_manager_t));

  /* Create WAPI socket */
  mgr->sock = wapi_make_socket();
  if (mgr->sock < 0)
    {
      _err("ERROR: Failed to create WAPI socket: %d\n", mgr->sock);
      return mgr->sock;
    }

  mgr->state = WIFI_STATE_DISCONNECTED;

  _info("WiFi manager initialized\n");
  return OK;
}

/**
 * Connect to WiFi network
 */

int wifi_manager_connect(wifi_manager_t *mgr, const char *ssid,
                         const char *password, int auth)
{
  int ret;

  if (mgr == NULL || ssid == NULL)
    {
      return -EINVAL;
    }

  if (mgr->state == WIFI_STATE_READY)
    {
      _warn("WARNING: Already connected to WiFi\n");
      return OK;
    }

  mgr->state = WIFI_STATE_CONNECTING;
  strncpy(mgr->ssid, ssid, sizeof(mgr->ssid) - 1);

  _info("Connecting to WiFi SSID: %s\n", ssid);

  /* Set WiFi mode to managed (station mode) */
  ret = wapi_set_mode(mgr->sock, WIFI_INTERFACE, WAPI_MODE_MANAGED);
  if (ret < 0)
    {
      _err("ERROR: Failed to set WiFi mode: %d\n", ret);
      mgr->state = WIFI_STATE_ERROR;
      return ret;
    }

  /* Configure WPA connection */
  if (password != NULL)
    {
      struct wpa_wconfig_s wconfig;
      memset(&wconfig, 0, sizeof(wconfig));

      wconfig.sta_mode = WAPI_MODE_MANAGED;
      wconfig.auth_wpa = IW_AUTH_WPA_VERSION_WPA2;
      wconfig.cipher_mode = IW_AUTH_CIPHER_CCMP;
      wconfig.alg = WPA_ALG_CCMP;
      wconfig.ifname = WIFI_INTERFACE;
      wconfig.ssid = ssid;
      wconfig.ssidlen = strlen(ssid);
      wconfig.passphrase = password;
      wconfig.phraselen = strlen(password);
      wconfig.bssid = NULL;
      wconfig.flag = WAPI_FREQ_AUTO;

      ret = wpa_driver_wext_associate(&wconfig);
      if (ret < 0)
        {
          _err("ERROR: Failed to associate with WPA: %d\n", ret);
          mgr->state = WIFI_STATE_ERROR;
          return ret;
        }
    }
  else
    {
      /* Open network (no encryption) */
      ret = wapi_set_essid(mgr->sock, WIFI_INTERFACE, ssid, WAPI_ESSID_ON);
      if (ret < 0)
        {
          _err("ERROR: Failed to set SSID: %d\n", ret);
          mgr->state = WIFI_STATE_ERROR;
          return ret;
        }

      ret = wapi_connect(mgr->sock, WIFI_INTERFACE);
      if (ret < 0)
        {
          _err("ERROR: Failed to connect to WiFi: %d\n", ret);
          mgr->state = WIFI_STATE_ERROR;
          return ret;
        }
    }

  mgr->state = WIFI_STATE_CONNECTED;

#if USE_DHCP
  /* Start DHCP client */
  _info("Starting DHCP client...\n");
  ret = wapi_start_dhcp(mgr->sock, WIFI_INTERFACE);
  if (ret < 0)
    {
      _warn("WARNING: Failed to start DHCP: %d\n", ret);
      /* Continue anyway, might have static IP */
    }

  /* Wait for IP address */
  ret = wifi_wait_connection(mgr, DHCP_TIMEOUT_SEC);
  if (ret < 0)
    {
      return ret;
    }
#else
  /* Static IP configuration */
  struct in_addr addr;
  struct in_addr netmask;
  struct in_addr gateway;

  inet_aton(STATIC_IP_ADDR, &addr);
  inet_aton(STATIC_NETMASK, &netmask);
  inet_aton(STATIC_GATEWAY, &gateway);

  ret = wapi_set_ip(mgr->sock, WIFI_INTERFACE, &addr);
  if (ret < 0)
    {
      _err("ERROR: Failed to set IP address: %d\n", ret);
      mgr->state = WIFI_STATE_ERROR;
      return ret;
    }

  ret = wapi_set_netmask(mgr->sock, WIFI_INTERFACE, &netmask);
  if (ret < 0)
    {
      _err("ERROR: Failed to set netmask: %d\n", ret);
      mgr->state = WIFI_STATE_ERROR;
      return ret;
    }

  ret = wapi_add_route_gw(mgr->sock, WAPI_ROUTE_TARGET_NET,
                          &addr, &netmask, &gateway);
  if (ret < 0)
    {
      _warn("WARNING: Failed to set default gateway: %d\n", ret);
    }

  mgr->ip_addr = addr;
  mgr->state = WIFI_STATE_READY;

  _info("WiFi connected with static IP: %s\n", STATIC_IP_ADDR);
#endif

  return OK;
}

/**
 * Disconnect from WiFi network
 */

int wifi_manager_disconnect(wifi_manager_t *mgr)
{
  int ret;

  if (mgr == NULL)
    {
      return -EINVAL;
    }

  if (mgr->state == WIFI_STATE_DISCONNECTED)
    {
      return OK;
    }

  _info("Disconnecting from WiFi...\n");

  ret = wapi_disconnect(mgr->sock, WIFI_INTERFACE);
  if (ret < 0)
    {
      _warn("WARNING: Failed to disconnect WiFi: %d\n", ret);
    }

  mgr->state = WIFI_STATE_DISCONNECTED;
  memset(&mgr->ip_addr, 0, sizeof(mgr->ip_addr));

  _info("WiFi disconnected\n");
  return OK;
}

/**
 * Get WiFi connection state
 */

wifi_state_t wifi_manager_get_state(wifi_manager_t *mgr)
{
  if (mgr == NULL)
    {
      return WIFI_STATE_ERROR;
    }

  return mgr->state;
}

/**
 * Get assigned IP address
 */

int wifi_manager_get_ip(wifi_manager_t *mgr, struct in_addr *addr)
{
  if (mgr == NULL || addr == NULL)
    {
      return -EINVAL;
    }

  if (mgr->state != WIFI_STATE_READY)
    {
      return -ENOTCONN;
    }

  *addr = mgr->ip_addr;
  return OK;
}

/**
 * Cleanup WiFi manager
 */

void wifi_manager_cleanup(wifi_manager_t *mgr)
{
  if (mgr == NULL)
    {
      return;
    }

  if (mgr->state != WIFI_STATE_DISCONNECTED)
    {
      wifi_manager_disconnect(mgr);
    }

  if (mgr->sock >= 0)
    {
      close(mgr->sock);
      mgr->sock = -1;
    }

  _info("WiFi manager cleaned up\n");
}
