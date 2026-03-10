/* cmd_wire.c - Interactive I2C/SPI REPL for littleOS */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#endif

/* Parse hex value like 0x3C or 3C */
static uint32_t parse_hex(const char *s) {
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    return (uint32_t)strtoul(s, NULL, 16);
}

int cmd_wire(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: wire <command> [args...]\r\n");
        printf("Commands:\r\n");
        printf("  i2c read  ADDR REG [LEN]  - Read I2C register(s)\r\n");
        printf("  i2c write ADDR REG VAL    - Write I2C register\r\n");
        printf("  i2c dump  ADDR            - Dump all registers 0x00-0xFF\r\n");
        printf("  spi xfer  BYTE [BYTE...]  - SPI transfer bytes\r\n");
        printf("  spi read  REG [LEN]       - SPI read register(s)\r\n");
        printf("  spi write REG VAL         - SPI write register\r\n");
        printf("\r\nAddresses/values in hex (0x prefix optional)\r\n");
        return 0;
    }

    if (strcmp(argv[1], "i2c") == 0) {
        if (argc < 3) {
            printf("Usage: wire i2c <read|write|dump> ...\r\n");
            return 1;
        }

#ifdef PICO_BUILD
        i2c_inst_t *i2c = i2c0;

        if (strcmp(argv[2], "read") == 0) {
            if (argc < 5) {
                printf("Usage: wire i2c read ADDR REG [LEN]\r\n");
                return 1;
            }
            uint8_t addr = (uint8_t)parse_hex(argv[3]);
            uint8_t reg = (uint8_t)parse_hex(argv[4]);
            int len = (argc >= 6) ? atoi(argv[5]) : 1;
            if (len < 1) len = 1;
            if (len > 32) len = 32;

            /* Write register address, then read */
            int ret = i2c_write_blocking(i2c, addr, &reg, 1, true);
            if (ret < 0) {
                printf("Error: I2C write failed (device 0x%02X not responding)\r\n", addr);
                return 1;
            }

            uint8_t buf[32];
            ret = i2c_read_blocking(i2c, addr, buf, (size_t)len, false);
            if (ret < 0) {
                printf("Error: I2C read failed\r\n");
                return 1;
            }

            printf("Device 0x%02X, Reg 0x%02X:", addr, reg);
            for (int i = 0; i < len; i++) {
                printf(" 0x%02X", buf[i]);
            }
            printf("\r\n");
            return 0;
        }

        if (strcmp(argv[2], "write") == 0) {
            if (argc < 6) {
                printf("Usage: wire i2c write ADDR REG VAL\r\n");
                return 1;
            }
            uint8_t addr = (uint8_t)parse_hex(argv[3]);
            uint8_t data[2];
            data[0] = (uint8_t)parse_hex(argv[4]); /* reg */
            data[1] = (uint8_t)parse_hex(argv[5]); /* val */

            int ret = i2c_write_blocking(i2c, addr, data, 2, false);
            if (ret < 0) {
                printf("Error: I2C write failed\r\n");
                return 1;
            }
            printf("Wrote 0x%02X to device 0x%02X reg 0x%02X\r\n", data[1], addr, data[0]);
            return 0;
        }

        if (strcmp(argv[2], "dump") == 0) {
            if (argc < 4) {
                printf("Usage: wire i2c dump ADDR\r\n");
                return 1;
            }
            uint8_t addr = (uint8_t)parse_hex(argv[3]);

            printf("Register dump for device 0x%02X:\r\n", addr);
            printf("     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\r\n");

            for (int reg = 0; reg < 256; reg++) {
                if (reg % 16 == 0) printf("%02X: ", reg);

                uint8_t r = (uint8_t)reg;
                uint8_t val;
                int ret = i2c_write_blocking(i2c, addr, &r, 1, true);
                if (ret >= 0) {
                    ret = i2c_read_blocking(i2c, addr, &val, 1, false);
                    if (ret >= 0) {
                        printf("%02X ", val);
                    } else {
                        printf("?? ");
                    }
                } else {
                    printf("XX ");
                }

                if (reg % 16 == 15) printf("\r\n");
            }
            return 0;
        }
#else
        printf("I2C commands require hardware (PICO_BUILD)\r\n");
        return 1;
#endif
    }

    if (strcmp(argv[1], "spi") == 0) {
        if (argc < 3) {
            printf("Usage: wire spi <xfer|read|write> ...\r\n");
            return 1;
        }

#ifdef PICO_BUILD
        spi_inst_t *spi = spi0;

        if (strcmp(argv[2], "xfer") == 0) {
            if (argc < 4) {
                printf("Usage: wire spi xfer BYTE [BYTE...]\r\n");
                return 1;
            }
            int nbytes = argc - 3;
            if (nbytes > 32) nbytes = 32;

            uint8_t tx[32], rx[32];
            for (int i = 0; i < nbytes; i++) {
                tx[i] = (uint8_t)parse_hex(argv[3 + i]);
            }

            spi_write_read_blocking(spi, tx, rx, (size_t)nbytes);

            printf("TX:");
            for (int i = 0; i < nbytes; i++) printf(" %02X", tx[i]);
            printf("\r\nRX:");
            for (int i = 0; i < nbytes; i++) printf(" %02X", rx[i]);
            printf("\r\n");
            return 0;
        }

        if (strcmp(argv[2], "read") == 0) {
            if (argc < 4) {
                printf("Usage: wire spi read REG [LEN]\r\n");
                return 1;
            }
            uint8_t reg = (uint8_t)parse_hex(argv[3]) | 0x80; /* read bit */
            int len = (argc >= 5) ? atoi(argv[4]) : 1;
            if (len < 1) len = 1;
            if (len > 32) len = 32;

            uint8_t tx[33] = {0};
            uint8_t rx[33] = {0};
            tx[0] = reg;
            spi_write_read_blocking(spi, tx, rx, (size_t)(len + 1));

            printf("Reg 0x%02X:", reg & 0x7F);
            for (int i = 1; i <= len; i++) printf(" 0x%02X", rx[i]);
            printf("\r\n");
            return 0;
        }

        if (strcmp(argv[2], "write") == 0) {
            if (argc < 5) {
                printf("Usage: wire spi write REG VAL\r\n");
                return 1;
            }
            uint8_t data[2];
            data[0] = (uint8_t)parse_hex(argv[3]) & 0x7F; /* write bit */
            data[1] = (uint8_t)parse_hex(argv[4]);

            spi_write_blocking(spi, data, 2);
            printf("Wrote 0x%02X to SPI reg 0x%02X\r\n", data[1], data[0]);
            return 0;
        }
#else
        printf("SPI commands require hardware (PICO_BUILD)\r\n");
        return 1;
#endif
    }

    printf("Unknown command: %s\r\n", argv[1]);
    return 1;
}
