/* cmd_ota.c - OTA update shell commands */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ota.h"

static void ota_progress_print(uint32_t received, uint32_t total) {
    if (total > 0) {
        uint32_t pct = (received * 100) / total;
        printf("\rOTA: %lu / %lu bytes (%lu%%)", (unsigned long)received,
               (unsigned long)total, (unsigned long)pct);
    } else {
        printf("\rOTA: %lu bytes received", (unsigned long)received);
    }
}

static void cmd_ota_usage(void) {
    printf("OTA update commands:\r\n");
    printf("  ota status        - Show OTA status and slot info\r\n");
    printf("  ota begin uart    - Begin OTA via UART (XMODEM)\r\n");
    printf("  ota begin tcp <p> - Begin OTA via TCP on port\r\n");
    printf("  ota verify        - Verify received firmware\r\n");
    printf("  ota apply         - Apply update and reboot\r\n");
    printf("  ota confirm       - Confirm current boot is good\r\n");
    printf("  ota rollback      - Rollback to previous firmware\r\n");
    printf("  ota cancel        - Cancel in-progress update\r\n");
}

int cmd_ota(int argc, char *argv[]) {
    if (argc < 2) {
        cmd_ota_usage();
        return -1;
    }

    if (strcmp(argv[1], "status") == 0) {
        ota_print_status();
        return 0;
    }

    if (strcmp(argv[1], "begin") == 0) {
        if (argc < 3) {
            printf("Usage: ota begin <uart|tcp> [port]\r\n");
            return -1;
        }
        if (strcmp(argv[2], "uart") == 0) {
            int r = ota_begin_uart(ota_progress_print);
            if (r == OTA_OK) {
                printf("OTA: Waiting for XMODEM data on UART...\r\n");
            } else {
                printf("OTA begin failed: %d\r\n", r);
            }
            return r;
        }
        if (strcmp(argv[2], "tcp") == 0) {
            uint16_t port = (argc >= 4) ? (uint16_t)atoi(argv[3]) : 4242;
            int r = ota_begin_tcp(port, ota_progress_print);
            if (r == OTA_OK) {
                printf("OTA: Listening on TCP port %d...\r\n", port);
            } else {
                printf("OTA begin failed: %d\r\n", r);
            }
            return r;
        }
        printf("Unknown transport: %s\r\n", argv[2]);
        return -1;
    }

    if (strcmp(argv[1], "verify") == 0) {
        printf("Verifying firmware image...\r\n");
        int r = ota_verify();
        printf("OTA verify: %s\r\n", r == OTA_OK ? "OK" : "FAILED");
        return r;
    }

    if (strcmp(argv[1], "apply") == 0) {
        printf("Applying OTA update (system will reboot)...\r\n");
        int r = ota_apply();
        /* If we get here, apply failed */
        printf("OTA apply failed: %d\r\n", r);
        return r;
    }

    if (strcmp(argv[1], "confirm") == 0) {
        int r = ota_confirm_boot();
        printf("OTA boot confirm: %s\r\n", r == OTA_OK ? "OK" : "failed");
        return r;
    }

    if (strcmp(argv[1], "rollback") == 0) {
        printf("Rolling back firmware (system will reboot)...\r\n");
        int r = ota_rollback();
        printf("OTA rollback failed: %d\r\n", r);
        return r;
    }

    if (strcmp(argv[1], "cancel") == 0) {
        int r = ota_cancel();
        printf("OTA %s\r\n", r == OTA_OK ? "cancelled" : "cancel failed");
        return r;
    }

    cmd_ota_usage();
    return -1;
}
