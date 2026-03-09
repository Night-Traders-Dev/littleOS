/* net.h - Networking support for littleOS (Pico W / CYW43) */
#ifndef LITTLEOS_NET_H
#define LITTLEOS_NET_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Network configuration
 * ============================================================================ */

#define NET_SSID_MAX        32
#define NET_PASS_MAX        64
#define NET_IP_STR_MAX      16
#define NET_HOSTNAME_MAX    32
#define NET_MAX_SOCKETS     4
#define NET_RECV_BUF_SIZE   1024

/* Network status */
typedef enum {
    NET_STATUS_DOWN         = 0,
    NET_STATUS_CONNECTING   = 1,
    NET_STATUS_CONNECTED    = 2,
    NET_STATUS_GOT_IP       = 3,
    NET_STATUS_ERROR        = 4,
} net_status_t;

/* WiFi auth mode */
typedef enum {
    NET_AUTH_OPEN       = 0,
    NET_AUTH_WPA_PSK    = 1,
    NET_AUTH_WPA2_PSK   = 2,
    NET_AUTH_WPA_WPA2   = 3,
} net_auth_t;

/* IP address (IPv4) */
typedef struct {
    uint8_t addr[4];
} net_ip4_t;

/* Network interface info */
typedef struct {
    net_status_t    status;
    net_ip4_t       ip;
    net_ip4_t       netmask;
    net_ip4_t       gateway;
    net_ip4_t       dns;
    char            ssid[NET_SSID_MAX];
    char            hostname[NET_HOSTNAME_MAX];
    int8_t          rssi;           /* Signal strength in dBm */
    uint8_t         mac[6];
    uint32_t        uptime_ms;      /* Time since connection */
    uint32_t        tx_bytes;
    uint32_t        rx_bytes;
} net_info_t;

/* Socket types */
typedef enum {
    NET_SOCK_TCP    = 0,
    NET_SOCK_UDP    = 1,
} net_sock_type_t;

/* Socket state */
typedef enum {
    SOCK_CLOSED     = 0,
    SOCK_LISTENING  = 1,
    SOCK_CONNECTED  = 2,
    SOCK_ERROR      = 3,
} net_sock_state_t;

/* WiFi scan result */
typedef struct {
    char        ssid[NET_SSID_MAX];
    int8_t      rssi;
    uint8_t     channel;
    net_auth_t  auth;
    uint8_t     bssid[6];
} net_scan_result_t;

#define NET_MAX_SCAN_RESULTS    16

/* ============================================================================
 * Public API - WiFi
 * ============================================================================ */

/* Initialize networking subsystem */
int net_init(void);

/* Connect to WiFi network */
int net_wifi_connect(const char *ssid, const char *password, uint32_t timeout_ms);

/* Disconnect from WiFi */
int net_wifi_disconnect(void);

/* Scan for WiFi networks */
int net_wifi_scan(net_scan_result_t *results, int max_results);

/* Get network interface info */
int net_get_info(net_info_t *info);

/* Get current status */
net_status_t net_get_status(void);

/* Set hostname */
int net_set_hostname(const char *hostname);

/* ============================================================================
 * Public API - Sockets (TCP/UDP)
 * ============================================================================ */

/* Create a socket */
int net_socket_create(net_sock_type_t type);

/* Close a socket */
int net_socket_close(int sock_id);

/* Connect to remote host (TCP) */
int net_socket_connect(int sock_id, net_ip4_t ip, uint16_t port, uint32_t timeout_ms);

/* Listen on a port (TCP server) */
int net_socket_listen(int sock_id, uint16_t port);

/* Accept incoming connection (TCP server) */
int net_socket_accept(int sock_id, int *new_sock_id);

/* Send data */
int net_socket_send(int sock_id, const void *data, size_t len);

/* Receive data (non-blocking) */
int net_socket_recv(int sock_id, void *data, size_t max_len);

/* Send UDP datagram */
int net_socket_sendto(int sock_id, net_ip4_t ip, uint16_t port,
                      const void *data, size_t len);

/* ============================================================================
 * Public API - Utilities
 * ============================================================================ */

/* DNS lookup */
int net_dns_lookup(const char *hostname, net_ip4_t *ip);

/* Ping host */
int net_ping(net_ip4_t ip, uint32_t timeout_ms);

/* HTTP GET (simple, blocking) */
int net_http_get(const char *url, char *response_buf, size_t buf_size);

/* Format IP address to string */
void net_ip4_to_str(net_ip4_t ip, char *buf, size_t buf_size);

/* Parse IP string to net_ip4_t */
int net_str_to_ip4(const char *str, net_ip4_t *ip);

/* Poll network stack (call periodically from main loop) */
void net_poll(void);

/* ============================================================================
 * Error codes
 * ============================================================================ */

#define NET_OK              0
#define NET_ERR_INIT        (-1)
#define NET_ERR_CONNECT     (-2)
#define NET_ERR_TIMEOUT     (-3)
#define NET_ERR_NO_RESOURCE (-4)
#define NET_ERR_NOT_FOUND   (-5)
#define NET_ERR_IO          (-6)
#define NET_ERR_CLOSED      (-7)
#define NET_ERR_INVALID     (-8)
#define NET_ERR_NOT_SUPPORTED (-9)

#ifdef __cplusplus
}
#endif

#endif /* LITTLEOS_NET_H */
