/* procfs.h - /proc Virtual Filesystem for littleOS */
#ifndef LITTLEOS_PROCFS_H
#define LITTLEOS_PROCFS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum registered virtual file entries */
#define PROCFS_MAX_ENTRIES      24

/* Maximum path length for a procfs entry */
#define PROCFS_MAX_PATH         48

/**
 * Read function signature for a virtual file.
 *
 * @param buf     Output buffer
 * @param buflen  Buffer capacity
 * @return Number of bytes written to buf, or negative on error
 */
typedef int (*procfs_read_fn)(char *buf, size_t buflen);

/**
 * Initialize the /proc virtual filesystem.
 * Registers all built-in entries (cpuinfo, meminfo, uptime, etc.).
 */
void procfs_init(void);

/**
 * Read a virtual file by path.
 *
 * @param path    Full path (e.g. "/proc/cpuinfo")
 * @param buf     Output buffer
 * @param buflen  Buffer capacity
 * @return Number of bytes written, or negative on error
 */
int procfs_read(const char *path, char *buf, size_t buflen);

/**
 * List entries in a virtual directory.
 *
 * @param path    Directory path (e.g. "/proc" or "/proc/")
 * @param buf     Output buffer (newline-separated names)
 * @param buflen  Buffer capacity
 * @return Number of bytes written, or negative on error
 */
int procfs_list(const char *path, char *buf, size_t buflen);

/**
 * Register a custom virtual file.
 *
 * @param path     Full path (e.g. "/proc/myfile")
 * @param read_fn  Generator function
 * @return 0 on success, negative on error
 */
int procfs_register(const char *path, procfs_read_fn read_fn);

/* ============================================================================
 * Built-in /proc entries
 * ========================================================================== */

/* /proc/cpuinfo  - RP2040 dual-core Cortex-M0+ info, clock speed          */
/* /proc/meminfo  - 264KB SRAM breakdown (used/free/segments)               */
/* /proc/uptime   - System uptime in seconds.hundredths                     */
/* /proc/version  - littleOS version string                                 */
/* /proc/tasks    - Task list from scheduler                                */
/* /proc/mounts   - Filesystem mount information                            */
/* /proc/interrupts - IRQ counts                                            */
/* /proc/gpio     - GPIO pin states (all 30 pins)                           */
/* /proc/dma      - DMA channel status                                      */

#ifdef __cplusplus
}
#endif

#endif /* LITTLEOS_PROCFS_H */
