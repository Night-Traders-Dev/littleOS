/* procfs.c - /proc Virtual Filesystem Implementation for littleOS
 *
 * Provides Linux-style /proc entries for system introspection on RP2040.
 * All generators write into a caller-supplied buffer; no dynamic allocation.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "procfs.h"
#include "board/board_config.h"
#include "scheduler.h"
#include "memory_segmented.h"
#include "hal/dma.h"
#include "dmesg.h"

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/structs/scb.h"
#endif

/* ============================================================================
 * Registration Table
 * ========================================================================== */

typedef struct {
    char           path[PROCFS_MAX_PATH];
    procfs_read_fn read_fn;
} procfs_entry_t;

static procfs_entry_t procfs_table[PROCFS_MAX_ENTRIES];
static uint8_t        procfs_count = 0;
static bool           procfs_initialized = false;

/* Forward declarations for built-in generators */
static int procfs_gen_cpuinfo(char *buf, size_t buflen);
static int procfs_gen_meminfo(char *buf, size_t buflen);
static int procfs_gen_uptime(char *buf, size_t buflen);
static int procfs_gen_version(char *buf, size_t buflen);
static int procfs_gen_tasks(char *buf, size_t buflen);
static int procfs_gen_mounts(char *buf, size_t buflen);
static int procfs_gen_interrupts(char *buf, size_t buflen);
static int procfs_gen_gpio(char *buf, size_t buflen);
static int procfs_gen_dma(char *buf, size_t buflen);

/* ============================================================================
 * Public API
 * ========================================================================== */

int procfs_register(const char *path, procfs_read_fn read_fn) {
    if (!path || !read_fn) return -1;
    if (procfs_count >= PROCFS_MAX_ENTRIES) return -2;
    if (strlen(path) >= PROCFS_MAX_PATH) return -3;

    /* Reject duplicates */
    for (uint8_t i = 0; i < procfs_count; i++) {
        if (strcmp(procfs_table[i].path, path) == 0) return -4;
    }

    procfs_entry_t *e = &procfs_table[procfs_count++];
    strncpy(e->path, path, PROCFS_MAX_PATH - 1);
    e->path[PROCFS_MAX_PATH - 1] = '\0';
    e->read_fn = read_fn;
    return 0;
}

void procfs_init(void) {
    if (procfs_initialized) return;

    procfs_count = 0;
    memset(procfs_table, 0, sizeof(procfs_table));

    procfs_register("/proc/cpuinfo",    procfs_gen_cpuinfo);
    procfs_register("/proc/meminfo",    procfs_gen_meminfo);
    procfs_register("/proc/uptime",     procfs_gen_uptime);
    procfs_register("/proc/version",    procfs_gen_version);
    procfs_register("/proc/tasks",      procfs_gen_tasks);
    procfs_register("/proc/mounts",     procfs_gen_mounts);
    procfs_register("/proc/interrupts", procfs_gen_interrupts);
    procfs_register("/proc/gpio",       procfs_gen_gpio);
    procfs_register("/proc/dma",        procfs_gen_dma);

    procfs_initialized = true;
    dmesg_info("procfs: initialized with %d entries", procfs_count);
}

int procfs_read(const char *path, char *buf, size_t buflen) {
    if (!path || !buf || buflen == 0) return -1;

    for (uint8_t i = 0; i < procfs_count; i++) {
        if (strcmp(procfs_table[i].path, path) == 0) {
            return procfs_table[i].read_fn(buf, buflen);
        }
    }
    return -2; /* not found */
}

int procfs_list(const char *path, char *buf, size_t buflen) {
    if (!path || !buf || buflen == 0) return -1;

    /* Normalise: treat "/proc" and "/proc/" the same */
    size_t plen = strlen(path);
    char dir[PROCFS_MAX_PATH];
    strncpy(dir, path, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';
    if (plen > 1 && dir[plen - 1] == '/') {
        dir[plen - 1] = '\0';
        plen--;
    }

    int written = 0;
    buf[0] = '\0';

    for (uint8_t i = 0; i < procfs_count; i++) {
        const char *entry = procfs_table[i].path;

        /* Entry must start with dir + '/' */
        if (strncmp(entry, dir, plen) != 0) continue;
        if (entry[plen] != '/') continue;

        /* Extract the name component right after the prefix */
        const char *name = entry + plen + 1;

        /* Skip deeper nested entries (only list immediate children) */
        if (strchr(name, '/') != NULL) continue;

        int n = snprintf(buf + written, buflen - (size_t)written, "%s\r\n", name);
        if (n < 0 || (size_t)(written + n) >= buflen) break;
        written += n;
    }

    return written;
}

/* ============================================================================
 * Built-in Generators
 * ========================================================================== */

/* Helper: safe append to buffer, returns new write position */
static int buf_append(char *buf, size_t buflen, int pos, const char *fmt, ...)
    __attribute__((format(printf, 4, 5)));

static int buf_append(char *buf, size_t buflen, int pos, const char *fmt, ...) {
    if (pos < 0 || (size_t)pos >= buflen) return pos;

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf + pos, buflen - (size_t)pos, fmt, ap);
    va_end(ap);

    if (n < 0) return pos;
    return pos + n;
}

