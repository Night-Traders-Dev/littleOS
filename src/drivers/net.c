/* net.c - Networking driver for littleOS (Pico W / CYW43439) */

#include "net.h"
#include "dmesg.h"
#include <stdio.h>
#include <string.h>

#ifdef PICO_W
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"
#include "lwip/dns.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/raw.h"
#include "lwip/icmp.h"
#include "lwip/inet_chksum.h"
#include "lwip/ip4.h"
#endif

/* ============================================================================
 * Utility functions (available on all builds)
 * ============================================================================ */

void net_ip4_to_str(net_ip4_t ip, char *buf, size_t buf_size) {
    snprintf(buf, buf_size, "%d.%d.%d.%d",
             ip.addr[0], ip.addr[1], ip.addr[2], ip.addr[3]);
}

int net_str_to_ip4(const char *str, net_ip4_t *ip) {
    if (!str || !ip) return NET_ERR_INVALID;

    unsigned int a, b, c, d;
    if (sscanf(str, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
        return NET_ERR_INVALID;
    }
    if (a > 255 || b > 255 || c > 255 || d > 255) {
        return NET_ERR_INVALID;
    }

    ip->addr[0] = (uint8_t)a;
    ip->addr[1] = (uint8_t)b;
    ip->addr[2] = (uint8_t)c;
    ip->addr[3] = (uint8_t)d;
    return NET_OK;
}

void net_poll(void) {
#ifdef PICO_W
    cyw43_arch_poll();
#endif
}

/* ============================================================================
 * PICO_W implementation
 * ============================================================================ */

#ifdef PICO_W

/* ---------- Internal socket structure ---------- */

typedef struct {
    bool            in_use;
    net_sock_type_t type;
    net_sock_state_t state;
    struct tcp_pcb  *tcp_pcb;
    struct udp_pcb  *udp_pcb;
    uint8_t         recv_buf[NET_RECV_BUF_SIZE];
    volatile uint32_t recv_head;
    volatile uint32_t recv_tail;
    bool            connect_done;
    err_t           connect_err;
} net_socket_t;

static net_socket_t sockets[NET_MAX_SOCKETS];
static bool net_initialized = false;
static char current_hostname[NET_HOSTNAME_MAX] = "littleos";
static char connected_ssid[NET_SSID_MAX] = {0};
static uint32_t connect_time_ms = 0;
static uint32_t tx_total = 0;
static uint32_t rx_total = 0;

/* ---------- WiFi scan state ---------- */

static net_scan_result_t *scan_results_ptr = NULL;
static int scan_max = 0;
static volatile int scan_count = 0;
static volatile bool scan_complete = false;

/* ---------- Ring buffer helpers ---------- */

static uint32_t ring_available(const net_socket_t *s) {
    return (s->recv_head - s->recv_tail) % NET_RECV_BUF_SIZE;
}

static void ring_push(net_socket_t *s, const uint8_t *data, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        uint32_t next = (s->recv_head + 1) % NET_RECV_BUF_SIZE;
        if (next == s->recv_tail) break;  /* buffer full, drop */
        s->recv_buf[s->recv_head] = data[i];
        s->recv_head = next;
    }
}

static uint32_t ring_pop(net_socket_t *s, uint8_t *data, uint32_t max_len) {
    uint32_t count = 0;
    while (count < max_len && s->recv_tail != s->recv_head) {
        data[count++] = s->recv_buf[s->recv_tail];
        s->recv_tail = (s->recv_tail + 1) % NET_RECV_BUF_SIZE;
    }
    return count;
}

/* ---------- TCP callbacks ---------- */

static err_t tcp_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    int sock_id = (int)(intptr_t)arg;
    if (sock_id < 0 || sock_id >= NET_MAX_SOCKETS) return ERR_ARG;

    net_socket_t *s = &sockets[sock_id];

    if (p == NULL) {
        /* Remote closed connection */
        s->state = SOCK_CLOSED;
        return ERR_OK;
    }

    if (err != ERR_OK) {
        pbuf_free(p);
        return err;
    }

    /* Copy pbuf chain into ring buffer */
    struct pbuf *q;
    for (q = p; q != NULL; q = q->next) {
        ring_push(s, (const uint8_t *)q->payload, q->len);
        rx_total += q->len;
    }

    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

