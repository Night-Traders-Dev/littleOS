/* lwipopts.h - lwIP configuration for littleOS (RP2040 Pico W) */
#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

/* Platform: bare metal, no RTOS */
#define NO_SYS                      1
#define LWIP_SOCKET                 0
#define LWIP_NETCONN                0

/* Memory - conservative for RP2040 (256KB SRAM total) */
#define MEM_SIZE                    4096
#define MEMP_NUM_TCP_PCB            4
#define MEMP_NUM_TCP_SEG            8
#define MEMP_NUM_UDP_PCB            4
#define MEMP_NUM_PBUF               8
#define PBUF_POOL_SIZE              8
#define PBUF_POOL_BUFSIZE           1500

/* Protocols */
#define LWIP_TCP                    1
#define LWIP_UDP                    1
#define LWIP_DHCP                   1
#define LWIP_DNS                    1
#define LWIP_ICMP                   1
#define LWIP_RAW                    1
#define LWIP_ARP                    1
#define LWIP_AUTOIP                 0

/* DNS */
#define LWIP_DNS_TABLE_SIZE         4
#define DNS_MAX_SERVERS             2

/* TCP tuning */
#define TCP_MSS                     1460
#define TCP_WND                     (2 * TCP_MSS)
#define TCP_SND_BUF                 (2 * TCP_MSS)
#define TCP_SND_QUEUELEN            8

/* DHCP */
#define DHCP_DOES_ARP_CHECK         0

/* Thread safety for pico_cyw43_arch_lwip_threadsafe_background */
#define LWIP_TCPIP_CORE_LOCKING     0

/* Checksums */
#define CHECKSUM_GEN_IP             1
#define CHECKSUM_GEN_UDP            1
#define CHECKSUM_GEN_TCP            1
#define CHECKSUM_CHECK_IP           1
#define CHECKSUM_CHECK_UDP          1
#define CHECKSUM_CHECK_TCP          1

/* Disable stats to save RAM */
#define LWIP_STATS                  0
#define LWIP_STATS_DISPLAY          0

/* Hostname support */
#define LWIP_NETIF_HOSTNAME         1

#endif /* _LWIPOPTS_H */