/* ---- /proc/cpuinfo ---- */
static int procfs_gen_cpuinfo(char *buf, size_t buflen) {
    int pos = 0;

#ifdef PICO_BUILD
    uint32_t freq_khz = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);
    uint32_t freq_mhz = freq_khz / 1000;
#else
    uint32_t freq_mhz = 125;
#endif

    for (int core = 0; core < 2; core++) {
        pos = buf_append(buf, buflen, pos,
            "processor\t: %d\r\n"
            "model name\t: " CHIP_CORE_STR "\r\n"
            "cpu MHz\t\t: %lu\r\n"
            "Features\t: thumb\r\n"
            "\r\n",
            core, (unsigned long)freq_mhz);
    }

    return pos;
}

/* ---- /proc/meminfo ---- */
static int procfs_gen_meminfo(char *buf, size_t buflen) {
    MemoryStats stats = memory_get_stats();

    size_t total_kb = (stats.kernel_used + stats.kernel_free +
                       stats.interpreter_used + stats.interpreter_free) / 1024;
    size_t used_kb  = (stats.kernel_used + stats.interpreter_used) / 1024;
    size_t free_kb  = (stats.kernel_free + stats.interpreter_free) / 1024;

    int pos = 0;
    pos = buf_append(buf, buflen, pos, "MemTotal:\t%6lu kB\r\n", (unsigned long)total_kb);
    pos = buf_append(buf, buflen, pos, "MemFree:\t%6lu kB\r\n",  (unsigned long)free_kb);
    pos = buf_append(buf, buflen, pos, "MemUsed:\t%6lu kB\r\n",  (unsigned long)used_kb);
    pos = buf_append(buf, buflen, pos, "KernelUsed:\t%6lu kB\r\n",
                     (unsigned long)(stats.kernel_used / 1024));
    pos = buf_append(buf, buflen, pos, "KernelFree:\t%6lu kB\r\n",
                     (unsigned long)(stats.kernel_free / 1024));
    pos = buf_append(buf, buflen, pos, "KernelPeak:\t%6lu kB\r\n",
                     (unsigned long)(stats.kernel_peak / 1024));
    pos = buf_append(buf, buflen, pos, "InterpUsed:\t%6lu kB\r\n",
                     (unsigned long)(stats.interpreter_used / 1024));
    pos = buf_append(buf, buflen, pos, "InterpFree:\t%6lu kB\r\n",
                     (unsigned long)(stats.interpreter_free / 1024));
    pos = buf_append(buf, buflen, pos, "InterpPeak:\t%6lu kB\r\n",
                     (unsigned long)(stats.interpreter_peak / 1024));
    pos = buf_append(buf, buflen, pos, "SRAMTotal:\t%6u kB\r\n",
                     (unsigned)(CHIP_RAM_SIZE / 1024));

    return pos;
}

/* ---- /proc/uptime ---- */
static int procfs_gen_uptime(char *buf, size_t buflen) {
#ifdef PICO_BUILD
    uint64_t ms = to_ms_since_boot(get_absolute_time());
#else
    uint64_t ms = (uint64_t)dmesg_get_uptime();
#endif

    uint32_t secs      = (uint32_t)(ms / 1000);
    uint32_t hundredths = (uint32_t)((ms % 1000) / 10);

    return snprintf(buf, buflen, "%lu.%02lu\r\n",
                    (unsigned long)secs, (unsigned long)hundredths);
}

/* ---- /proc/version ---- */
static int procfs_gen_version(char *buf, size_t buflen) {
    return snprintf(buf, buflen,
        "littleOS version 0.5.0 (gcc) #1 SMP\r\n");
}

