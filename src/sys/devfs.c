/* devfs.c - littleOS /dev device filesystem implementation */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "devfs.h"

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/structs/rosc.h"
#endif

/* =========================
 * Internal state
 * ========================= */
static devfs_device_t devices[DEVFS_MAX_DEVICES];
static int device_count = 0;
static bool devfs_initialized = false;

/* =========================
 * Path helper
 * ========================= */
static const char *strip_dev_prefix(const char *path) {
    if (path == NULL) return NULL;
    /* Strip "/dev/" prefix if present */
    if (strncmp(path, "/dev/", 5) == 0) {
        return path + 5;
    }
    return path;
}

/* =========================
 * GPIO device operations
 * ========================= */

/* Context for GPIO devices: pin number stored as uintptr_t in ctx */

static int gpio_dev_read(devfs_device_t *dev, uint8_t *buf, size_t len) {
    if (buf == NULL || len < 2) return -1;
    uint8_t pin = (uint8_t)(uintptr_t)dev->ctx;
    int val = 0;

#ifdef PICO_BUILD
    val = gpio_get(pin) ? 1 : 0;
#endif

    int n = snprintf((char *)buf, len, "%d\n", val);
    return (n > 0 && (size_t)n < len) ? n : -1;
}

static int gpio_dev_write(devfs_device_t *dev, const uint8_t *buf, size_t len) {
    if (buf == NULL || len == 0) return -1;
    uint8_t pin = (uint8_t)(uintptr_t)dev->ctx;

    /* Accept: "1", "0", "in", "out" */
    if (len >= 3 && strncmp((const char *)buf, "out", 3) == 0) {
#ifdef PICO_BUILD
        gpio_set_dir(pin, GPIO_OUT);
#endif
        return (int)len;
    }
    if (len >= 2 && strncmp((const char *)buf, "in", 2) == 0) {
#ifdef PICO_BUILD
        gpio_set_dir(pin, GPIO_IN);
#endif
        return (int)len;
    }
    if (buf[0] == '1') {
#ifdef PICO_BUILD
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
        gpio_put(pin, 1);
#endif
        return (int)len;
    }
    if (buf[0] == '0') {
#ifdef PICO_BUILD
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
        gpio_put(pin, 0);
#endif
        return (int)len;
    }

    return -1; /* unrecognized command */
}

static int gpio_dev_ioctl(devfs_device_t *dev, uint32_t cmd, uint32_t arg) {
    (void)dev; (void)cmd; (void)arg;
    return -1; /* not implemented */
}

/* =========================
 * UART device operations
 * ========================= */

static int uart_dev_read(devfs_device_t *dev, uint8_t *buf, size_t len) {
    (void)dev;
    if (buf == NULL || len == 0) return -1;

#ifdef PICO_BUILD
    /* Non-blocking read from stdio */
    for (size_t i = 0; i < len; i++) {
        int c = getchar_timeout_us(0);
        if (c == PICO_ERROR_TIMEOUT) {
            return (int)i;
        }
        buf[i] = (uint8_t)c;
    }
    return (int)len;
#else
    return 0;
#endif
}

static int uart_dev_write(devfs_device_t *dev, const uint8_t *buf, size_t len) {
    (void)dev;
    if (buf == NULL || len == 0) return -1;

    for (size_t i = 0; i < len; i++) {
        putchar(buf[i]);
    }
    return (int)len;
}

static int uart_dev_ioctl(devfs_device_t *dev, uint32_t cmd, uint32_t arg) {
    (void)dev; (void)cmd; (void)arg;
    return -1;
}

/* =========================
 * ADC device operations
 * ========================= */

static int adc_dev_read(devfs_device_t *dev, uint8_t *buf, size_t len) {
    if (buf == NULL || len < 6) return -1;
    uint8_t channel = (uint8_t)(uintptr_t)dev->ctx;
    uint16_t raw = 0;

#ifdef PICO_BUILD
    adc_select_input(channel);
    raw = adc_read();
#endif

    int n;
    if (channel == 4) {
        /* Temperature sensor: convert raw to temperature */
        /* Using standard RP2040 conversion: T = 27 - (V - 0.706) / 0.001721 */
#ifdef PICO_BUILD
        float voltage = raw * 3.3f / (1 << 12);
        float temp_c = 27.0f - (voltage - 0.706f) / 0.001721f;
        n = snprintf((char *)buf, len, "%.1f\n", temp_c);
#else
        n = snprintf((char *)buf, len, "0.0\n");
#endif
    } else {
        n = snprintf((char *)buf, len, "%u\n", raw);
    }

    return (n > 0 && (size_t)n < len) ? n : -1;
}

