/* cmd_hw.c - Hardware peripheral shell commands (I2C/SPI/PWM/ADC) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hal/i2c.h"
#include "hal/spi.h"
#include "hal/pwm.h"
#include "hal/adc.h"

static void cmd_hw_usage(void) {
    printf("Hardware peripheral commands:\r\n");
    printf("  hw i2c init <inst> <sda> <scl> [speed] - Init I2C (speed: 100/400/1000 kHz)\r\n");
    printf("  hw i2c scan <inst>                     - Scan I2C bus for devices\r\n");
    printf("  hw i2c read <inst> <addr> <len>        - Read from I2C device\r\n");
    printf("  hw i2c write <inst> <addr> <byte...>   - Write to I2C device\r\n");
    printf("  hw spi init <inst> <mosi> <miso> <sck> <cs> [baud] - Init SPI\r\n");
    printf("  hw spi transfer <inst> <byte...>       - SPI transfer\r\n");
    printf("  hw pwm init <pin> <freq> <duty>        - Init PWM (duty 0-100%%)\r\n");
    printf("  hw pwm set <pin> <duty>                - Set PWM duty cycle\r\n");
    printf("  hw pwm stop <pin>                      - Stop PWM\r\n");
    printf("  hw adc init                            - Init ADC subsystem\r\n");
    printf("  hw adc read <ch>                       - Read ADC channel (0-4)\r\n");
    printf("  hw adc temp                            - Read die temperature\r\n");
    printf("  hw adc all                             - Read all ADC channels\r\n");
}

static int cmd_hw_i2c(int argc, char *argv[]) {
    if (argc < 3) {
        cmd_hw_usage();
        return -1;
    }

    if (strcmp(argv[2], "init") == 0) {
        if (argc < 6) {
            printf("Usage: hw i2c init <instance> <sda_pin> <scl_pin> [speed_khz]\r\n");
            return -1;
        }
        uint8_t inst = (uint8_t)atoi(argv[3]);
        uint8_t sda = (uint8_t)atoi(argv[4]);
        uint8_t scl = (uint8_t)atoi(argv[5]);
        uint32_t speed = (argc >= 7) ? (uint32_t)atoi(argv[6]) * 1000 : 100000;
        int r = i2c_hal_init(inst, sda, scl, (i2c_speed_t)speed);
        if (r == 0) {
            printf("I2C%d initialized: SDA=%d SCL=%d speed=%lu Hz\r\n", inst, sda, scl, (unsigned long)speed);
        } else {
            printf("I2C%d init failed: %d\r\n", inst, r);
        }
        return r;
    }

    if (strcmp(argv[2], "scan") == 0) {
        if (argc < 4) {
            printf("Usage: hw i2c scan <instance>\r\n");
            return -1;
        }
        uint8_t inst = (uint8_t)atoi(argv[3]);
        uint8_t addrs[16];
        int count = i2c_hal_scan(inst, addrs, 16);
        if (count < 0) {
            printf("I2C scan failed: %d\r\n", count);
            return count;
        }
        printf("I2C%d scan: %d device(s) found\r\n", inst, count);
        for (int i = 0; i < count; i++) {
            printf("  0x%02X\r\n", addrs[i]);
        }
        return 0;
    }

    if (strcmp(argv[2], "read") == 0) {
        if (argc < 6) {
            printf("Usage: hw i2c read <inst> <addr> <len>\r\n");
            return -1;
        }
        uint8_t inst = (uint8_t)atoi(argv[3]);
        uint8_t addr = (uint8_t)strtoul(argv[4], NULL, 0);
        int len = atoi(argv[5]);
        if (len > 64) len = 64;
        uint8_t buf[64];
        int r = i2c_hal_read(inst, addr, buf, (size_t)len);
        if (r < 0) {
            printf("I2C read failed: %d\r\n", r);
            return r;
        }
        printf("I2C%d read 0x%02X (%d bytes):", inst, addr, len);
        for (int i = 0; i < len; i++) {
            printf(" %02X", buf[i]);
        }
        printf("\r\n");
        return 0;
    }

    if (strcmp(argv[2], "write") == 0) {
        if (argc < 5) {
            printf("Usage: hw i2c write <inst> <addr> <byte...>\r\n");
            return -1;
        }
        uint8_t inst = (uint8_t)atoi(argv[3]);
        uint8_t addr = (uint8_t)strtoul(argv[4], NULL, 0);
        int data_count = argc - 5;
        if (data_count > 32) data_count = 32;
        uint8_t data[32];
        for (int i = 0; i < data_count; i++) {
            data[i] = (uint8_t)strtoul(argv[5 + i], NULL, 0);
        }
        int r = i2c_hal_write(inst, addr, data, (size_t)data_count);
        if (r < 0) {
            printf("I2C write failed: %d\r\n", r);
            return r;
        }
        printf("I2C%d wrote %d bytes to 0x%02X\r\n", inst, data_count, addr);
        return 0;
    }

    cmd_hw_usage();
    return -1;
}

static int cmd_hw_spi(int argc, char *argv[]) {
    if (argc < 3) {
        cmd_hw_usage();
        return -1;
    }

    if (strcmp(argv[2], "init") == 0) {
        if (argc < 8) {
            printf("Usage: hw spi init <inst> <mosi> <miso> <sck> <cs> [baudrate]\r\n");
            return -1;
        }
        uint8_t inst = (uint8_t)atoi(argv[3]);
        uint8_t mosi = (uint8_t)atoi(argv[4]);
        uint8_t miso = (uint8_t)atoi(argv[5]);
        uint8_t sck = (uint8_t)atoi(argv[6]);
        uint8_t cs = (uint8_t)atoi(argv[7]);
        uint32_t baud = (argc >= 9) ? (uint32_t)strtoul(argv[8], NULL, 0) : 1000000;
        int r = spi_hal_init(inst, mosi, miso, sck, cs, baud, SPI_MODE_0);
        if (r == 0) {
            printf("SPI%d initialized: MOSI=%d MISO=%d SCK=%d CS=%d baud=%lu\r\n",
                   inst, mosi, miso, sck, cs, (unsigned long)baud);
        } else {
            printf("SPI%d init failed: %d\r\n", inst, r);
        }
        return r;
    }

    if (strcmp(argv[2], "transfer") == 0) {
        if (argc < 4) {
            printf("Usage: hw spi transfer <inst> <byte...>\r\n");
            return -1;
        }
        uint8_t inst = (uint8_t)atoi(argv[3]);
        int count = argc - 4;
        if (count > 32) count = 32;
        uint8_t tx[32], rx[32];
        for (int i = 0; i < count; i++) {
            tx[i] = (uint8_t)strtoul(argv[4 + i], NULL, 0);
        }
        int r = spi_hal_transfer(inst, tx, rx, (size_t)count);
        if (r < 0) {
            printf("SPI transfer failed: %d\r\n", r);
            return r;
        }
        printf("SPI%d TX:", inst);
        for (int i = 0; i < count; i++) printf(" %02X", tx[i]);
        printf("\r\nSPI%d RX:", inst);
        for (int i = 0; i < count; i++) printf(" %02X", rx[i]);
        printf("\r\n");
        return 0;
    }

    cmd_hw_usage();
    return -1;
}

static int cmd_hw_pwm(int argc, char *argv[]) {
    if (argc < 3) {
        cmd_hw_usage();
        return -1;
    }

    if (strcmp(argv[2], "init") == 0) {
        if (argc < 6) {
            printf("Usage: hw pwm init <pin> <freq_hz> <duty_percent>\r\n");
            return -1;
        }
        uint8_t pin = (uint8_t)atoi(argv[3]);
        uint32_t freq = (uint32_t)strtoul(argv[4], NULL, 0);
        uint16_t duty = (uint16_t)(atoi(argv[5]) * 100); /* percent to x100 */
        int r = pwm_hal_init(pin, freq, duty);
        if (r == 0) {
            printf("PWM on GPIO%d: %lu Hz, %d%% duty\r\n", pin, (unsigned long)freq, duty / 100);
        } else {
            printf("PWM init failed: %d\r\n", r);
        }
        return r;
    }

    if (strcmp(argv[2], "set") == 0) {
        if (argc < 5) {
            printf("Usage: hw pwm set <pin> <duty_percent>\r\n");
            return -1;
        }
        uint8_t pin = (uint8_t)atoi(argv[3]);
        uint16_t duty = (uint16_t)(atoi(argv[4]) * 100);
        int r = pwm_hal_set_duty(pin, duty);
        if (r == 0) {
            printf("PWM GPIO%d duty set to %d%%\r\n", pin, duty / 100);
        } else {
            printf("PWM set duty failed: %d\r\n", r);
        }
        return r;
    }

    if (strcmp(argv[2], "stop") == 0) {
        if (argc < 4) {
            printf("Usage: hw pwm stop <pin>\r\n");
            return -1;
        }
        uint8_t pin = (uint8_t)atoi(argv[3]);
        int r = pwm_hal_enable(pin, false);
        printf("PWM GPIO%d %s\r\n", pin, r == 0 ? "stopped" : "stop failed");
        return r;
    }

    cmd_hw_usage();
    return -1;
}

