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

/* Flash partition layout (within RP2040's 2MB onboard flash)
 *
 * 0x000000 - 0x100000  Code + data (1 MB reserved)
 * 0x100000 - 0x1F0000  Filesystem partition (960 KB)
 * 0x1F0000 - 0x1FF000  Config storage (existing)
 * 0x1FF000 - 0x200000  Last sector (existing config_storage.c)
 */

#define FLASH_FS_PARTITION_OFFSET   0x100000u   /* 1 MB into flash */
#define FLASH_FS_PARTITION_SIZE     0x0F0000u   /* 960 KB */
#define FLASH_FS_SECTOR_SIZE        4096u       /* RP2040 flash erase sector */
#define FLASH_FS_PAGE_SIZE          256u        /* RP2040 flash program page */

/* Maximum blocks = partition size / FS block size */
#define FLASH_FS_MAX_BLOCKS         (FLASH_FS_PARTITION_SIZE / FS_BLOCK_SIZE)

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

#ifdef __cplusplus
}
#endif

#endif /* LITTLEOS_HAL_FLASH_H */