static int adc_dev_write(devfs_device_t *dev, const uint8_t *buf, size_t len) {
    (void)dev; (void)buf; (void)len;
    return -1; /* ADC is read-only */
}

static int adc_dev_ioctl(devfs_device_t *dev, uint32_t cmd, uint32_t arg) {
    (void)dev; (void)cmd; (void)arg;
    return -1;
}

/* =========================
 * /dev/null operations
 * ========================= */

static int null_dev_read(devfs_device_t *dev, uint8_t *buf, size_t len) {
    (void)dev; (void)buf; (void)len;
    return 0; /* EOF immediately */
}

static int null_dev_write(devfs_device_t *dev, const uint8_t *buf, size_t len) {
    (void)dev; (void)buf;
    return (int)len; /* discard everything */
}

static int null_dev_ioctl(devfs_device_t *dev, uint32_t cmd, uint32_t arg) {
    (void)dev; (void)cmd; (void)arg;
    return 0;
}

/* =========================
 * /dev/zero operations
 * ========================= */

static int zero_dev_read(devfs_device_t *dev, uint8_t *buf, size_t len) {
    (void)dev;
    if (buf == NULL) return -1;
    memset(buf, 0, len);
    return (int)len;
}

static int zero_dev_write(devfs_device_t *dev, const uint8_t *buf, size_t len) {
    (void)dev; (void)buf;
    return (int)len; /* discard */
}

static int zero_dev_ioctl(devfs_device_t *dev, uint32_t cmd, uint32_t arg) {
    (void)dev; (void)cmd; (void)arg;
    return 0;
}

/* =========================
 * /dev/random operations
 * ========================= */

static int random_dev_read(devfs_device_t *dev, uint8_t *buf, size_t len) {
    (void)dev;
    if (buf == NULL) return -1;

    for (size_t i = 0; i < len; i++) {
        uint8_t byte = 0;
#ifdef PICO_BUILD
        /* Build random byte from ROSC random bit */
        for (int bit = 0; bit < 8; bit++) {
            byte = (byte << 1) | (rosc_hw->randombit & 1);
        }
#else
        byte = (uint8_t)(rand() & 0xFF);
#endif
        buf[i] = byte;
    }

    return (int)len;
}

static int random_dev_write(devfs_device_t *dev, const uint8_t *buf, size_t len) {
    (void)dev; (void)buf; (void)len;
    return -1; /* cannot write to random */
}

static int random_dev_ioctl(devfs_device_t *dev, uint32_t cmd, uint32_t arg) {
    (void)dev; (void)cmd; (void)arg;
    return -1;
}

/* =========================
 * Device type name helper
 * ========================= */

static const char *devfs_type_name(devfs_type_t type) {
    switch (type) {
    case DEV_TYPE_GPIO:   return "gpio";
    case DEV_TYPE_UART:   return "uart";
    case DEV_TYPE_ADC:    return "adc";
    case DEV_TYPE_PWM:    return "pwm";
    case DEV_TYPE_I2C:    return "i2c";
    case DEV_TYPE_SPI:    return "spi";
    case DEV_TYPE_PIO:    return "pio";
    case DEV_TYPE_NULL:   return "null";
    case DEV_TYPE_ZERO:   return "zero";
    case DEV_TYPE_RANDOM: return "random";
    default:              return "unknown";
    }
}

/* =========================
 * Public API
 * ========================= */

int devfs_register(const char *name, devfs_type_t type,
                   const devfs_ops_t *ops, void *ctx) {
    if (name == NULL || ops == NULL) return -1;
    if (device_count >= DEVFS_MAX_DEVICES) return -2;

    devfs_device_t *dev = &devices[device_count];
    strncpy(dev->name, name, DEVFS_MAX_NAME_LEN - 1);
    dev->name[DEVFS_MAX_NAME_LEN - 1] = '\0';
    dev->type = type;
    dev->ops = *ops;
    dev->ctx = ctx;
    dev->permissions = 0644; /* rw-r--r-- default */
    dev->active = true;

    device_count++;
    return 0;
}

