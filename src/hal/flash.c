/* flash.c - Flash storage backend for the filesystem (RP2040) */

#include "hal/flash.h"
#include "dmesg.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef PICO_BUILD
#include "hardware/flash.h"
#include "hardware/sync.h"
#include <pico/platform.h>
#endif

#include "fs.h"

static flash_backend_t flash_ctx;
static uint8_t sector_buf[FLASH_FS_SECTOR_SIZE];

/* ------------------------------------------------------------------ */
/*  Init                                                               */
/* ------------------------------------------------------------------ */

int flash_backend_init(void) {
    flash_ctx.partition_offset = FLASH_FS_PARTITION_OFFSET;
    flash_ctx.partition_size   = FLASH_FS_PARTITION_SIZE;
    flash_ctx.total_blocks     = FLASH_FS_PARTITION_SIZE / FS_BLOCK_SIZE;
    flash_ctx.initialized      = true;

    dmesg_info("flash: partition initialized, blocks=%u",
               flash_ctx.total_blocks);

    return 0;
}

/* ------------------------------------------------------------------ */
/*  Read                                                               */
/* ------------------------------------------------------------------ */

int flash_fs_read_block(void *ctx, uint32_t block_addr, uint8_t *buf) {
    (void)ctx;
    flash_backend_t *fb = &flash_ctx;

    if (!fb->initialized) return -1;
    if (block_addr >= fb->total_blocks) return -1;
    if (!buf) return -1;

#ifdef PICO_BUILD
    const uint8_t *src = (const uint8_t *)(XIP_BASE +
                          fb->partition_offset +
                          block_addr * FS_BLOCK_SIZE);
    memcpy(buf, src, FS_BLOCK_SIZE);
#else
    /* Stub: zero-fill when not running on hardware */
    memset(buf, 0xFF, FS_BLOCK_SIZE);
#endif

    return 0;
}

/* ------------------------------------------------------------------ */
/*  Write  (read-modify-write at sector granularity)                   */
/* ------------------------------------------------------------------ */

#ifdef PICO_BUILD
static void __not_in_flash_func(flash_do_program)(uint32_t flash_offset,
                                                   const uint8_t *data,
                                                   size_t len) {
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(flash_offset, FLASH_FS_SECTOR_SIZE);
    flash_range_program(flash_offset, data, len);
    restore_interrupts(ints);
}
#endif

#ifdef PICO_BUILD
int __not_in_flash_func(flash_fs_write_block)(void *ctx,
                                               uint32_t block_addr,
                                               const uint8_t *buf) {
#else
int flash_fs_write_block(void *ctx, uint32_t block_addr, const uint8_t *buf) {
#endif
    (void)ctx;
    flash_backend_t *fb = &flash_ctx;

    if (!fb->initialized) return -1;
    if (block_addr >= fb->total_blocks) return -1;
    if (!buf) return -1;

#ifdef PICO_BUILD
    /* Byte offset of the block within the partition */
    uint32_t byte_offset = block_addr * FS_BLOCK_SIZE;

    /* Align down to the enclosing 4 KB sector */
    uint32_t sector_offset = byte_offset & ~(FLASH_FS_SECTOR_SIZE - 1);
    uint32_t offset_in_sector = byte_offset - sector_offset;

    /* Absolute flash offset (from start of flash, not XIP) */
    uint32_t flash_sector_addr = fb->partition_offset + sector_offset;

    /* Read the entire sector through XIP */
    const uint8_t *src = (const uint8_t *)(XIP_BASE + flash_sector_addr);
    memcpy(sector_buf, src, FLASH_FS_SECTOR_SIZE);

    /* Patch the 512-byte block into the sector buffer */
    memcpy(sector_buf + offset_in_sector, buf, FS_BLOCK_SIZE);

    /* Erase + reprogram the full sector */
    flash_do_program(flash_sector_addr, sector_buf, FLASH_FS_SECTOR_SIZE);
#else
    (void)buf;
#endif

    return 0;
}

/* ------------------------------------------------------------------ */
/*  Erase sector                                                       */
/* ------------------------------------------------------------------ */

int flash_fs_erase_sector(void *ctx, uint32_t sector_addr) {
    (void)ctx;
    flash_backend_t *fb = &flash_ctx;

    if (!fb->initialized) return -1;

    uint32_t byte_offset = sector_addr * FLASH_FS_SECTOR_SIZE;
    if (byte_offset >= fb->partition_size) return -1;

#ifdef PICO_BUILD
    uint32_t flash_offset = fb->partition_offset + byte_offset;

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(flash_offset, FLASH_FS_SECTOR_SIZE);
    restore_interrupts(ints);
#endif

    return 0;
}

/* ------------------------------------------------------------------ */
/*  Erase entire partition                                             */
/* ------------------------------------------------------------------ */

int flash_backend_erase_all(void) {
    flash_backend_t *fb = &flash_ctx;
    if (!fb->initialized) return -1;

#ifdef PICO_BUILD
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(fb->partition_offset, fb->partition_size);
    restore_interrupts(ints);
#endif

    dmesg_info("flash: erased entire partition");
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Attach backend to a filesystem instance                            */
/* ------------------------------------------------------------------ */

int flash_backend_attach(struct fs *fs) {
    if (!fs) return -1;
    if (!flash_ctx.initialized) {
        int r = flash_backend_init();
        if (r != 0) return r;
    }

    fs_set_storage_backend(fs, &flash_ctx,
                           flash_fs_read_block,
                           flash_fs_write_block,
                           flash_fs_erase_sector);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Query helpers                                                      */
/* ------------------------------------------------------------------ */

uint32_t flash_backend_get_total_blocks(void) {
    return flash_ctx.total_blocks;
}

uint32_t flash_backend_get_partition_size(void) {
    return flash_ctx.partition_size;
}
