/* cmd_dev.c - /dev device file shell commands */
#include <stdio.h>
#include <string.h>
#include "devfs.h"

int cmd_dev(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: dev <command> [args]\r\n");
        printf("Commands:\r\n");
        printf("  list              - List all devices\r\n");
        printf("  read PATH         - Read from device\r\n");
        printf("  write PATH VALUE  - Write to device\r\n");
        printf("  info PATH         - Show device info\r\n");
        return 0;
    }

    if (strcmp(argv[1], "list") == 0) {
        char buf[2048];
        int n = devfs_list(buf, sizeof(buf));
        if (n > 0) {
            printf("%s", buf);
        } else {
            printf("No devices registered\r\n");
        }
        return 0;
    }

    if (strcmp(argv[1], "read") == 0) {
        if (argc < 3) {
            printf("Usage: dev read <path>\r\n");
            return 1;
        }
        char buf[256];
        int n = devfs_read(argv[2], (uint8_t *)buf, sizeof(buf) - 1);
        if (n < 0) {
            printf("Error reading %s: %d\r\n", argv[2], n);
            return 1;
        }
        buf[n] = '\0';
        printf("%s", buf);
        if (n > 0 && buf[n - 1] != '\n') printf("\r\n");
        return 0;
    }

    if (strcmp(argv[1], "write") == 0) {
        if (argc < 4) {
            printf("Usage: dev write <path> <value>\r\n");
            return 1;
        }
        int n = devfs_write(argv[2], (const uint8_t *)argv[3], strlen(argv[3]));
        if (n < 0) {
            printf("Error writing to %s: %d\r\n", argv[2], n);
            return 1;
        }
        printf("Wrote %d bytes to %s\r\n", n, argv[2]);
        return 0;
    }

    if (strcmp(argv[1], "info") == 0) {
        if (argc < 3) {
            printf("Usage: dev info <path>\r\n");
            return 1;
        }
        /* Strip /dev/ prefix */
        const char *name = argv[2];
        if (strncmp(name, "/dev/", 5) == 0) name += 5;

        devfs_device_t *dev = devfs_lookup(name);
        if (!dev) {
            printf("Device not found: %s\r\n", argv[2]);
            return 1;
        }
        printf("Device: /dev/%s\r\n", dev->name);
        printf("  Type: ");
        switch (dev->type) {
            case DEV_TYPE_GPIO:   printf("GPIO\r\n"); break;
            case DEV_TYPE_UART:   printf("UART\r\n"); break;
            case DEV_TYPE_ADC:    printf("ADC\r\n"); break;
            case DEV_TYPE_PWM:    printf("PWM\r\n"); break;
            case DEV_TYPE_I2C:    printf("I2C\r\n"); break;
            case DEV_TYPE_SPI:    printf("SPI\r\n"); break;
            case DEV_TYPE_PIO:    printf("PIO\r\n"); break;
            case DEV_TYPE_NULL:   printf("null\r\n"); break;
            case DEV_TYPE_ZERO:   printf("zero\r\n"); break;
            case DEV_TYPE_RANDOM: printf("random\r\n"); break;
            default:              printf("unknown\r\n"); break;
        }
        return 0;
    }

    printf("Unknown subcommand: %s\r\n", argv[1]);
    return 1;
}
