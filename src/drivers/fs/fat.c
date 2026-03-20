/* fat.c - FAT12/FAT16 filesystem implementation for littleOS
 *
 * Supports both FAT12 (up to 4084 clusters) and FAT16 (up to 65524 clusters)
 * with in-memory FAT table for fast access on RAM-constrained RP2040/RP2350.
 *
 * Storage backend is the same read_sector/write_sector interface as the
 * F2FS-style filesystem, enabling RAM or flash backends.
 */

#include "fat.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* =========================
 * Internal helpers
 * ========================= */

static int fat_read_sector_i(fat_vol_t *vol, uint32_t sector, uint8_t *buf) {
    if (!vol || !vol->read_sector || !buf) return FAT_ERR_INVALID;
    return vol->read_sector(vol->storage_ctx, sector, buf);
}

static int fat_write_sector_i(fat_vol_t *vol, uint32_t sector, const uint8_t *buf) {
    if (!vol || !vol->write_sector || !buf) return FAT_ERR_INVALID;
    return vol->write_sector(vol->storage_ctx, sector, buf);
}

/* Convert cluster number to first sector of that cluster */
static uint32_t cluster_to_sector(const fat_vol_t *vol, uint16_t cluster) {
    return vol->data_start + (uint32_t)(cluster - 2) * vol->bpb.sectors_per_cluster;
}

/* Get/set FAT table entries */
static uint16_t fat_get_entry(const fat_vol_t *vol, uint16_t cluster) {
    if (cluster >= vol->fat_table_entries) return (vol->type == FAT_TYPE_12) ? FAT12_EOC : FAT16_EOC;
    return vol->fat_table[cluster];
}

static void fat_set_entry(fat_vol_t *vol, uint16_t cluster, uint16_t value) {
    if (cluster < vol->fat_table_entries) {
        vol->fat_table[cluster] = value;
        vol->dirty = true;
    }
}

static bool fat_is_eoc(const fat_vol_t *vol, uint16_t cluster) {
    if (vol->type == FAT_TYPE_12) return cluster >= FAT12_EOC;
    return cluster >= FAT16_EOC;
}

static uint16_t fat_eoc_marker(const fat_vol_t *vol) {
    return (vol->type == FAT_TYPE_12) ? 0x0FFFu : 0xFFFFu;
}

static uint16_t fat_free_marker(void) {
    return 0x0000u;
}

/* Find a free cluster */
static uint16_t fat_alloc_cluster(fat_vol_t *vol) {
    for (uint16_t c = 2; c < vol->fat_table_entries; c++) {
        if (vol->fat_table[c] == fat_free_marker()) {
            fat_set_entry(vol, c, fat_eoc_marker(vol));
            return c;
        }
    }
    return 0; /* no free cluster */
}

/* Free a chain starting at cluster */
static void fat_free_chain(fat_vol_t *vol, uint16_t start) {
    uint16_t c = start;
    while (c >= 2 && c < vol->fat_table_entries && !fat_is_eoc(vol, c)) {
        uint16_t next = fat_get_entry(vol, c);
        fat_set_entry(vol, c, fat_free_marker());
        c = next;
    }
}

/* =========================
 * 8.3 filename helpers
 * ========================= */

/* Convert "FILENAMEEXT" (11 bytes, space-padded) to "FILENAME.EXT\0" */
static void fat83_to_str(const uint8_t name83[11], char *out) {
    int i, j = 0;

    /* Copy name part, trim trailing spaces */
    for (i = 0; i < 8 && name83[i] != ' '; i++)
        out[j++] = (char)name83[i];

    /* Add dot and extension if present */
    if (name83[8] != ' ') {
        out[j++] = '.';
        for (i = 8; i < 11 && name83[i] != ' '; i++)
            out[j++] = (char)name83[i];
    }

    out[j] = '\0';
}

/* Convert "filename.ext" to 11-byte 8.3 format, uppercase, space-padded */
static int str_to_fat83(const char *name, uint8_t out[11]) {
    memset(out, ' ', 11);

    if (!name || name[0] == '\0' || name[0] == '.') return FAT_ERR_INVALID;

    int i = 0, j = 0;
    /* Name part (up to 8 chars before dot) */
    while (name[i] && name[i] != '.' && j < 8) {
        out[j++] = (uint8_t)toupper((unsigned char)name[i++]);
    }

    /* Skip to extension */
    if (name[i] == '.') {
        i++;
        j = 8;
        while (name[i] && j < 11) {
            out[j++] = (uint8_t)toupper((unsigned char)name[i++]);
        }
    }

    return FAT_OK;
}

/* =========================
 * FAT table serialization
 * ========================= */