static void tcp_err_cb(void *arg, err_t err) {
    int sock_id = (int)(intptr_t)arg;
    if (sock_id < 0 || sock_id >= NET_MAX_SOCKETS) return;

    (void)err;
    sockets[sock_id].state = SOCK_ERROR;
    sockets[sock_id].tcp_pcb = NULL; /* lwIP frees it on error */
}

static err_t tcp_connect_cb(void *arg, struct tcp_pcb *tpcb, err_t err) {
    int sock_id = (int)(intptr_t)arg;
    if (sock_id < 0 || sock_id >= NET_MAX_SOCKETS) return ERR_ARG;

    (void)tpcb;
    sockets[sock_id].connect_err = err;
    sockets[sock_id].connect_done = true;

    if (err == ERR_OK) {
        sockets[sock_id].state = SOCK_CONNECTED;
    } else {
        sockets[sock_id].state = SOCK_ERROR;
    }
    return ERR_OK;
}

/* ---------- Scan callback ---------- */

static int scan_cb(void *env, const cyw43_ev_scan_result_t *result) {
    (void)env;
    if (!result || scan_count >= scan_max) return 0;

    net_scan_result_t *r = &scan_results_ptr[scan_count];
    memset(r, 0, sizeof(*r));

    size_t ssid_len = result->ssid_len;
    if (ssid_len >= NET_SSID_MAX) ssid_len = NET_SSID_MAX - 1;
    memcpy(r->ssid, result->ssid, ssid_len);
    r->ssid[ssid_len] = '\0';

    r->rssi = result->rssi;
    r->channel = result->channel;
    memcpy(r->bssid, result->bssid, 6);

    /* Map auth type */
    if (result->auth_mode == 0) {
        r->auth = NET_AUTH_OPEN;
    } else if (result->auth_mode & 0x04) {
        r->auth = NET_AUTH_WPA2_PSK;
    } else if (result->auth_mode & 0x02) {
        r->auth = NET_AUTH_WPA_PSK;
    } else {
        r->auth = NET_AUTH_WPA_WPA2;
    }

    scan_count++;
    return 0;
}

/* ---------- WiFi API ---------- */

int net_init(void) {
    if (net_initialized) return NET_OK;

    memset(sockets, 0, sizeof(sockets));

    if (cyw43_arch_init()) {
        dmesg_err("net: CYW43 init failed");
        return NET_ERR_INIT;
    }

    cyw43_arch_enable_sta_mode();
    net_initialized = true;
    dmesg_info("net: initialized (CYW43439)");
    return NET_OK;
}

int net_wifi_connect(const char *ssid, const char *password, uint32_t timeout_ms) {
    if (!net_initialized) return NET_ERR_INIT;
    if (!ssid) return NET_ERR_INVALID;

    dmesg_info("net: connecting to '%s'...", ssid);

    uint32_t auth = CYW43_AUTH_WPA2_AES_PSK;
    if (!password || password[0] == '\0') {
        auth = CYW43_AUTH_OPEN;
    }

    int err = cyw43_arch_wifi_connect_timeout_ms(ssid, password, auth, timeout_ms);
    if (err != 0) {
        dmesg_err("net: WiFi connect failed (err=%d)", err);
        return NET_ERR_CONNECT;
    }

    strncpy(connected_ssid, ssid, NET_SSID_MAX - 1);
    connected_ssid[NET_SSID_MAX - 1] = '\0';
    connect_time_ms = to_ms_since_boot(get_absolute_time());

    dmesg_info("net: connected to '%s'", ssid);
    return NET_OK;
}