devfs_device_t *devfs_lookup(const char *path) {
    const char *name = strip_dev_prefix(path);
    if (name == NULL) return NULL;

    for (int i = 0; i < device_count; i++) {
        if (devices[i].active && strcmp(devices[i].name, name) == 0) {
            return &devices[i];
        }
    }
    return NULL;
}

int devfs_read(const char *path, uint8_t *buf, size_t len) {
    devfs_device_t *dev = devfs_lookup(path);
    if (dev == NULL) return -1;
    if (dev->ops.read == NULL) return -1;
    return dev->ops.read(dev, buf, len);
}

int devfs_write(const char *path, const uint8_t *buf, size_t len) {
    devfs_device_t *dev = devfs_lookup(path);
    if (dev == NULL) return -1;
    if (dev->ops.write == NULL) return -1;
    return dev->ops.write(dev, buf, len);
}

int devfs_ioctl(const char *path, uint32_t cmd, uint32_t arg) {
    devfs_device_t *dev = devfs_lookup(path);
    if (dev == NULL) return -1;
    if (dev->ops.ioctl == NULL) return -1;
    return dev->ops.ioctl(dev, cmd, arg);
}

int devfs_list(char *buf, size_t buflen) {
    if (buf == NULL || buflen == 0) return -1;

    int total = 0;
    for (int i = 0; i < device_count; i++) {
        if (!devices[i].active) continue;

        int n = snprintf(buf + total, buflen - (size_t)total,
                         "/dev/%-12s  type=%-6s  perms=%04o\r\n",
                         devices[i].name,
                         devfs_type_name(devices[i].type),
                         devices[i].permissions);
        if (n < 0 || (size_t)(total + n) >= buflen) break;
        total += n;
    }

    return total;
}

void devfs_init(void) {
    if (devfs_initialized) return;

    memset(devices, 0, sizeof(devices));
    device_count = 0;

    /* --- GPIO devices: gpio0 through gpio29 --- */
    static const devfs_ops_t gpio_ops = {
        .read  = gpio_dev_read,
        .write = gpio_dev_write,
        .ioctl = gpio_dev_ioctl,
    };

    for (int pin = 0; pin < 30; pin++) {
        char name[DEVFS_MAX_NAME_LEN];
        snprintf(name, sizeof(name), "gpio%d", pin);
        devfs_register(name, DEV_TYPE_GPIO, &gpio_ops, (void *)(uintptr_t)pin);
    }

    /* --- LED alias (gpio25) --- */
    devfs_register("led", DEV_TYPE_GPIO, &gpio_ops, (void *)(uintptr_t)25);

    /* --- UART devices --- */
    static const devfs_ops_t uart_ops = {
        .read  = uart_dev_read,
        .write = uart_dev_write,
        .ioctl = uart_dev_ioctl,
    };

    devfs_register("uart0", DEV_TYPE_UART, &uart_ops, (void *)(uintptr_t)0);
    devfs_register("uart1", DEV_TYPE_UART, &uart_ops, (void *)(uintptr_t)1);

    /* --- ADC devices: adc0 through adc3 + adc4 (temperature) --- */
    static const devfs_ops_t adc_ops = {
        .read  = adc_dev_read,
        .write = adc_dev_write,
        .ioctl = adc_dev_ioctl,
    };

    for (int ch = 0; ch <= 4; ch++) {
        char name[DEVFS_MAX_NAME_LEN];
        snprintf(name, sizeof(name), "adc%d", ch);
        devfs_register(name, DEV_TYPE_ADC, &adc_ops, (void *)(uintptr_t)ch);
    }

    /* --- /dev/null --- */
    static const devfs_ops_t null_ops = {
        .read  = null_dev_read,
        .write = null_dev_write,
        .ioctl = null_dev_ioctl,
    };
    devfs_register("null", DEV_TYPE_NULL, &null_ops, NULL);

    /* --- /dev/zero --- */
    static const devfs_ops_t zero_ops = {
        .read  = zero_dev_read,
        .write = zero_dev_write,
        .ioctl = zero_dev_ioctl,
    };
    devfs_register("zero", DEV_TYPE_ZERO, &zero_ops, NULL);

    /* --- /dev/random --- */
    static const devfs_ops_t random_ops = {
        .read  = random_dev_read,
        .write = random_dev_write,
        .ioctl = random_dev_ioctl,
    };
    devfs_register("random", DEV_TYPE_RANDOM, &random_ops, NULL);

    devfs_initialized = true;
}