/* Write in-memory FAT table to on-disk sectors (both FAT copies) */
static int fat_write_table(fat_vol_t *vol) {
    if (!vol || !vol->fat_table) return FAT_ERR_INVALID;

    uint8_t sector[FAT_SECTOR_SIZE];
    uint32_t fat_sectors = vol->bpb.fat_size_16;

    for (uint8_t fat_copy = 0; fat_copy < vol->bpb.num_fats; fat_copy++) {
        uint32_t fat_base = vol->fat_start + (uint32_t)fat_copy * fat_sectors;

        if (vol->type == FAT_TYPE_16) {
            /* FAT16: 2 bytes per entry, straightforward */
            uint32_t entries_per_sector = FAT_SECTOR_SIZE / 2;
            for (uint32_t s = 0; s < fat_sectors; s++) {
                memset(sector, 0, sizeof(sector));
                uint16_t *p = (uint16_t *)sector;
                for (uint32_t i = 0; i < entries_per_sector; i++) {
                    uint32_t idx = s * entries_per_sector + i;
                    if (idx < vol->fat_table_entries)
                        p[i] = vol->fat_table[idx];
                }
                int r = fat_write_sector_i(vol, fat_base + s, sector);
                if (r != FAT_OK) return r;
            }
        } else {
            /* FAT12: 12 bits per entry, packed */
            memset(sector, 0, sizeof(sector));
            uint32_t byte_offset = 0;
            uint32_t current_sector = 0;

            for (uint32_t idx = 0; idx < vol->fat_table_entries; idx++) {
                uint16_t val = vol->fat_table[idx];
                uint32_t fat_byte = idx + (idx / 2); /* 1.5 bytes per entry */
                uint32_t sec = fat_byte / FAT_SECTOR_SIZE;
                uint32_t off = fat_byte % FAT_SECTOR_SIZE;

                /* Flush sector if we moved to a new one */
                if (sec != current_sector) {
                    int r = fat_write_sector_i(vol, fat_base + current_sector, sector);
                    if (r != FAT_OK) return r;
                    memset(sector, 0, sizeof(sector));
                    current_sector = sec;
                }

                if (idx & 1) {
                    /* Odd entry: high nibble of byte[off], all of byte[off+1] */
                    sector[off] = (sector[off] & 0x0F) | (uint8_t)((val & 0x0F) << 4);
                    if (off + 1 < FAT_SECTOR_SIZE) {
                        sector[off + 1] = (uint8_t)(val >> 4);
                    } else {
                        /* Entry spans sector boundary */
                        int r = fat_write_sector_i(vol, fat_base + current_sector, sector);
                        if (r != FAT_OK) return r;
                        memset(sector, 0, sizeof(sector));
                        current_sector++;
                        sector[0] = (uint8_t)(val >> 4);
                    }
                } else {
                    /* Even entry: all of byte[off], low nibble of byte[off+1] */
                    sector[off] = (uint8_t)(val & 0xFF);
                    if (off + 1 < FAT_SECTOR_SIZE) {
                        sector[off + 1] = (sector[off + 1] & 0xF0) | (uint8_t)((val >> 8) & 0x0F);
                    } else {
                        int r = fat_write_sector_i(vol, fat_base + current_sector, sector);
                        if (r != FAT_OK) return r;
                        memset(sector, 0, sizeof(sector));
                        current_sector++;
                        sector[0] = (uint8_t)((val >> 8) & 0x0F);
                    }
                }

                byte_offset = fat_byte + ((idx & 1) ? 2 : 1);
            }
            /* Flush last sector */
            int r = fat_write_sector_i(vol, fat_base + current_sector, sector);
            if (r != FAT_OK) return r;

            /* Zero remaining sectors */
            memset(sector, 0, sizeof(sector));
            for (uint32_t s = current_sector + 1; s < fat_sectors; s++) {
                int r2 = fat_write_sector_i(vol, fat_base + s, sector);
                if (r2 != FAT_OK) return r2;
            }
        }
    }

    vol->dirty = false;
    return FAT_OK;
}

/* Read on-disk FAT into in-memory table */
static int fat_read_table(fat_vol_t *vol) {
    if (!vol || !vol->fat_table) return FAT_ERR_INVALID;

    uint8_t sector[FAT_SECTOR_SIZE];

    if (vol->type == FAT_TYPE_16) {
        uint32_t entries_per_sector = FAT_SECTOR_SIZE / 2;
        for (uint32_t s = 0; s < vol->bpb.fat_size_16; s++) {
            int r = fat_read_sector_i(vol, vol->fat_start + s, sector);
            if (r != FAT_OK) return r;
            const uint16_t *p = (const uint16_t *)sector;
            for (uint32_t i = 0; i < entries_per_sector; i++) {
                uint32_t idx = s * entries_per_sector + i;
                if (idx < vol->fat_table_entries)
                    vol->fat_table[idx] = p[i];
            }
        }
    } else {
        /* FAT12: read packed 12-bit entries */
        /* Read all FAT sectors into a flat buffer */
        uint32_t fat_bytes = (uint32_t)vol->bpb.fat_size_16 * FAT_SECTOR_SIZE;
        uint8_t *fat_raw = (uint8_t *)calloc(fat_bytes, 1);
        if (!fat_raw) return FAT_ERR_NO_SPACE;

        for (uint32_t s = 0; s < vol->bpb.fat_size_16; s++) {
            int r = fat_read_sector_i(vol, vol->fat_start + s, fat_raw + s * FAT_SECTOR_SIZE);
            if (r != FAT_OK) { free(fat_raw); return r; }
        }

        for (uint32_t idx = 0; idx < vol->fat_table_entries; idx++) {
            uint32_t fat_byte = idx + (idx / 2);
            if (fat_byte + 1 >= fat_bytes) break;

            uint16_t raw = (uint16_t)fat_raw[fat_byte] | ((uint16_t)fat_raw[fat_byte + 1] << 8);
            if (idx & 1)
                vol->fat_table[idx] = raw >> 4;
            else
                vol->fat_table[idx] = raw & 0x0FFF;
        }

        free(fat_raw);
    }

    return FAT_OK;
}