int net_wifi_disconnect(void) {
    if (!net_initialized) return NET_ERR_INIT;

    /* Close all open sockets */
    for (int i = 0; i < NET_MAX_SOCKETS; i++) {
        if (sockets[i].in_use) {
            net_socket_close(i);
        }
    }

    cyw43_arch_deinit();
    net_initialized = false;
    connected_ssid[0] = '\0';
    connect_time_ms = 0;

    dmesg_info("net: disconnected");
    return NET_OK;
}

int net_wifi_scan(net_scan_result_t *results, int max_results) {
    if (!net_initialized) return NET_ERR_INIT;
    if (!results || max_results <= 0) return NET_ERR_INVALID;

    scan_results_ptr = results;
    scan_max = (max_results > NET_MAX_SCAN_RESULTS) ? NET_MAX_SCAN_RESULTS : max_results;
    scan_count = 0;
    scan_complete = false;

    cyw43_wifi_scan_options_t opts = {0};
    int err = cyw43_wifi_scan(&cyw43_state, &opts, NULL, scan_cb);
    if (err != 0) {
        dmesg_err("net: scan failed (err=%d)", err);
        return NET_ERR_IO;
    }

    /* Poll until scan completes */
    uint32_t start = to_ms_since_boot(get_absolute_time());
    while (cyw43_wifi_scan_active(&cyw43_state)) {
        cyw43_arch_poll();
        sleep_ms(100);
        uint32_t elapsed = to_ms_since_boot(get_absolute_time()) - start;
        if (elapsed > 10000) {
            dmesg_warn("net: scan timed out");
            break;
        }
    }

    dmesg_info("net: scan found %d networks", scan_count);
    return scan_count;
}

int net_get_info(net_info_t *info) {
    if (!info) return NET_ERR_INVALID;
    if (!net_initialized) return NET_ERR_INIT;

    memset(info, 0, sizeof(*info));

    info->status = net_get_status();
    strncpy(info->ssid, connected_ssid, NET_SSID_MAX - 1);
    info->ssid[NET_SSID_MAX - 1] = '\0';
    strncpy(info->hostname, current_hostname, NET_HOSTNAME_MAX - 1);
    info->hostname[NET_HOSTNAME_MAX - 1] = '\0';
    info->tx_bytes = tx_total;
    info->rx_bytes = rx_total;

    if (connect_time_ms > 0) {
        info->uptime_ms = to_ms_since_boot(get_absolute_time()) - connect_time_ms;
    }

    /* Get IP info from lwIP netif */
    if (netif_default != NULL) {
        uint32_t ip_raw = ip4_addr_get_u32(netif_ip4_addr(netif_default));
        info->ip.addr[0] = (ip_raw >>  0) & 0xFF;
        info->ip.addr[1] = (ip_raw >>  8) & 0xFF;
        info->ip.addr[2] = (ip_raw >> 16) & 0xFF;
        info->ip.addr[3] = (ip_raw >> 24) & 0xFF;

        uint32_t nm_raw = ip4_addr_get_u32(netif_ip4_netmask(netif_default));
        info->netmask.addr[0] = (nm_raw >>  0) & 0xFF;
        info->netmask.addr[1] = (nm_raw >>  8) & 0xFF;
        info->netmask.addr[2] = (nm_raw >> 16) & 0xFF;
        info->netmask.addr[3] = (nm_raw >> 24) & 0xFF;

        uint32_t gw_raw = ip4_addr_get_u32(netif_ip4_gw(netif_default));
        info->gateway.addr[0] = (gw_raw >>  0) & 0xFF;
        info->gateway.addr[1] = (gw_raw >>  8) & 0xFF;
        info->gateway.addr[2] = (gw_raw >> 16) & 0xFF;
        info->gateway.addr[3] = (gw_raw >> 24) & 0xFF;

        memcpy(info->mac, netif_default->hwaddr, 6);
    }

    /* Get RSSI */
    int32_t rssi = 0;
    cyw43_wifi_get_rssi(&cyw43_state, &rssi);
    info->rssi = (int8_t)rssi;

    return NET_OK;
}

