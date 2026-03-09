/* cmd_net.c - Networking shell commands */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "net.h"

static void cmd_net_usage(void) {
    printf("Networking commands:\r\n");
    printf("  net status            - Show network status\r\n");
    printf("  net connect <ssid> <pass> - Connect to WiFi\r\n");
    printf("  net disconnect        - Disconnect from WiFi\r\n");
    printf("  net scan              - Scan for WiFi networks\r\n");
    printf("  net ping <ip>         - Ping an IP address\r\n");
    printf("  net dns <hostname>    - DNS lookup\r\n");
    printf("  net http <url>        - HTTP GET request\r\n");
}

int cmd_net(int argc, char *argv[]) {
    if (argc < 2) {
        cmd_net_usage();
        return -1;
    }

    if (strcmp(argv[1], "status") == 0) {
        net_info_t info;
        int r = net_get_info(&info);
        if (r != NET_OK) {
            printf("Network not available\r\n");
            return r;
        }
        const char *status_names[] = {"DOWN", "CONNECTING", "CONNECTED", "GOT_IP", "ERROR"};
        const char *sname = (info.status < 5) ? status_names[info.status] : "UNKNOWN";
        printf("Network Status: %s\r\n", sname);
        if (info.status >= NET_STATUS_CONNECTED) {
            char ip_str[16], gw_str[16], nm_str[16];
            net_ip4_to_str(info.ip, ip_str, sizeof(ip_str));
            net_ip4_to_str(info.gateway, gw_str, sizeof(gw_str));
            net_ip4_to_str(info.netmask, nm_str, sizeof(nm_str));
            printf("  SSID:     %s\r\n", info.ssid);
            printf("  IP:       %s\r\n", ip_str);
            printf("  Gateway:  %s\r\n", gw_str);
            printf("  Netmask:  %s\r\n", nm_str);
            printf("  RSSI:     %d dBm\r\n", info.rssi);
            printf("  Hostname: %s\r\n", info.hostname);
            printf("  MAC:      %02X:%02X:%02X:%02X:%02X:%02X\r\n",
                   info.mac[0], info.mac[1], info.mac[2],
                   info.mac[3], info.mac[4], info.mac[5]);
            printf("  TX: %lu bytes  RX: %lu bytes\r\n",
                   (unsigned long)info.tx_bytes, (unsigned long)info.rx_bytes);
        }
        return 0;
    }

    if (strcmp(argv[1], "connect") == 0) {
        if (argc < 4) {
            printf("Usage: net connect <ssid> <password>\r\n");
            return -1;
        }
        printf("Connecting to '%s'...\r\n", argv[2]);
        int r = net_wifi_connect(argv[2], argv[3], 15000);
        if (r == NET_OK) {
            printf("Connected!\r\n");
            net_info_t info;
            if (net_get_info(&info) == NET_OK) {
                char ip_str[16];
                net_ip4_to_str(info.ip, ip_str, sizeof(ip_str));
                printf("IP: %s\r\n", ip_str);
            }
        } else {
            printf("Connection failed: %d\r\n", r);
        }
        return r;
    }

    if (strcmp(argv[1], "disconnect") == 0) {
        int r = net_wifi_disconnect();
        printf("WiFi %s\r\n", r == NET_OK ? "disconnected" : "disconnect failed");
        return r;
    }

    if (strcmp(argv[1], "scan") == 0) {
        net_scan_result_t results[NET_MAX_SCAN_RESULTS];
        int count = net_wifi_scan(results, NET_MAX_SCAN_RESULTS);
        if (count < 0) {
            printf("WiFi scan failed: %d\r\n", count);
            return count;
        }
        printf("Found %d network(s):\r\n", count);
        const char *auth_names[] = {"OPEN", "WPA", "WPA2", "WPA/WPA2"};
        for (int i = 0; i < count; i++) {
            const char *auth = (results[i].auth < 4) ? auth_names[results[i].auth] : "?";
            printf("  %-32s  ch%-2d  %4d dBm  %s\r\n",
                   results[i].ssid, results[i].channel,
                   results[i].rssi, auth);
        }
        return 0;
    }

    if (strcmp(argv[1], "ping") == 0) {
        if (argc < 3) {
            printf("Usage: net ping <ip>\r\n");
            return -1;
        }
        net_ip4_t ip;
        if (net_str_to_ip4(argv[2], &ip) != NET_OK) {
            printf("Invalid IP address\r\n");
            return -1;
        }
        printf("Pinging %s...\r\n", argv[2]);
        int r = net_ping(ip, 3000);
        printf("Ping %s\r\n", r == NET_OK ? "OK" : "failed");
        return r;
    }

    if (strcmp(argv[1], "dns") == 0) {
        if (argc < 3) {
            printf("Usage: net dns <hostname>\r\n");
            return -1;
        }
        net_ip4_t ip;
        int r = net_dns_lookup(argv[2], &ip);
        if (r == NET_OK) {
            char ip_str[16];
            net_ip4_to_str(ip, ip_str, sizeof(ip_str));
            printf("%s -> %s\r\n", argv[2], ip_str);
        } else {
            printf("DNS lookup failed: %d\r\n", r);
        }
        return r;
    }

    if (strcmp(argv[1], "http") == 0) {
        if (argc < 3) {
            printf("Usage: net http <url>\r\n");
            return -1;
        }
        char response[512];
        int r = net_http_get(argv[2], response, sizeof(response));
        if (r >= 0) {
            printf("%s\r\n", response);
        } else {
            printf("HTTP GET failed: %d\r\n", r);
        }
        return r;
    }

    cmd_net_usage();
    return -1;
}