/* =========================
 * Root directory helpers
 * ========================= */

/* Read a directory entry from root dir or cluster chain */
static int fat_read_dirent(fat_vol_t *vol, uint16_t dir_cluster,
                           uint32_t index, fat_dirent_t *out) {
    uint8_t sector[FAT_SECTOR_SIZE];
    uint32_t entries_per_sector = FAT_SECTOR_SIZE / sizeof(fat_dirent_t);
    uint32_t sector_offset = index / entries_per_sector;
    uint32_t entry_offset = index % entries_per_sector;

    uint32_t abs_sector;
    if (dir_cluster == 0) {
        /* Root directory (fixed location) */
        if (sector_offset >= vol->root_dir_sectors) return FAT_ERR_NOT_FOUND;
        abs_sector = vol->root_dir_start + sector_offset;
    } else {
        /* Subdirectory: follow cluster chain */
        uint16_t c = dir_cluster;
        uint32_t sectors_per_cluster = vol->bpb.sectors_per_cluster;
        uint32_t cluster_index = sector_offset / sectors_per_cluster;
        uint32_t sector_in_cluster = sector_offset % sectors_per_cluster;

        for (uint32_t i = 0; i < cluster_index; i++) {
            c = fat_get_entry(vol, c);
            if (fat_is_eoc(vol, c) || c < 2) return FAT_ERR_NOT_FOUND;
        }
        abs_sector = cluster_to_sector(vol, c) + sector_in_cluster;
    }

    int r = fat_read_sector_i(vol, abs_sector, sector);
    if (r != FAT_OK) return r;

    memcpy(out, sector + entry_offset * sizeof(fat_dirent_t), sizeof(fat_dirent_t));
    return FAT_OK;
}

/* Write a directory entry */
static int fat_write_dirent(fat_vol_t *vol, uint16_t dir_cluster,
                            uint32_t index, const fat_dirent_t *entry) {
    uint8_t sector[FAT_SECTOR_SIZE];
    uint32_t entries_per_sector = FAT_SECTOR_SIZE / sizeof(fat_dirent_t);
    uint32_t sector_offset = index / entries_per_sector;
    uint32_t entry_offset = index % entries_per_sector;

    uint32_t abs_sector;
    if (dir_cluster == 0) {
        if (sector_offset >= vol->root_dir_sectors) return FAT_ERR_NO_SPACE;
        abs_sector = vol->root_dir_start + sector_offset;
    } else {
        uint16_t c = dir_cluster;
        uint32_t sectors_per_cluster = vol->bpb.sectors_per_cluster;
        uint32_t cluster_index = sector_offset / sectors_per_cluster;
        uint32_t sector_in_cluster = sector_offset % sectors_per_cluster;

        for (uint32_t i = 0; i < cluster_index; i++) {
            c = fat_get_entry(vol, c);
            if (fat_is_eoc(vol, c) || c < 2) return FAT_ERR_NO_SPACE;
        }
        abs_sector = cluster_to_sector(vol, c) + sector_in_cluster;
    }

    int r = fat_read_sector_i(vol, abs_sector, sector);
    if (r != FAT_OK) return r;

    memcpy(sector + entry_offset * sizeof(fat_dirent_t), entry, sizeof(fat_dirent_t));
    return fat_write_sector_i(vol, abs_sector, sector);
}

/* Get max entries in a directory */
static uint32_t fat_dir_max_entries(fat_vol_t *vol, uint16_t dir_cluster) {
    if (dir_cluster == 0) {
        return vol->bpb.root_entry_count;
    }
    /* Subdirectory: count entries across cluster chain */
    uint32_t entries_per_cluster = (uint32_t)vol->bpb.sectors_per_cluster *
                                   FAT_SECTOR_SIZE / sizeof(fat_dirent_t);
    uint32_t count = 0;
    uint16_t c = dir_cluster;
    while (c >= 2 && !fat_is_eoc(vol, c)) {
        count += entries_per_cluster;
        c = fat_get_entry(vol, c);
    }
    return count;
}

/* Find a directory entry by 8.3 name within a directory */
static int fat_dir_find(fat_vol_t *vol, uint16_t dir_cluster,
                        const uint8_t name83[11], uint32_t *out_index,
                        fat_dirent_t *out_entry) {
    uint32_t max = fat_dir_max_entries(vol, dir_cluster);
    fat_dirent_t de;

    for (uint32_t i = 0; i < max; i++) {
        int r = fat_read_dirent(vol, dir_cluster, i, &de);
        if (r != FAT_OK) return r;

        if (de.name[0] == 0x00) break;       /* end of directory */
        if (de.name[0] == 0xE5) continue;    /* deleted entry */
        if (de.attr == 0x0F) continue;        /* LFN entry */

        if (memcmp(de.name, name83, 11) == 0) {
            if (out_index) *out_index = i;
            if (out_entry) *out_entry = de;
            return FAT_OK;
        }
    }

    return FAT_ERR_NOT_FOUND;
}

