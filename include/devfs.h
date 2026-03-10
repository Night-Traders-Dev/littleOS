/* devfs.h - littleOS /dev device filesystem */
#ifndef LITTLEOS_DEVFS_H
#define LITTLEOS_DEVFS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================
 * Constants
 * ========================= */
#define DEVFS_MAX_DEVICES       32
#define DEVFS_MAX_NAME_LEN      16

/* =========================
 * Device Types
 * ========================= */
typedef enum {
    DEV_TYPE_GPIO   = 0,
    DEV_TYPE_UART   = 1,
    DEV_TYPE_ADC    = 2,
    DEV_TYPE_PWM    = 3,
    DEV_TYPE_I2C    = 4,
    DEV_TYPE_SPI    = 5,
    DEV_TYPE_PIO    = 6,
    DEV_TYPE_NULL   = 7,
    DEV_TYPE_ZERO   = 8,
    DEV_TYPE_RANDOM = 9,
} devfs_type_t;

/* =========================
 * Device Operations
 * ========================= */

/* Forward declaration */
struct devfs_device;

typedef struct {
    int (*read)(struct devfs_device *dev, uint8_t *buf, size_t len);
    int (*write)(struct devfs_device *dev, const uint8_t *buf, size_t len);
    int (*ioctl)(struct devfs_device *dev, uint32_t cmd, uint32_t arg);
} devfs_ops_t;

/* =========================
 * Device Entry
 * ========================= */

typedef struct devfs_device {
    char         name[DEVFS_MAX_NAME_LEN];  /* device name without /dev/ prefix */
    devfs_type_t type;
    devfs_ops_t  ops;
    void        *ctx;                        /* driver-specific context */
    uint16_t     permissions;                /* Unix-style permission bits */
    bool         active;
} devfs_device_t;

/* =========================
 * Public API
 * ========================= */

/**
 * Initialize devfs and register all built-in devices.
 */
void devfs_init(void);

/**
 * Register a new device.
 *
 * @param name  Device name (without /dev/ prefix, e.g. "gpio25")
 * @param type  Device type
 * @param ops   Device operations (read/write/ioctl)
 * @param ctx   Driver-specific context pointer
 * @return 0 on success, negative on error
 */
int devfs_register(const char *name, devfs_type_t type,
                   const devfs_ops_t *ops, void *ctx);

/**
 * Read from a device by path.
 *
 * @param path  Full path (e.g. "/dev/gpio25") or just name ("gpio25")
 * @param buf   Buffer to read into
 * @param len   Maximum bytes to read
 * @return bytes read, or negative on error
 */
int devfs_read(const char *path, uint8_t *buf, size_t len);

/**
 * Write to a device by path.
 *
 * @param path  Full path (e.g. "/dev/gpio25") or just name ("gpio25")
 * @param buf   Data to write
 * @param len   Number of bytes to write
 * @return bytes written, or negative on error
 */
int devfs_write(const char *path, const uint8_t *buf, size_t len);

/**
 * Device control operation.
 *
 * @param path  Full path or device name
 * @param cmd   Command code
 * @param arg   Command argument
 * @return 0 on success, negative on error
 */
int devfs_ioctl(const char *path, uint32_t cmd, uint32_t arg);

/**
 * List all registered devices into a text buffer.
 *
 * @param buf    Output buffer
 * @param buflen Buffer size
 * @return number of characters written (excluding NUL)
 */
int devfs_list(char *buf, size_t buflen);

/**
 * Look up a device by path or name.
 *
 * @param path  Full path or device name
 * @return pointer to device, or NULL if not found
 */
devfs_device_t *devfs_lookup(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* LITTLEOS_DEVFS_H */
