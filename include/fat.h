/* fat.h - FAT12/FAT16 filesystem for littleOS */
#ifndef LITTLEOS_FAT_H
#define LITTLEOS_FAT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================
 * Constants
 * ========================= */
#define FAT_SECTOR_SIZE         512u
#define FAT_MAX_NAME            11u   /* 8.3 format: 8 name + 3 ext */
#define FAT_MAX_PATH            128u

#define FAT_ATTR_READ_ONLY      0x01
#define FAT_ATTR_HIDDEN         0x02
#define FAT_ATTR_SYSTEM         0x04
#define FAT_ATTR_VOLUME_ID      0x08
#define FAT_ATTR_DIRECTORY      0x10
#define FAT_ATTR_ARCHIVE        0x20

/* FAT12/16 cluster markers */
#define FAT12_EOC               0x0FF8u
#define FAT12_FREE              0x0000u
#define FAT12_BAD               0x0FF7u
#define FAT16_EOC               0xFFF8u
#define FAT16_FREE              0x0000u
#define FAT16_BAD               0xFFF7u

/* FAT type */
typedef enum {
    FAT_TYPE_12 = 12,
    FAT_TYPE_16 = 16
} fat_type_t;

/* Error codes (match fs.h convention) */
#define FAT_OK                  0
#define FAT_ERR_NO_SPACE        (-1)
#define FAT_ERR_NOT_FOUND       (-2)
#define FAT_ERR_EXISTS          (-3)
#define FAT_ERR_IO              (-6)
#define FAT_ERR_NOT_DIR         (-7)
#define FAT_ERR_INVALID         (-10)
#define FAT_ERR_CORRUPTED       (-9)

/* =========================
 * On-disk structures
 * ========================= */

#pragma pack(push, 1)

/* BIOS Parameter Block (BPB) - first sector of volume */
typedef struct {
    uint8_t  jmp_boot[3];
    uint8_t  oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    /* FAT12/16 extended */
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_sig;
    uint32_t volume_serial;
    uint8_t  volume_label[11];
    uint8_t  fs_type[8];
    uint8_t  boot_code[448];
    uint16_t signature;     /* 0xAA55 */
} fat_bpb_t;
_Static_assert(sizeof(fat_bpb_t) == 512, "BPB must be 512 bytes");

/* Directory entry (32 bytes) */
typedef struct {
    uint8_t  name[11];      /* 8.3 format, space-padded */
    uint8_t  attr;
    uint8_t  nt_reserved;
    uint8_t  create_time_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t first_cluster_hi; /* always 0 for FAT12/16 */
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_lo;
    uint32_t file_size;
} fat_dirent_t;
_Static_assert(sizeof(fat_dirent_t) == 32, "dirent must be 32 bytes");

#pragma pack(pop)

/* =========================
 * In-memory structures
 * ========================= */

/* Storage backend (same pattern as F2FS) */
typedef int (*fat_read_sector_fn)(void *ctx, uint32_t sector, uint8_t *buf);
typedef int (*fat_write_sector_fn)(void *ctx, uint32_t sector, const uint8_t *buf);

/* Mounted FAT volume */
typedef struct {
    /* Backend */
    void *storage_ctx;
    fat_read_sector_fn  read_sector;
    fat_write_sector_fn write_sector;

    /* BPB cache */
    fat_bpb_t bpb;

    /* Derived geometry */
    fat_type_t type;
    uint32_t total_sectors;
    uint32_t fat_start;         /* first sector of FAT */
    uint32_t root_dir_start;    /* first sector of root dir (FAT12/16) */
    uint32_t root_dir_sectors;  /* sectors occupied by root dir */
    uint32_t data_start;        /* first sector of data area */
    uint32_t total_clusters;    /* total data clusters */

    /* In-memory FAT (small enough for RAM on RP2040) */
    uint16_t *fat_table;        /* cluster -> next cluster mapping */
    uint32_t fat_table_entries;

    bool mounted;
    bool dirty;
} fat_vol_t;

/* Open file handle */
typedef struct {
    uint16_t first_cluster;
    uint32_t file_size;
    uint32_t position;
    uint16_t dir_cluster;       /* cluster of parent dir (0 = root) */
    uint16_t dir_entry_index;   /* index within parent dir */
    uint8_t  attr;
    bool     is_open;
    bool     is_dir;
} fat_file_t;

/* Directory iteration state */
typedef struct {
    char     name[13];          /* 8.3 decoded: "FILENAME.EXT\0" */
    uint8_t  attr;
    uint32_t file_size;
    uint16_t first_cluster;
} fat_dir_entry_t;

/* =========================
 * Public API
 * ========================= */

/* Setup */
void fat_set_backend(fat_vol_t *vol, void *ctx,
                     fat_read_sector_fn rfn, fat_write_sector_fn wfn);

/* Format a volume as FAT12 or FAT16 */
int fat_format(fat_vol_t *vol, uint32_t total_sectors, fat_type_t type,
               const char *label);

/* Mount / unmount */
int fat_mount(fat_vol_t *vol);
int fat_unmount(fat_vol_t *vol);
int fat_sync(fat_vol_t *vol);

/* File operations */
int fat_open(fat_vol_t *vol, const char *path, fat_file_t *fd, bool create);
int fat_close(fat_vol_t *vol, fat_file_t *fd);
int fat_read(fat_vol_t *vol, fat_file_t *fd, uint8_t *buf, uint32_t count);
int fat_write(fat_vol_t *vol, fat_file_t *fd, const uint8_t *buf, uint32_t count);

/* Directory operations */
int fat_mkdir(fat_vol_t *vol, const char *path);
int fat_opendir(fat_vol_t *vol, const char *path, fat_file_t *fd);
int fat_readdir(fat_vol_t *vol, fat_file_t *fd, fat_dir_entry_t *entry);
int fat_unlink(fat_vol_t *vol, const char *path);

/* Info */
int fat_stat(fat_vol_t *vol, uint32_t *total_bytes, uint32_t *free_bytes);
const char *fat_type_str(fat_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* LITTLEOS_FAT_H */
