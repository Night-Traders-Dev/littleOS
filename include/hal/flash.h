/* flash.h - SPI NOR Flash Storage Backend for littleOS FS */
#ifndef LITTLEOS_HAL_FLASH_H
#define LITTLEOS_HAL_FLASH_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "fs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Flash partition layout (within RP2040's 2MB / RP2350's 4MB onboard flash)
 *
 * 0x000000 - 0x100000  Code + data (1 MB reserved)
 * 0x100000 - 0x180000  F2FS partition (512 KB)
 * 0x180000 - 0x1F0000  FAT partition  (448 KB)
 * 0x1F0000 - 0x1FF000  Config storage (existing)
 * 0x1FF000 - 0x200000  Last sector (existing config_storage.c)
 *
 * On RP2350 with 4MB+ flash, FAT partition extends to 0x3F0000 (2.5 MB).
 */

#define FLASH_FS_SECTOR_SIZE        4096u       /* RP2040 flash erase sector */
#define FLASH_FS_PAGE_SIZE          256u        /* RP2040 flash program page */

/* F2FS partition */
#define FLASH_FS_PARTITION_OFFSET   0x100000u   /* 1 MB into flash */
#define FLASH_FS_PARTITION_SIZE     0x080000u   /* 512 KB for F2FS */
#define FLASH_FS_MAX_BLOCKS         (FLASH_FS_PARTITION_SIZE / FS_BLOCK_SIZE)

/* FAT partition (immediately after F2FS) */
#define FLASH_FAT_PARTITION_OFFSET  0x180000u   /* 1.5 MB into flash */

#if defined(PICO_FLASH_SIZE_BYTES) && PICO_FLASH_SIZE_BYTES >= 0x400000
/* 4MB+ flash (RP2350, Feather): FAT gets up to config area */
#define FLASH_FAT_PARTITION_SIZE    0x270000u   /* 2.5 MB - generous */
#else
/* 2MB flash (RP2040): FAT gets remainder before config */
#define FLASH_FAT_PARTITION_SIZE    0x070000u   /* 448 KB */
#endif

#define FLASH_FAT_MAX_SECTORS       (FLASH_FAT_PARTITION_SIZE / 512u)

/* Flash backend context */
typedef struct {
    uint32_t partition_offset;  /* Offset from flash base */
    uint32_t partition_size;    /* Partition size in bytes */
    uint32_t total_blocks;      /* Total FS blocks available */
    bool     initialized;
} flash_backend_t;

/* Initialize flash backend for filesystem use */
int flash_backend_init(void);

/* FS-compatible block I/O callbacks */
int flash_fs_read_block(void *ctx, uint32_t block_addr, uint8_t *buf);
int flash_fs_write_block(void *ctx, uint32_t block_addr, const uint8_t *buf);
int flash_fs_erase_sector(void *ctx, uint32_t sector_addr);

/* Connect flash backend to an fs instance */
int flash_backend_attach(struct fs *filesystem);

/* Get partition info */
uint32_t flash_backend_get_total_blocks(void);
uint32_t flash_backend_get_partition_size(void);

/* Erase entire filesystem partition */
int flash_backend_erase_all(void);

/* FAT flash backend */
int  flash_fat_init(void);
int  flash_fat_read_sector(void *ctx, uint32_t sector, uint8_t *buf);
int  flash_fat_write_sector(void *ctx, uint32_t sector, const uint8_t *buf);
int  flash_fat_erase_all(void);
uint32_t flash_fat_get_total_sectors(void);

/* Safe raw flash helpers for non-filesystem users */
int flash_safe_session_begin(void);
void flash_safe_session_end(void);
int flash_safe_erase_range(uint32_t flash_offset, size_t erase_len);
int flash_safe_program_range(uint32_t flash_offset, const uint8_t *data, size_t len);
int flash_safe_erase_and_program(uint32_t flash_offset,
                                 size_t erase_len,
                                 const uint8_t *data,
                                 size_t program_len);

#ifdef __cplusplus
}
#endif

#endif /* LITTLEOS_HAL_FLASH_H */
