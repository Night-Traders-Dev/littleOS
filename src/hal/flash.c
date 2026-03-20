/* flash.c - Flash storage backend for the filesystem (RP2040) */

#include "hal/flash.h"
#include "dmesg.h"
#include "supervisor.h"
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

#ifdef PICO_BUILD
typedef struct {
    uint32_t flash_offset;
    size_t erase_len;
    const uint8_t *data;
    size_t program_len;
} flash_raw_op_t;

static uint32_t flash_session_depth = 0;
static bool flash_session_resume_supervisor = false;

static void __not_in_flash_func(flash_run_raw_op)(void *param) {
    flash_raw_op_t *op = (flash_raw_op_t *)param;
    uint32_t ints = save_and_disable_interrupts();

    if (op->erase_len > 0) {
        flash_range_erase(op->flash_offset, op->erase_len);
    }
    if (op->program_len > 0 && op->data) {
        flash_range_program(op->flash_offset, op->data, op->program_len);
    }

    restore_interrupts(ints);
}
#endif

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

int flash_safe_session_begin(void) {
#ifdef PICO_BUILD
    if (flash_session_depth > 0) {
        flash_session_depth++;
        return 0;
    }

    flash_session_resume_supervisor = supervisor_pause_for_flash();
    flash_session_depth = 1;
    return 0;
#else
    return 0;
#endif
}

void flash_safe_session_end(void) {
#ifdef PICO_BUILD
    if (flash_session_depth == 0) {
        return;
    }

    flash_session_depth--;
    if (flash_session_depth == 0) {
        bool resume_supervisor = flash_session_resume_supervisor;
        flash_session_resume_supervisor = false;
        supervisor_resume_after_flash(resume_supervisor);
    }
#endif
}

int flash_safe_erase_and_program(uint32_t flash_offset,
                                 size_t erase_len,
                                 const uint8_t *data,
                                 size_t program_len) {
#ifdef PICO_BUILD
    if (erase_len == 0 && (program_len == 0 || !data)) {
        return -1;
    }

    if (flash_safe_session_begin() != 0) {
        return -1;
    }

    flash_raw_op_t op = {
        .flash_offset = flash_offset,
        .erase_len = erase_len,
        .data = data,
        .program_len = program_len,
    };
    flash_run_raw_op(&op);
    flash_safe_session_end();
    return 0;
#else
    (void)flash_offset;
    (void)erase_len;
    (void)data;
    (void)program_len;
    return 0;
#endif
}

int flash_safe_erase_range(uint32_t flash_offset, size_t erase_len) {
    return flash_safe_erase_and_program(flash_offset, erase_len, NULL, 0);
}

int flash_safe_program_range(uint32_t flash_offset, const uint8_t *data, size_t len) {
    return flash_safe_erase_and_program(flash_offset, 0, data, len);
}

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
    if (flash_safe_erase_and_program(flash_sector_addr,
                                     FLASH_FS_SECTOR_SIZE,
                                     sector_buf,
                                     FLASH_FS_SECTOR_SIZE) != 0) {
        return -1;
    }
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
    if (flash_safe_erase_range(flash_offset, FLASH_FS_SECTOR_SIZE) != 0) {
        return -1;
    }
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
    if (flash_safe_erase_range(fb->partition_offset, fb->partition_size) != 0) {
        return -1;
    }
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

/* ================================================================== */
/*  FAT Flash Backend                                                  */
/*                                                                     */
/*  Uses the same flash_safe_* primitives as the F2FS backend but     */
/*  operates on the FAT partition (FLASH_FAT_PARTITION_OFFSET).       */
/*  Sector size for FAT is 512 bytes (standard FAT sector).           */
/* ================================================================== */

static struct {
    uint32_t partition_offset;
    uint32_t partition_size;
    uint32_t total_sectors;
    bool     initialized;
} fat_flash_ctx;

static uint8_t fat_sector_buf[FLASH_FS_SECTOR_SIZE]; /* 4KB erase sector */

int flash_fat_init(void) {
    fat_flash_ctx.partition_offset = FLASH_FAT_PARTITION_OFFSET;
    fat_flash_ctx.partition_size   = FLASH_FAT_PARTITION_SIZE;
    fat_flash_ctx.total_sectors    = FLASH_FAT_PARTITION_SIZE / 512u;
    fat_flash_ctx.initialized      = true;

    dmesg_info("flash_fat: partition at 0x%06X, %uKB, %u sectors",
               FLASH_FAT_PARTITION_OFFSET,
               FLASH_FAT_PARTITION_SIZE / 1024,
               fat_flash_ctx.total_sectors);
    return 0;
}

int flash_fat_read_sector(void *ctx, uint32_t sector, uint8_t *buf) {
    (void)ctx;
    if (!fat_flash_ctx.initialized || !buf) return -1;
    if (sector >= fat_flash_ctx.total_sectors) return -1;

#ifdef PICO_BUILD
    const uint8_t *src = (const uint8_t *)(XIP_BASE +
                          fat_flash_ctx.partition_offset +
                          sector * 512u);
    memcpy(buf, src, 512);
#else
    memset(buf, 0xFF, 512);
#endif
    return 0;
}

#ifdef PICO_BUILD
int __not_in_flash_func(flash_fat_write_sector)(void *ctx,
                                                 uint32_t sector,
                                                 const uint8_t *buf) {
#else
int flash_fat_write_sector(void *ctx, uint32_t sector, const uint8_t *buf) {
#endif
    (void)ctx;
    if (!fat_flash_ctx.initialized || !buf) return -1;
    if (sector >= fat_flash_ctx.total_sectors) return -1;

#ifdef PICO_BUILD
    /* Byte offset of the FAT sector within the partition */
    uint32_t byte_offset = sector * 512u;

    /* Align down to the enclosing 4KB erase sector */
    uint32_t erase_offset = byte_offset & ~(FLASH_FS_SECTOR_SIZE - 1u);
    uint32_t offset_in_erase = byte_offset - erase_offset;

    /* Absolute flash offset */
    uint32_t flash_addr = fat_flash_ctx.partition_offset + erase_offset;

    /* Read the full 4KB erase sector */
    const uint8_t *src = (const uint8_t *)(XIP_BASE + flash_addr);
    memcpy(fat_sector_buf, src, FLASH_FS_SECTOR_SIZE);

    /* Patch the 512-byte FAT sector */
    memcpy(fat_sector_buf + offset_in_erase, buf, 512);

    /* Erase + reprogram */
    if (flash_safe_erase_and_program(flash_addr,
                                     FLASH_FS_SECTOR_SIZE,
                                     fat_sector_buf,
                                     FLASH_FS_SECTOR_SIZE) != 0) {
        return -1;
    }
#else
    (void)buf;
#endif
    return 0;
}

int flash_fat_erase_all(void) {
    if (!fat_flash_ctx.initialized) return -1;

#ifdef PICO_BUILD
    if (flash_safe_erase_range(fat_flash_ctx.partition_offset,
                                fat_flash_ctx.partition_size) != 0) {
        return -1;
    }
#endif

    dmesg_info("flash_fat: erased entire FAT partition");
    return 0;
}

uint32_t flash_fat_get_total_sectors(void) {
    return fat_flash_ctx.total_sectors;
}
