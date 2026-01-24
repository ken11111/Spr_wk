/****************************************************************************
 * apps/examples/security_camera/wifi_config.h
 *
 * WiFi Configuration (Phase 7)
 *
 ****************************************************************************/

#ifndef __APPS_EXAMPLES_SECURITY_CAMERA_WIFI_CONFIG_H
#define __APPS_EXAMPLES_SECURITY_CAMERA_WIFI_CONFIG_H

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* WiFi Network Configuration */

#define WIFI_SSID        "DESKTOP-GPU979R"    /* WiFi SSID */
#define WIFI_PASSWORD    "B54p3530"           /* WiFi WPA2 password */
#define WIFI_AUTH        0   /* Unused: auth is hardcoded in wifi_manager.c */

/* TCP Server Configuration */

#define TCP_SERVER_PORT  8888                 /* TCP port for MJPEG streaming */
#define TCP_SERVER_BACKLOG 1                  /* Max pending connections */

/* WiFi Interface */

#define WIFI_INTERFACE   "wlan0"              /* WiFi network interface name */

/* Network Configuration (if not using DHCP) */

#define USE_DHCP         1                    /* 1 = DHCP, 0 = Static IP */

#if !USE_DHCP
#  define STATIC_IP_ADDR   "192.168.1.100"   /* Static IP address */
#  define STATIC_NETMASK   "255.255.255.0"   /* Subnet mask */
#  define STATIC_GATEWAY   "192.168.1.1"     /* Default gateway */
#endif

/* Connection Timeout */

#define WIFI_CONNECT_TIMEOUT_SEC  15          /* WiFi connection timeout (seconds) */
#define DHCP_TIMEOUT_SEC          10          /* DHCP timeout (seconds) */

#endif /* __APPS_EXAMPLES_SECURITY_CAMERA_WIFI_CONFIG_H */