net_status_t net_get_status(void) {
    if (!net_initialized) return NET_STATUS_DOWN;

    int link = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
    switch (link) {
        case CYW43_LINK_DOWN:       return NET_STATUS_DOWN;
        case CYW43_LINK_JOIN:       return NET_STATUS_CONNECTING;
        case CYW43_LINK_NOIP:       return NET_STATUS_CONNECTED;
        case CYW43_LINK_UP:         return NET_STATUS_GOT_IP;
        case CYW43_LINK_FAIL:
        case CYW43_LINK_NONET:
        case CYW43_LINK_BADAUTH:
        default:                    return NET_STATUS_ERROR;
    }
}

int net_set_hostname(const char *hostname) {
    if (!hostname) return NET_ERR_INVALID;
    strncpy(current_hostname, hostname, NET_HOSTNAME_MAX - 1);
    current_hostname[NET_HOSTNAME_MAX - 1] = '\0';

    if (netif_default != NULL) {
        netif_set_hostname(netif_default, current_hostname);
    }

    return NET_OK;
}

/* ---------- Socket API ---------- */

int net_socket_create(net_sock_type_t type) {
    /* Find free slot */
    int id = -1;
    for (int i = 0; i < NET_MAX_SOCKETS; i++) {
        if (!sockets[i].in_use) { id = i; break; }
    }
    if (id < 0) return NET_ERR_NO_RESOURCE;

    net_socket_t *s = &sockets[id];
    memset(s, 0, sizeof(*s));
    s->in_use = true;
    s->type = type;
    s->state = SOCK_CLOSED;

    if (type == NET_SOCK_TCP) {
        s->tcp_pcb = tcp_new();
        if (!s->tcp_pcb) {
            s->in_use = false;
            return NET_ERR_NO_RESOURCE;
        }
        tcp_arg(s->tcp_pcb, (void *)(intptr_t)id);
        tcp_recv(s->tcp_pcb, tcp_recv_cb);
        tcp_err(s->tcp_pcb, tcp_err_cb);
    } else {
        s->udp_pcb = udp_new();
        if (!s->udp_pcb) {
            s->in_use = false;
            return NET_ERR_NO_RESOURCE;
        }
    }

    return id;
}

int net_socket_close(int sock_id) {
    if (sock_id < 0 || sock_id >= NET_MAX_SOCKETS) return NET_ERR_INVALID;
    net_socket_t *s = &sockets[sock_id];
    if (!s->in_use) return NET_ERR_INVALID;

    if (s->type == NET_SOCK_TCP && s->tcp_pcb) {
        tcp_arg(s->tcp_pcb, NULL);
        tcp_recv(s->tcp_pcb, NULL);
        tcp_err(s->tcp_pcb, NULL);
        tcp_close(s->tcp_pcb);
        s->tcp_pcb = NULL;
    } else if (s->type == NET_SOCK_UDP && s->udp_pcb) {
        udp_remove(s->udp_pcb);
        s->udp_pcb = NULL;
    }

    s->in_use = false;
    s->state = SOCK_CLOSED;
    return NET_OK;
}

int net_socket_connect(int sock_id, net_ip4_t ip, uint16_t port, uint32_t timeout_ms) {
    if (sock_id < 0 || sock_id >= NET_MAX_SOCKETS) return NET_ERR_INVALID;
    net_socket_t *s = &sockets[sock_id];
    if (!s->in_use || s->type != NET_SOCK_TCP || !s->tcp_pcb) return NET_ERR_INVALID;

    ip_addr_t remote;
    IP4_ADDR(&remote, ip.addr[0], ip.addr[1], ip.addr[2], ip.addr[3]);

    s->connect_done = false;
    s->connect_err = ERR_INPROGRESS;

    err_t err = tcp_connect(s->tcp_pcb, &remote, port, tcp_connect_cb);
    if (err != ERR_OK) {
        s->state = SOCK_ERROR;
        return NET_ERR_CONNECT;
    }

    /* Poll until connected or timeout */
    uint32_t start = to_ms_since_boot(get_absolute_time());
    while (!s->connect_done) {
        cyw43_arch_poll();
        sleep_ms(10);
        uint32_t elapsed = to_ms_since_boot(get_absolute_time()) - start;
        if (elapsed >= timeout_ms) {
            s->state = SOCK_ERROR;
            return NET_ERR_TIMEOUT;
        }
    }

    if (s->connect_err != ERR_OK) {
        s->state = SOCK_ERROR;
        return NET_ERR_CONNECT;
    }

    return NET_OK;
}