/* ---- /proc/tasks ---- */
static int procfs_gen_tasks(char *buf, size_t buflen) {
    static const char *state_names[] = {
        "idle", "ready", "run", "block", "susp", "term"
    };

    int pos = 0;
    pos = buf_append(buf, buflen, pos,
        "PID  STATE  NAME                     MEM(B)  RUNTIME(ms)\r\n");

    for (uint16_t id = 0; id < LITTLEOS_MAX_TASKS; id++) {
        task_descriptor_t desc;
        if (!task_get_descriptor(id, &desc)) continue;
        if (desc.state == TASK_STATE_TERMINATED) continue;

        const char *st = (desc.state < 6) ? state_names[desc.state] : "???";
        pos = buf_append(buf, buflen, pos,
            "%3u  %-5s  %-24s %6lu  %lu\r\n",
            desc.task_id, st, desc.name,
            (unsigned long)desc.memory_allocated,
            (unsigned long)desc.total_runtime_ms);
    }

    return pos;
}

/* ---- /proc/mounts ---- */
static int procfs_gen_mounts(char *buf, size_t buflen) {
    int pos = 0;
    pos = buf_append(buf, buflen, pos, "procfs /proc procfs rw 0 0\r\n");
    pos = buf_append(buf, buflen, pos, "littlefs /flash littlefs rw 0 0\r\n");
    return pos;
}

/* ---- /proc/interrupts ---- */

#ifdef PICO_BUILD
/* Track IRQ fire counts (updated by ISRs that call procfs_irq_inc) */
static volatile uint32_t irq_counts[32];
#endif

static int procfs_gen_interrupts(char *buf, size_t buflen) {
    int pos = 0;
    pos = buf_append(buf, buflen, pos, "IRQ  COUNT\r\n");

#ifdef PICO_BUILD
    for (int i = 0; i < 32; i++) {
        if (irq_counts[i] > 0) {
            pos = buf_append(buf, buflen, pos, "%3d  %lu\r\n",
                             i, (unsigned long)irq_counts[i]);
        }
    }
#else
    pos = buf_append(buf, buflen, pos, "(stub - no hardware)\r\n");
#endif

    return pos;
}

/* ---- /proc/gpio ---- */
static int procfs_gen_gpio(char *buf, size_t buflen) {
    int pos = 0;
    pos = buf_append(buf, buflen, pos, "PIN  DIR  VAL  FUNC\r\n");

#ifdef PICO_BUILD
    for (int pin = 0; pin < (int)NUM_BANK0_GPIOS; pin++) {
        bool dir = gpio_get_dir(pin);       /* true = out */
        bool val = gpio_get(pin);
        uint gpio_func = gpio_get_function(pin);

        const char *func_name;
        switch (gpio_func) {
#if !PICO_RP2350
            case GPIO_FUNC_XIP:  func_name = "XIP";  break;
#endif
            case GPIO_FUNC_SPI:  func_name = "SPI";  break;
            case GPIO_FUNC_UART: func_name = "UART"; break;
            case GPIO_FUNC_I2C:  func_name = "I2C";  break;
            case GPIO_FUNC_PWM:  func_name = "PWM";  break;
            case GPIO_FUNC_SIO:  func_name = "SIO";  break;
            case GPIO_FUNC_PIO0: func_name = "PIO0"; break;
            case GPIO_FUNC_PIO1: func_name = "PIO1"; break;
#if PICO_RP2350
            case GPIO_FUNC_PIO2: func_name = "PIO2"; break;
            case GPIO_FUNC_HSTX: func_name = "HSTX"; break;
#endif
            case GPIO_FUNC_NULL: func_name = "NULL"; break;
            default:             func_name = "???";  break;
        }

        pos = buf_append(buf, buflen, pos, "%3d  %-3s  %d    %s\r\n",
                         pin,
                         dir ? "out" : "in",
                         val ? 1 : 0,
                         func_name);
    }
#else
    for (int pin = 0; pin < 30; pin++) {
        pos = buf_append(buf, buflen, pos, "%3d  in   0    SIO\r\n", pin);
    }
#endif

    return pos;
}

/* ---- /proc/dma ---- */
static int procfs_gen_dma(char *buf, size_t buflen) {
    int pos = 0;
    pos = buf_append(buf, buflen, pos,
        "CH  CLAIMED  BUSY  TRANSFERS  BYTES\r\n");

    for (int ch = 0; ch < DMA_NUM_CHANNELS; ch++) {
        dma_channel_status_t st;
        if (dma_hal_status(ch, &st) != 0) {
            pos = buf_append(buf, buflen, pos,
                "%2d  (error)\r\n", ch);
            continue;
        }
        pos = buf_append(buf, buflen, pos,
            "%2d  %-7s  %-4s  %9lu  %lu\r\n",
            ch,
            st.claimed ? "yes" : "no",
            st.busy    ? "yes" : "no",
            (unsigned long)st.total_transfers,
            (unsigned long)st.total_bytes);
    }

    return pos;
}