static int cmd_hw_adc(int argc, char *argv[]) {
    if (argc < 3) {
        cmd_hw_usage();
        return -1;
    }

    if (strcmp(argv[2], "init") == 0) {
        int r = adc_hal_init();
        printf("ADC %s\r\n", r == 0 ? "initialized" : "init failed");
        return r;
    }

    if (strcmp(argv[2], "read") == 0) {
        if (argc < 4) {
            printf("Usage: hw adc read <channel 0-4>\r\n");
            return -1;
        }
        uint8_t ch = (uint8_t)atoi(argv[3]);
        if (ch >= ADC_NUM_CHANNELS) {
            printf("Invalid channel (0-4)\r\n");
            return -1;
        }
        adc_hal_channel_init(ch);
        uint16_t raw = adc_hal_read_raw(ch);
        float voltage = adc_hal_read_voltage(ch);
        printf("ADC ch%d: raw=%u voltage=%.3fV\r\n", ch, raw, voltage);
        return 0;
    }

    if (strcmp(argv[2], "temp") == 0) {
        adc_hal_channel_init(ADC_TEMP_CHANNEL);
        float temp = adc_hal_read_temp_c();
        printf("Die temperature: %.1f C\r\n", temp);
        return 0;
    }

    if (strcmp(argv[2], "all") == 0) {
        printf("ADC Readings:\r\n");
        for (uint8_t ch = 0; ch < ADC_NUM_CHANNELS; ch++) {
            adc_hal_channel_init(ch);
            uint16_t raw = adc_hal_read_raw(ch);
            float voltage = adc_hal_read_voltage(ch);
            if (ch < 4) {
                printf("  CH%d (GPIO%d): raw=%4u  %.3fV\r\n",
                       ch, 26 + ch, raw, voltage);
            } else {
                float temp = adc_hal_read_temp_c();
                printf("  CH%d (temp):   raw=%4u  %.1f C\r\n", ch, raw, temp);
            }
        }
        return 0;
    }

    cmd_hw_usage();
    return -1;
}

int cmd_hw(int argc, char *argv[]) {
    if (argc < 2) {
        cmd_hw_usage();
        return -1;
    }

    if (strcmp(argv[1], "i2c") == 0) return cmd_hw_i2c(argc, argv);
    if (strcmp(argv[1], "spi") == 0) return cmd_hw_spi(argc, argv);
    if (strcmp(argv[1], "pwm") == 0) return cmd_hw_pwm(argc, argv);
    if (strcmp(argv[1], "adc") == 0) return cmd_hw_adc(argc, argv);

    cmd_hw_usage();
    return -1;
}