int net_socket_listen(int sock_id, uint16_t port) {
    if (sock_id < 0 || sock_id >= NET_MAX_SOCKETS) return NET_ERR_INVALID;
    net_socket_t *s = &sockets[sock_id];
    if (!s->in_use || s->type != NET_SOCK_TCP || !s->tcp_pcb) return NET_ERR_INVALID;

    err_t err = tcp_bind(s->tcp_pcb, IP_ADDR_ANY, port);
    if (err != ERR_OK) return NET_ERR_IO;

    struct tcp_pcb *listen_pcb = tcp_listen(s->tcp_pcb);
    if (!listen_pcb) return NET_ERR_NO_RESOURCE;

    s->tcp_pcb = listen_pcb;
    s->state = SOCK_LISTENING;
    tcp_arg(s->tcp_pcb, (void *)(intptr_t)sock_id);
    return NET_OK;
}

int net_socket_accept(int sock_id, int *new_sock_id) {
    /* Accept is handled via lwIP callback; this is a simplified stub.
     * A full implementation would use tcp_accept() callback to populate
     * a new socket slot. For now, return not-supported. */
    (void)sock_id;
    (void)new_sock_id;
    return NET_ERR_NOT_SUPPORTED;
}

int net_socket_send(int sock_id, const void *data, size_t len) {
    if (sock_id < 0 || sock_id >= NET_MAX_SOCKETS) return NET_ERR_INVALID;
    net_socket_t *s = &sockets[sock_id];
    if (!s->in_use || s->state != SOCK_CONNECTED) return NET_ERR_CLOSED;

    if (s->type == NET_SOCK_TCP) {
        if (!s->tcp_pcb) return NET_ERR_CLOSED;

        err_t err = tcp_write(s->tcp_pcb, data, (uint16_t)len, TCP_WRITE_FLAG_COPY);
        if (err != ERR_OK) return NET_ERR_IO;

        err = tcp_output(s->tcp_pcb);
        if (err != ERR_OK) return NET_ERR_IO;

        tx_total += len;
        return (int)len;
    }

    return NET_ERR_INVALID;
}

int net_socket_recv(int sock_id, void *data, size_t max_len) {
    if (sock_id < 0 || sock_id >= NET_MAX_SOCKETS) return NET_ERR_INVALID;
    net_socket_t *s = &sockets[sock_id];
    if (!s->in_use) return NET_ERR_CLOSED;

    if (s->state == SOCK_CLOSED && ring_available(s) == 0) {
        return NET_ERR_CLOSED;
    }

    uint32_t count = ring_pop(s, (uint8_t *)data, (uint32_t)max_len);
    if (count == 0 && s->state == SOCK_ERROR) return NET_ERR_IO;
    return (int)count;
}

int net_socket_sendto(int sock_id, net_ip4_t ip, uint16_t port,
                      const void *data, size_t len)
{
    if (sock_id < 0 || sock_id >= NET_MAX_SOCKETS) return NET_ERR_INVALID;
    net_socket_t *s = &sockets[sock_id];
    if (!s->in_use || s->type != NET_SOCK_UDP || !s->udp_pcb) return NET_ERR_INVALID;

    ip_addr_t remote;
    IP4_ADDR(&remote, ip.addr[0], ip.addr[1], ip.addr[2], ip.addr[3]);

    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, (uint16_t)len, PBUF_RAM);
    if (!p) return NET_ERR_NO_RESOURCE;

    memcpy(p->payload, data, len);
    err_t err = udp_sendto(s->udp_pcb, p, &remote, port);
    pbuf_free(p);

    if (err != ERR_OK) return NET_ERR_IO;

    tx_total += len;
    return (int)len;
}

