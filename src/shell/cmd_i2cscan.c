/* cmd_i2cscan.c - I2C bus scanner for littleOS */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#endif

int cmd_i2cscan(int argc, char *argv[]) {
    /* Usage: i2cscan [bus]  where bus is 0 or 1, default 0 */
    int bus = 0;
    if (argc >= 2) bus = atoi(argv[1]);

#ifdef PICO_BUILD
    i2c_inst_t *i2c = (bus == 1) ? i2c1 : i2c0;

    printf("Scanning I2C%d bus...\r\n\r\n", bus);
    printf("     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\r\n");

    int found = 0;
    for (int addr = 0; addr < 128; addr++) {
        if (addr % 16 == 0) {
            printf("%02X: ", addr);
        }

        if (addr < 0x08 || addr > 0x77) {
            printf("   ");
        } else {
            uint8_t rxdata;
            int ret = i2c_read_blocking(i2c, (uint8_t)addr, &rxdata, 1, false);
            if (ret >= 0) {
                printf("%02X ", addr);
                found++;
            } else {
                printf("-- ");
            }
        }

        if (addr % 16 == 15) printf("\r\n");
    }

    printf("\r\n%d device(s) found on I2C%d\r\n", found, bus);

    /* Known device identification */
    if (found > 0) {
        printf("\r\nKnown devices:\r\n");
        /* Re-scan for identification */
        for (int addr = 0x08; addr <= 0x77; addr++) {
            uint8_t rxdata;
            if (i2c_read_blocking(i2c, (uint8_t)addr, &rxdata, 1, false) >= 0) {
                printf("  0x%02X: ", addr);
                /* Common I2C device addresses */
                switch (addr) {
                    case 0x3C: case 0x3D: printf("SSD1306/SH1106 OLED Display"); break;
                    case 0x27: case 0x20: printf("PCF8574 I/O Expander / LCD"); break;
                    case 0x48: printf("ADS1115 ADC / TMP102 Temp Sensor"); break;
                    case 0x50: case 0x51: case 0x52: case 0x53:
                    case 0x54: case 0x55: case 0x56: case 0x57:
                        printf("AT24C EEPROM"); break;
                    case 0x68: printf("DS3231 RTC / MPU6050 IMU"); break;
                    case 0x69: printf("MPU6050 IMU (alt addr)"); break;
                    case 0x76: case 0x77: printf("BME280/BMP280 Sensor"); break;
                    case 0x29: printf("VL53L0X Distance Sensor"); break;
                    case 0x1E: printf("HMC5883L Magnetometer"); break;
                    case 0x23: printf("BH1750 Light Sensor"); break;
                    case 0x40: printf("INA219 Current Sensor / HDC1080"); break;
                    case 0x44: printf("SHT31 Temp/Humidity"); break;
                    case 0x5A: printf("MLX90614 IR Temp"); break;
                    case 0x60: printf("MCP4725 DAC / ATECC608"); break;
                    case 0x62: printf("SCD40 CO2 Sensor"); break;
                    default: printf("Unknown device"); break;
                }
                printf("\r\n");
            }
        }
    }
#else
    printf("I2C scan requires hardware (PICO_BUILD)\r\n");
    printf("Simulated scan on I2C%d:\r\n", bus);
    printf("     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\r\n");
    for (int addr = 0; addr < 128; addr++) {
        if (addr % 16 == 0) printf("%02X: ", addr);
        printf("-- ");
        if (addr % 16 == 15) printf("\r\n");
    }
    printf("\r\n0 device(s) found (emulated)\r\n");
#endif

    return 0;
}