/* Add a new entry to a directory */
static int fat_dir_add_entry(fat_vol_t *vol, uint16_t dir_cluster,
                             const fat_dirent_t *entry) {
    uint32_t max = fat_dir_max_entries(vol, dir_cluster);
    fat_dirent_t de;

    for (uint32_t i = 0; i < max; i++) {
        int r = fat_read_dirent(vol, dir_cluster, i, &de);
        if (r != FAT_OK) return r;

        if (de.name[0] == 0x00 || de.name[0] == 0xE5) {
            return fat_write_dirent(vol, dir_cluster, i, entry);
        }
    }

    /* For subdirectories, allocate a new cluster */
    if (dir_cluster != 0) {
        uint16_t new_cluster = fat_alloc_cluster(vol);
        if (new_cluster == 0) return FAT_ERR_NO_SPACE;

        /* Append to chain */
        uint16_t c = dir_cluster;
        while (!fat_is_eoc(vol, fat_get_entry(vol, c))) {
            c = fat_get_entry(vol, c);
        }
        fat_set_entry(vol, c, new_cluster);

        /* Zero the new cluster */
        uint8_t zero[FAT_SECTOR_SIZE];
        memset(zero, 0, sizeof(zero));
        uint32_t first = cluster_to_sector(vol, new_cluster);
        for (uint32_t s = 0; s < vol->bpb.sectors_per_cluster; s++) {
            fat_write_sector_i(vol, first + s, zero);
        }

        /* Write entry at first slot of new cluster */
        return fat_write_dirent(vol, dir_cluster,
                                max, /* first entry of new cluster */
                                entry);
    }

    return FAT_ERR_NO_SPACE;
}

/* =========================
 * Path resolution
 * ========================= */

/* Walk path components, return parent dir cluster and final component name */
static int fat_resolve_path(fat_vol_t *vol, const char *path,
                            uint16_t *parent_cluster, char *basename,
                            fat_dirent_t *found_entry) {
    if (!path || path[0] != '/') return FAT_ERR_INVALID;

    uint16_t current = 0; /* root */
    const char *p = path + 1; /* skip leading '/' */

    if (*p == '\0') {
        /* Root directory itself */
        if (parent_cluster) *parent_cluster = 0;
        if (basename) basename[0] = '\0';
        return FAT_OK;
    }

    char component[13];
    while (*p) {
        /* Extract next path component */
        int i = 0;
        while (*p && *p != '/' && i < 12) {
            component[i++] = *p++;
        }
        component[i] = '\0';

        /* Skip trailing slash */
        bool more = (*p == '/');
        if (more) p++;

        /* Skip empty components */
        if (i == 0) continue;

        if (!more && !*p) {
            /* This is the final component (basename) */
            if (parent_cluster) *parent_cluster = current;
            if (basename) {
                strncpy(basename, component, 12);
                basename[12] = '\0';
            }

            /* Try to find it */
            uint8_t name83[11];
            if (str_to_fat83(component, name83) != FAT_OK) return FAT_ERR_INVALID;
            int r = fat_dir_find(vol, current, name83, NULL, found_entry);
            return r; /* FAT_OK if found, FAT_ERR_NOT_FOUND if not */
        }

        /* Intermediate component: must be a directory */
        uint8_t name83[11];
        if (str_to_fat83(component, name83) != FAT_OK) return FAT_ERR_INVALID;

        fat_dirent_t de;
        int r = fat_dir_find(vol, current, name83, NULL, &de);
        if (r != FAT_OK) return r;
        if (!(de.attr & FAT_ATTR_DIRECTORY)) return FAT_ERR_NOT_DIR;

        current = de.first_cluster_lo;
    }

    return FAT_ERR_INVALID;
}

/* =========================
 * Public API
 * ========================= */

void fat_set_backend(fat_vol_t *vol, void *ctx,
                     fat_read_sector_fn rfn, fat_write_sector_fn wfn) {
    if (!vol) return;
    memset(vol, 0, sizeof(*vol));
    vol->storage_ctx = ctx;
    vol->read_sector = rfn;
    vol->write_sector = wfn;
}