/* ---------- DNS ---------- */

static volatile bool dns_done = false;
static ip_addr_t dns_result_addr;

static void dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *arg) {
    (void)name;
    (void)arg;
    if (ipaddr) {
        dns_result_addr = *ipaddr;
    } else {
        ip_addr_set_zero(&dns_result_addr);
    }
    dns_done = true;
}

int net_dns_lookup(const char *hostname, net_ip4_t *ip) {
    if (!hostname || !ip) return NET_ERR_INVALID;
    if (!net_initialized) return NET_ERR_INIT;

    ip_addr_t addr;
    dns_done = false;

    err_t err = dns_gethostbyname(hostname, &addr, dns_found_cb, NULL);
    if (err == ERR_OK) {
        /* Already cached */
        uint32_t raw = ip4_addr_get_u32(ip_2_ip4(&addr));
        ip->addr[0] = (raw >>  0) & 0xFF;
        ip->addr[1] = (raw >>  8) & 0xFF;
        ip->addr[2] = (raw >> 16) & 0xFF;
        ip->addr[3] = (raw >> 24) & 0xFF;
        return NET_OK;
    }

    if (err != ERR_INPROGRESS) return NET_ERR_NOT_FOUND;

    /* Wait for callback */
    uint32_t start = to_ms_since_boot(get_absolute_time());
    while (!dns_done) {
        cyw43_arch_poll();
        sleep_ms(10);
        if (to_ms_since_boot(get_absolute_time()) - start > 5000) {
            return NET_ERR_TIMEOUT;
        }
    }

    if (ip_addr_isany(&dns_result_addr)) return NET_ERR_NOT_FOUND;

    uint32_t raw = ip4_addr_get_u32(ip_2_ip4(&dns_result_addr));
    ip->addr[0] = (raw >>  0) & 0xFF;
    ip->addr[1] = (raw >>  8) & 0xFF;
    ip->addr[2] = (raw >> 16) & 0xFF;
    ip->addr[3] = (raw >> 24) & 0xFF;
    return NET_OK;
}

/* ---------- Ping / HTTP stubs ---------- */

/* ---------- Ping (ICMP Echo) ---------- */

static volatile bool ping_reply_received = false;
static uint32_t ping_rtt_ms = 0;

static uint8_t ping_recv_cb(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr) {
    (void)arg;
    (void)pcb;
    (void)addr;

    /* Check for ICMP echo reply (type 0) */
    if (p->len >= 8) {
        uint8_t *data = (uint8_t *)p->payload;
        /* Skip IP header (20 bytes minimum) */
        if (p->len >= 28 && data[20] == 0) {  /* Type 0 = echo reply */
            uint16_t id = (uint16_t)((data[24] << 8) | data[25]);
            if (id == 0x4C4F) {  /* "LO" = littleOS */
                ping_reply_received = true;
                pbuf_free(p);
                return 1;  /* consumed */
            }
        }
    }
    return 0;  /* not consumed */
}