int fat_format(fat_vol_t *vol, uint32_t total_sectors, fat_type_t type,
               const char *label) {
    if (!vol || total_sectors < 16) return FAT_ERR_INVALID;
    if (type != FAT_TYPE_12 && type != FAT_TYPE_16) return FAT_ERR_INVALID;

    /* Compute geometry */
    uint8_t sectors_per_cluster;
    uint16_t root_entries;
    uint16_t reserved_sectors = 1;

    if (type == FAT_TYPE_12) {
        sectors_per_cluster = 1;
        root_entries = 64; /* 2 sectors for root dir */
    } else {
        /* FAT16 */
        if (total_sectors <= 512)
            sectors_per_cluster = 1;
        else if (total_sectors <= 2048)
            sectors_per_cluster = 2;
        else if (total_sectors <= 8192)
            sectors_per_cluster = 4;
        else
            sectors_per_cluster = 8;
        root_entries = 128; /* 8 sectors for root dir */
    }

    uint32_t root_dir_sectors = ((uint32_t)root_entries * 32 + FAT_SECTOR_SIZE - 1) / FAT_SECTOR_SIZE;
    uint32_t data_sectors_approx = total_sectors - reserved_sectors - root_dir_sectors;
    uint32_t total_clusters = data_sectors_approx / sectors_per_cluster;

    /* Compute FAT size */
    uint16_t fat_size;
    if (type == FAT_TYPE_12) {
        /* Each FAT12 entry is 1.5 bytes */
        fat_size = (uint16_t)(((total_clusters + 2) * 3 + 1) / 2 + FAT_SECTOR_SIZE - 1) / FAT_SECTOR_SIZE;
    } else {
        /* Each FAT16 entry is 2 bytes */
        fat_size = (uint16_t)(((total_clusters + 2) * 2 + FAT_SECTOR_SIZE - 1) / FAT_SECTOR_SIZE);
    }

    /* Recompute with actual FAT size (2 copies) */
    uint32_t overhead = reserved_sectors + 2 * (uint32_t)fat_size + root_dir_sectors;
    if (overhead >= total_sectors) return FAT_ERR_NO_SPACE;
    total_clusters = (total_sectors - overhead) / sectors_per_cluster;

    /* Build BPB */
    fat_bpb_t bpb;
    memset(&bpb, 0, sizeof(bpb));
    bpb.jmp_boot[0] = 0xEB; bpb.jmp_boot[1] = 0x3C; bpb.jmp_boot[2] = 0x90;
    memcpy(bpb.oem_name, "LITTLEOS", 8);
    bpb.bytes_per_sector = FAT_SECTOR_SIZE;
    bpb.sectors_per_cluster = sectors_per_cluster;
    bpb.reserved_sectors = reserved_sectors;
    bpb.num_fats = 2;
    bpb.root_entry_count = root_entries;
    if (total_sectors <= 0xFFFF)
        bpb.total_sectors_16 = (uint16_t)total_sectors;
    else
        bpb.total_sectors_32 = total_sectors;
    bpb.media_type = 0xF8; /* hard disk */
    bpb.fat_size_16 = fat_size;
    bpb.sectors_per_track = 1;
    bpb.num_heads = 1;
    bpb.drive_number = 0x80;
    bpb.boot_sig = 0x29;
    bpb.volume_serial = 0x4C4F5300; /* "LOS\0" */

    /* Volume label */
    memset(bpb.volume_label, ' ', 11);
    if (label) {
        size_t len = strlen(label);
        if (len > 11) len = 11;
        for (size_t i = 0; i < len; i++)
            bpb.volume_label[i] = (uint8_t)toupper((unsigned char)label[i]);
    } else {
        memcpy(bpb.volume_label, "LITTLEOS   ", 11);
    }

    if (type == FAT_TYPE_12)
        memcpy(bpb.fs_type, "FAT12   ", 8);
    else
        memcpy(bpb.fs_type, "FAT16   ", 8);

    bpb.signature = 0xAA55;

    /* Write BPB */
    int r = fat_write_sector_i(vol, 0, (const uint8_t *)&bpb);
    if (r != FAT_OK) return r;

    /* Zero FAT sectors and root dir */
    uint8_t zero[FAT_SECTOR_SIZE];
    memset(zero, 0, sizeof(zero));

    for (uint32_t s = reserved_sectors; s < reserved_sectors + 2u * fat_size + root_dir_sectors; s++) {
        r = fat_write_sector_i(vol, s, zero);
        if (r != FAT_OK) return r;
    }

    /* Initialize FAT: entries 0 and 1 are reserved */
    vol->bpb = bpb;
    vol->type = type;
    vol->fat_start = reserved_sectors;
    vol->fat_table_entries = total_clusters + 2;

    /* Allocate temp table to write initial FAT */
    vol->fat_table = (uint16_t *)calloc(vol->fat_table_entries, sizeof(uint16_t));
    if (!vol->fat_table) return FAT_ERR_NO_SPACE;

    if (type == FAT_TYPE_12) {
        vol->fat_table[0] = 0x0FF8; /* media type */
        vol->fat_table[1] = 0x0FFF; /* EOC */
    } else {
        vol->fat_table[0] = 0xFFF8;
        vol->fat_table[1] = 0xFFFF;
    }

    vol->dirty = true;
    r = fat_write_table(vol);
    free(vol->fat_table);
    vol->fat_table = NULL;
    vol->mounted = false;

    return r;
}

int fat_mount(fat_vol_t *vol) {
    if (!vol || !vol->read_sector) return FAT_ERR_INVALID;

    /* Read BPB */
    uint8_t sector[FAT_SECTOR_SIZE];
    int r = fat_read_sector_i(vol, 0, sector);
    if (r != FAT_OK) return r;

    memcpy(&vol->bpb, sector, sizeof(vol->bpb));

    /* Validate */
    if (vol->bpb.signature != 0xAA55) return FAT_ERR_CORRUPTED;
    if (vol->bpb.bytes_per_sector != FAT_SECTOR_SIZE) return FAT_ERR_CORRUPTED;
    if (vol->bpb.num_fats == 0 || vol->bpb.num_fats > 2) return FAT_ERR_CORRUPTED;
    if (vol->bpb.sectors_per_cluster == 0) return FAT_ERR_CORRUPTED;

    /* Compute geometry */
    vol->total_sectors = vol->bpb.total_sectors_16 ?
                         vol->bpb.total_sectors_16 : vol->bpb.total_sectors_32;
    vol->fat_start = vol->bpb.reserved_sectors;
    vol->root_dir_start = vol->fat_start +
                          (uint32_t)vol->bpb.num_fats * vol->bpb.fat_size_16;
    vol->root_dir_sectors = ((uint32_t)vol->bpb.root_entry_count * 32 +
                             FAT_SECTOR_SIZE - 1) / FAT_SECTOR_SIZE;
    vol->data_start = vol->root_dir_start + vol->root_dir_sectors;

    uint32_t data_sectors = vol->total_sectors - vol->data_start;
    vol->total_clusters = data_sectors / vol->bpb.sectors_per_cluster;

    /* Determine FAT type */
    if (vol->total_clusters < 4085)
        vol->type = FAT_TYPE_12;
    else
        vol->type = FAT_TYPE_16;

    /* Allocate FAT table */
    vol->fat_table_entries = vol->total_clusters + 2;
    vol->fat_table = (uint16_t *)calloc(vol->fat_table_entries, sizeof(uint16_t));
    if (!vol->fat_table) return FAT_ERR_NO_SPACE;

    /* Read FAT */
    r = fat_read_table(vol);
    if (r != FAT_OK) {
        free(vol->fat_table);
        vol->fat_table = NULL;
        return r;
    }

    vol->mounted = true;
    vol->dirty = false;
    return FAT_OK;
}

int fat_unmount(fat_vol_t *vol) {
    if (!vol) return FAT_ERR_INVALID;

    int r = fat_sync(vol);

    free(vol->fat_table);
    vol->fat_table = NULL;
    vol->mounted = false;
    return r;
}

int fat_sync(fat_vol_t *vol) {
    if (!vol || !vol->mounted) return FAT_ERR_INVALID;
    if (!vol->dirty) return FAT_OK;
    return fat_write_table(vol);
}

/* =========================
 * File operations
 * ========================= */

int fat_open(fat_vol_t *vol, const char *path, fat_file_t *fd, bool create) {
    if (!vol || !vol->mounted || !path || !fd) return FAT_ERR_INVALID;

    memset(fd, 0, sizeof(*fd));

    uint16_t parent = 0;
    char basename[13] = {0};
    fat_dirent_t found;

    int r = fat_resolve_path(vol, path, &parent, basename, &found);

    if (r == FAT_OK) {
        /* Found existing file/dir */
        fd->first_cluster = found.first_cluster_lo;
        fd->file_size = found.file_size;
        fd->attr = found.attr;
        fd->is_dir = (found.attr & FAT_ATTR_DIRECTORY) != 0;
        fd->dir_cluster = parent;
        fd->position = 0;
        fd->is_open = true;

        /* Find index for updates */
        uint8_t name83[11];
        str_to_fat83(basename, name83);
        uint32_t tmp_idx = 0;
        fat_dir_find(vol, parent, name83, &tmp_idx, NULL);
        fd->dir_entry_index = (uint16_t)tmp_idx;

        return FAT_OK;
    }

    if (r == FAT_ERR_NOT_FOUND && create && basename[0]) {
        /* Create new file */
        uint8_t name83[11];
        if (str_to_fat83(basename, name83) != FAT_OK) return FAT_ERR_INVALID;

        fat_dirent_t new_entry;
        memset(&new_entry, 0, sizeof(new_entry));
        memcpy(new_entry.name, name83, 11);
        new_entry.attr = FAT_ATTR_ARCHIVE;
        new_entry.file_size = 0;
        new_entry.first_cluster_lo = 0;

        r = fat_dir_add_entry(vol, parent, &new_entry);
        if (r != FAT_OK) return r;

        fd->first_cluster = 0;
        fd->file_size = 0;
        fd->attr = FAT_ATTR_ARCHIVE;
        fd->dir_cluster = parent;
        fd->position = 0;
        fd->is_open = true;
        fd->is_dir = false;

        /* Find the entry we just wrote */
        uint32_t tmp_idx2 = 0;
        fat_dir_find(vol, parent, name83, &tmp_idx2, NULL);
        fd->dir_entry_index = (uint16_t)tmp_idx2;

        return FAT_OK;
    }

    return r;
}

int fat_close(fat_vol_t *vol, fat_file_t *fd) {
    if (!fd || !fd->is_open) return FAT_ERR_INVALID;
    fd->is_open = false;
    return FAT_OK;
}

int fat_read(fat_vol_t *vol, fat_file_t *fd, uint8_t *buf, uint32_t count) {
    if (!vol || !vol->mounted || !fd || !fd->is_open || !buf) return FAT_ERR_INVALID;
    if (fd->position >= fd->file_size) return 0;

    if (fd->position + count > fd->file_size)
        count = fd->file_size - fd->position;

    uint32_t bytes_read = 0;
    uint32_t bytes_per_cluster = (uint32_t)vol->bpb.sectors_per_cluster * FAT_SECTOR_SIZE;
    uint8_t sector[FAT_SECTOR_SIZE];

    /* Find starting cluster */
    uint32_t cluster_offset = fd->position / bytes_per_cluster;
    uint16_t c = fd->first_cluster;
    for (uint32_t i = 0; i < cluster_offset && c >= 2 && !fat_is_eoc(vol, c); i++) {
        c = fat_get_entry(vol, c);
    }

    while (bytes_read < count && c >= 2 && !fat_is_eoc(vol, c)) {
        uint32_t pos_in_cluster = (fd->position + bytes_read) % bytes_per_cluster;
        uint32_t sector_in_cluster = pos_in_cluster / FAT_SECTOR_SIZE;
        uint32_t offset_in_sector = pos_in_cluster % FAT_SECTOR_SIZE;

        uint32_t abs_sector = cluster_to_sector(vol, c) + sector_in_cluster;
        int r = fat_read_sector_i(vol, abs_sector, sector);
        if (r != FAT_OK) return r;

        uint32_t to_copy = FAT_SECTOR_SIZE - offset_in_sector;
        if (to_copy > count - bytes_read) to_copy = count - bytes_read;

        memcpy(buf + bytes_read, sector + offset_in_sector, to_copy);
        bytes_read += to_copy;

        /* Move to next cluster if needed */
        if ((fd->position + bytes_read) % bytes_per_cluster == 0) {
            c = fat_get_entry(vol, c);
        }
    }

    fd->position += bytes_read;
    return (int)bytes_read;
}