int net_ping(net_ip4_t ip, uint32_t timeout_ms) {
    if (!net_initialized) return NET_ERR_INIT;

    struct raw_pcb *pcb = raw_new(IP_PROTO_ICMP);
    if (!pcb) return NET_ERR_NO_RESOURCE;

    raw_recv(pcb, ping_recv_cb, NULL);
    raw_bind(pcb, IP_ADDR_ANY);

    /* Build ICMP echo request */
    struct pbuf *p = pbuf_alloc(PBUF_IP, 32, PBUF_RAM);
    if (!p) {
        raw_remove(pcb);
        return NET_ERR_NO_RESOURCE;
    }

    uint8_t *icmp = (uint8_t *)p->payload;
    memset(icmp, 0, 32);
    icmp[0] = 8;       /* Type: echo request */
    icmp[1] = 0;       /* Code */
    icmp[4] = 0x4C;    /* ID high: 'L' */
    icmp[5] = 0x4F;    /* ID low:  'O' */
    icmp[6] = 0;       /* Seq high */
    icmp[7] = 1;       /* Seq low */

    /* Compute ICMP checksum */
    uint32_t sum = 0;
    for (int i = 0; i < 32; i += 2) {
        sum += (uint32_t)((icmp[i] << 8) | icmp[i + 1]);
    }
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    uint16_t cksum = (uint16_t)~sum;
    icmp[2] = (uint8_t)(cksum >> 8);
    icmp[3] = (uint8_t)(cksum & 0xFF);

    ip_addr_t remote;
    IP4_ADDR(&remote, ip.addr[0], ip.addr[1], ip.addr[2], ip.addr[3]);

    ping_reply_received = false;
    uint32_t start = to_ms_since_boot(get_absolute_time());

    raw_sendto(pcb, p, &remote);
    pbuf_free(p);

    /* Wait for reply or timeout */
    while (!ping_reply_received) {
        cyw43_arch_poll();
        sleep_ms(1);
        uint32_t elapsed = to_ms_since_boot(get_absolute_time()) - start;
        if (elapsed >= timeout_ms) {
            raw_remove(pcb);
            return NET_ERR_TIMEOUT;
        }
    }

    ping_rtt_ms = to_ms_since_boot(get_absolute_time()) - start;
    raw_remove(pcb);
    return (int)ping_rtt_ms;
}

int net_http_get(const char *url, char *response_buf, size_t buf_size) {
    if (!url || !response_buf || buf_size == 0) return NET_ERR_INVALID;
    if (!net_initialized) return NET_ERR_INIT;

    /* Parse URL: http://hostname[:port]/path */
    const char *p = url;
    if (strncmp(p, "http://", 7) == 0) p += 7;

    char host[64];
    uint16_t port = 80;
    const char *path = "/";

    /* Extract host */
    const char *slash = strchr(p, '/');
    const char *colon = strchr(p, ':');
    size_t host_len;

    if (colon && (!slash || colon < slash)) {
        host_len = (size_t)(colon - p);
        port = (uint16_t)atoi(colon + 1);
    } else if (slash) {
        host_len = (size_t)(slash - p);
    } else {
        host_len = strlen(p);
    }

    if (host_len >= sizeof(host)) host_len = sizeof(host) - 1;
    memcpy(host, p, host_len);
    host[host_len] = '\0';

    if (slash) path = slash;

    /* Resolve hostname */
    net_ip4_t ip;
    if (net_str_to_ip4(host, &ip) != NET_OK) {
        int err = net_dns_lookup(host, &ip);
        if (err != NET_OK) return err;
    }

    /* Create TCP socket and connect */
    int sock = net_socket_create(NET_SOCK_TCP);
    if (sock < 0) return sock;

    int err = net_socket_connect(sock, ip, port, 5000);
    if (err != NET_OK) {
        net_socket_close(sock);
        return err;
    }

    /* Send HTTP GET request */
    char request[256];
    int rlen = snprintf(request, sizeof(request),
                        "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n",
                        path, host);
    if (rlen <= 0 || (size_t)rlen >= sizeof(request)) {
        net_socket_close(sock);
        return NET_ERR_INVALID;
    }

    err = net_socket_send(sock, request, (size_t)rlen);
    if (err < 0) {
        net_socket_close(sock);
        return err;
    }

    /* Receive response */
    size_t total = 0;
    uint32_t start = to_ms_since_boot(get_absolute_time());

    while (total < buf_size - 1) {
        cyw43_arch_poll();
        int n = net_socket_recv(sock, response_buf + total, buf_size - 1 - total);
        if (n > 0) {
            total += (size_t)n;
            start = to_ms_since_boot(get_absolute_time()); /* reset timeout on data */
        } else if (n == NET_ERR_CLOSED) {
            break;  /* server closed connection = done */
        }
        sleep_ms(10);

        if (to_ms_since_boot(get_absolute_time()) - start > 10000) {
            break;  /* 10s idle timeout */
        }
    }

    response_buf[total] = '\0';
    net_socket_close(sock);

    /* Find body (after \r\n\r\n header separator) */
    char *body = strstr(response_buf, "\r\n\r\n");
    if (body) {
        body += 4;
        size_t body_len = total - (size_t)(body - response_buf);
        memmove(response_buf, body, body_len + 1);
        return (int)body_len;
    }

    return (int)total;
}