int fat_write(fat_vol_t *vol, fat_file_t *fd, const uint8_t *buf, uint32_t count) {
    if (!vol || !vol->mounted || !fd || !fd->is_open || !buf) return FAT_ERR_INVALID;

    uint32_t bytes_written = 0;
    uint32_t bytes_per_cluster = (uint32_t)vol->bpb.sectors_per_cluster * FAT_SECTOR_SIZE;
    uint8_t sector[FAT_SECTOR_SIZE];

    /* Allocate first cluster if file is empty */
    if (fd->first_cluster == 0 && count > 0) {
        uint16_t c = fat_alloc_cluster(vol);
        if (c == 0) return FAT_ERR_NO_SPACE;
        fd->first_cluster = c;

        /* Zero the cluster */
        memset(sector, 0, sizeof(sector));
        uint32_t first = cluster_to_sector(vol, c);
        for (uint32_t s = 0; s < vol->bpb.sectors_per_cluster; s++) {
            fat_write_sector_i(vol, first + s, sector);
        }
    }

    /* Find cluster for current position */
    uint32_t cluster_offset = fd->position / bytes_per_cluster;
    uint16_t c = fd->first_cluster;
    for (uint32_t i = 0; i < cluster_offset && c >= 2 && !fat_is_eoc(vol, c); i++) {
        c = fat_get_entry(vol, c);
    }

    while (bytes_written < count) {
        if (c < 2 || fat_is_eoc(vol, c)) {
            /* Need a new cluster */
            uint16_t new_c = fat_alloc_cluster(vol);
            if (new_c == 0) break; /* out of space */

            /* Zero new cluster */
            memset(sector, 0, sizeof(sector));
            uint32_t first = cluster_to_sector(vol, new_c);
            for (uint32_t s = 0; s < vol->bpb.sectors_per_cluster; s++) {
                fat_write_sector_i(vol, first + s, sector);
            }

            if (c >= 2) {
                /* Link previous cluster to new one */
                fat_set_entry(vol, c, new_c);
            }
            c = new_c;
        }

        uint32_t pos_in_cluster = (fd->position + bytes_written) % bytes_per_cluster;
        uint32_t sector_in_cluster = pos_in_cluster / FAT_SECTOR_SIZE;
        uint32_t offset_in_sector = pos_in_cluster % FAT_SECTOR_SIZE;

        uint32_t abs_sector = cluster_to_sector(vol, c) + sector_in_cluster;

        /* Read-modify-write if partial sector */
        if (offset_in_sector != 0 || (count - bytes_written) < FAT_SECTOR_SIZE) {
            int r = fat_read_sector_i(vol, abs_sector, sector);
            if (r != FAT_OK) return r;
        }

        uint32_t to_copy = FAT_SECTOR_SIZE - offset_in_sector;
        if (to_copy > count - bytes_written) to_copy = count - bytes_written;

        memcpy(sector + offset_in_sector, buf + bytes_written, to_copy);
        int r = fat_write_sector_i(vol, abs_sector, sector);
        if (r != FAT_OK) return r;

        bytes_written += to_copy;

        /* Move to next cluster if needed */
        if ((fd->position + bytes_written) % bytes_per_cluster == 0) {
            uint16_t next = fat_get_entry(vol, c);
            if (fat_is_eoc(vol, next) && bytes_written < count) {
                /* Will allocate on next iteration */
            }
            c = next;
        }
    }

    fd->position += bytes_written;
    if (fd->position > fd->file_size) {
        fd->file_size = fd->position;
    }

    /* Update directory entry with new size */
    fat_dirent_t de;
    int r = fat_read_dirent(vol, fd->dir_cluster, fd->dir_entry_index, &de);
    if (r == FAT_OK) {
        de.file_size = fd->file_size;
        de.first_cluster_lo = fd->first_cluster;
        fat_write_dirent(vol, fd->dir_cluster, fd->dir_entry_index, &de);
    }

    return (int)bytes_written;
}

/* =========================
 * Directory operations
 * ========================= */

int fat_mkdir(fat_vol_t *vol, const char *path) {
    if (!vol || !vol->mounted || !path) return FAT_ERR_INVALID;

    uint16_t parent = 0;
    char basename[13] = {0};
    fat_dirent_t found;

    int r = fat_resolve_path(vol, path, &parent, basename, &found);
    if (r == FAT_OK) return FAT_ERR_EXISTS;
    if (r != FAT_ERR_NOT_FOUND) return r;
    if (basename[0] == '\0') return FAT_ERR_INVALID;

    uint8_t name83[11];
    if (str_to_fat83(basename, name83) != FAT_OK) return FAT_ERR_INVALID;

    /* Allocate cluster for directory */
    uint16_t dir_cluster = fat_alloc_cluster(vol);
    if (dir_cluster == 0) return FAT_ERR_NO_SPACE;

    /* Zero the cluster */
    uint8_t zero[FAT_SECTOR_SIZE];
    memset(zero, 0, sizeof(zero));
    uint32_t first = cluster_to_sector(vol, dir_cluster);
    for (uint32_t s = 0; s < vol->bpb.sectors_per_cluster; s++) {
        fat_write_sector_i(vol, first + s, zero);
    }

    /* Create . and .. entries */
    fat_dirent_t dot;
    memset(&dot, 0, sizeof(dot));
    memset(dot.name, ' ', 11);
    dot.name[0] = '.';
    dot.attr = FAT_ATTR_DIRECTORY;
    dot.first_cluster_lo = dir_cluster;
    fat_write_dirent(vol, dir_cluster, 0, &dot);

    fat_dirent_t dotdot;
    memset(&dotdot, 0, sizeof(dotdot));
    memset(dotdot.name, ' ', 11);
    dotdot.name[0] = '.'; dotdot.name[1] = '.';
    dotdot.attr = FAT_ATTR_DIRECTORY;
    dotdot.first_cluster_lo = parent;
    fat_write_dirent(vol, dir_cluster, 1, &dotdot);

    /* Add entry in parent directory */
    fat_dirent_t new_entry;
    memset(&new_entry, 0, sizeof(new_entry));
    memcpy(new_entry.name, name83, 11);
    new_entry.attr = FAT_ATTR_DIRECTORY;
    new_entry.first_cluster_lo = dir_cluster;
    new_entry.file_size = 0;

    return fat_dir_add_entry(vol, parent, &new_entry);
}

int fat_opendir(fat_vol_t *vol, const char *path, fat_file_t *fd) {
    if (!vol || !vol->mounted || !path || !fd) return FAT_ERR_INVALID;

    memset(fd, 0, sizeof(*fd));

    if (path[0] == '/' && path[1] == '\0') {
        /* Root directory */
        fd->first_cluster = 0;
        fd->dir_cluster = 0;
        fd->is_dir = true;
        fd->is_open = true;
        fd->position = 0;
        return FAT_OK;
    }

    uint16_t parent = 0;
    char basename[13] = {0};
    fat_dirent_t found;

    int r = fat_resolve_path(vol, path, &parent, basename, &found);
    if (r != FAT_OK) return r;
    if (!(found.attr & FAT_ATTR_DIRECTORY)) return FAT_ERR_NOT_DIR;

    fd->first_cluster = found.first_cluster_lo;
    fd->dir_cluster = parent;
    fd->is_dir = true;
    fd->is_open = true;
    fd->position = 0;
    return FAT_OK;
}

int fat_readdir(fat_vol_t *vol, fat_file_t *fd, fat_dir_entry_t *entry) {
    if (!vol || !fd || !fd->is_open || !fd->is_dir || !entry) return FAT_ERR_INVALID;

    uint16_t dir_cluster = fd->first_cluster;
    uint32_t max = fat_dir_max_entries(vol, dir_cluster);

    while (fd->position < max) {
        fat_dirent_t de;
        int r = fat_read_dirent(vol, dir_cluster, fd->position, &de);
        fd->position++;

        if (r != FAT_OK) return r;
        if (de.name[0] == 0x00) return FAT_ERR_NOT_FOUND; /* end */
        if (de.name[0] == 0xE5) continue; /* deleted */
        if (de.attr == 0x0F) continue;    /* LFN */

        fat83_to_str(de.name, entry->name);
        entry->attr = de.attr;
        entry->file_size = de.file_size;
        entry->first_cluster = de.first_cluster_lo;
        return FAT_OK;
    }

    return FAT_ERR_NOT_FOUND;
}

int fat_unlink(fat_vol_t *vol, const char *path) {
    if (!vol || !vol->mounted || !path) return FAT_ERR_INVALID;

    uint16_t parent = 0;
    char basename[13] = {0};
    fat_dirent_t found;

    int r = fat_resolve_path(vol, path, &parent, basename, &found);
    if (r != FAT_OK) return r;

    /* Don't delete directories with this (use rmdir) */
    if (found.attr & FAT_ATTR_DIRECTORY) return FAT_ERR_INVALID;

    /* Free cluster chain */
    if (found.first_cluster_lo >= 2) {
        fat_free_chain(vol, found.first_cluster_lo);
    }

    /* Mark directory entry as deleted */
    uint8_t name83[11];
    str_to_fat83(basename, name83);
    uint32_t idx;
    fat_dirent_t de;
    r = fat_dir_find(vol, parent, name83, &idx, &de);
    if (r != FAT_OK) return r;

    de.name[0] = 0xE5; /* deleted marker */
    return fat_write_dirent(vol, parent, idx, &de);
}

/* =========================
 * Info
 * ========================= */

int fat_stat(fat_vol_t *vol, uint32_t *total_bytes, uint32_t *free_bytes) {
    if (!vol || !vol->mounted) return FAT_ERR_INVALID;

    uint32_t bytes_per_cluster = (uint32_t)vol->bpb.sectors_per_cluster * FAT_SECTOR_SIZE;
    uint32_t free_clusters = 0;

    for (uint16_t c = 2; c < vol->fat_table_entries; c++) {
        if (vol->fat_table[c] == fat_free_marker())
            free_clusters++;
    }

    if (total_bytes) *total_bytes = vol->total_clusters * bytes_per_cluster;
    if (free_bytes)  *free_bytes  = free_clusters * bytes_per_cluster;
    return FAT_OK;
}

const char *fat_type_str(fat_type_t type) {
    switch (type) {
    case FAT_TYPE_12: return "FAT12";
    case FAT_TYPE_16: return "FAT16";
    default: return "unknown";
    }
}