/* ============================================================================
 * Non-PICO_W stubs
 * ============================================================================ */

#else /* !PICO_W */

static void net_no_wifi(void) {
    printf("WiFi not available (requires Pico W)\r\n");
}

int net_init(void)                          { net_no_wifi(); return NET_ERR_NOT_SUPPORTED; }
int net_wifi_connect(const char *ssid, const char *password, uint32_t timeout_ms) {
    (void)ssid; (void)password; (void)timeout_ms;
    net_no_wifi(); return NET_ERR_NOT_SUPPORTED;
}
int net_wifi_disconnect(void)               { net_no_wifi(); return NET_ERR_NOT_SUPPORTED; }
int net_wifi_scan(net_scan_result_t *results, int max_results) {
    (void)results; (void)max_results;
    net_no_wifi(); return NET_ERR_NOT_SUPPORTED;
}
int net_get_info(net_info_t *info)          { (void)info; net_no_wifi(); return NET_ERR_NOT_SUPPORTED; }
net_status_t net_get_status(void)           { return NET_STATUS_DOWN; }
int net_set_hostname(const char *hostname)  { (void)hostname; net_no_wifi(); return NET_ERR_NOT_SUPPORTED; }

int net_socket_create(net_sock_type_t type)  { (void)type; net_no_wifi(); return NET_ERR_NOT_SUPPORTED; }
int net_socket_close(int sock_id)            { (void)sock_id; net_no_wifi(); return NET_ERR_NOT_SUPPORTED; }
int net_socket_connect(int sock_id, net_ip4_t ip, uint16_t port, uint32_t timeout_ms) {
    (void)sock_id; (void)ip; (void)port; (void)timeout_ms;
    net_no_wifi(); return NET_ERR_NOT_SUPPORTED;
}
int net_socket_listen(int sock_id, uint16_t port) {
    (void)sock_id; (void)port;
    net_no_wifi(); return NET_ERR_NOT_SUPPORTED;
}
int net_socket_accept(int sock_id, int *new_sock_id) {
    (void)sock_id; (void)new_sock_id;
    net_no_wifi(); return NET_ERR_NOT_SUPPORTED;
}
int net_socket_send(int sock_id, const void *data, size_t len) {
    (void)sock_id; (void)data; (void)len;
    net_no_wifi(); return NET_ERR_NOT_SUPPORTED;
}
int net_socket_recv(int sock_id, void *data, size_t max_len) {
    (void)sock_id; (void)data; (void)max_len;
    net_no_wifi(); return NET_ERR_NOT_SUPPORTED;
}
int net_socket_sendto(int sock_id, net_ip4_t ip, uint16_t port,
                      const void *data, size_t len) {
    (void)sock_id; (void)ip; (void)port; (void)data; (void)len;
    net_no_wifi(); return NET_ERR_NOT_SUPPORTED;
}
int net_dns_lookup(const char *hostname, net_ip4_t *ip) {
    (void)hostname; (void)ip;
    net_no_wifi(); return NET_ERR_NOT_SUPPORTED;
}
int net_ping(net_ip4_t ip, uint32_t timeout_ms) {
    (void)ip; (void)timeout_ms;
    net_no_wifi(); return NET_ERR_NOT_SUPPORTED;
}
int net_http_get(const char *url, char *response_buf, size_t buf_size) {
    (void)url; (void)response_buf; (void)buf_size;
    net_no_wifi(); return NET_ERR_NOT_SUPPORTED;
}

#endif /* PICO_W */
