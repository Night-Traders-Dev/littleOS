/* fs_core.c - littleOS F2FS-inspired filesystem (core metadata/lifecycle) */

#include "fs.h"

#include <string.h>
#include <stdlib.h>
#include <time.h>

/* =========================
 * CRC32
 * ========================= */
static uint32_t fs_crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            uint32_t mask = (uint32_t)-(int)(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

static uint32_t fs_time_now_seconds(void) {
    /* On embedded targets, replace with your RTC/timebase. */
    return (uint32_t)time(NULL);
}

/* =========================
 * Backend wrappers
 * ========================= */
void fs_set_storage_backend(struct fs *fs,
                            void *ctx,
                            fs_read_block_fn rfn,
                            fs_write_block_fn wfn,
                            fs_erase_sector_fn efn) {
    if (!fs) return;
    fs->storage_ctx = ctx;
    fs->read_block  = rfn;
    fs->write_block = wfn;
    fs->erase_sector = efn;
}

int fs_read_block_i(struct fs *fs, uint32_t block, uint8_t *buf) {
    if (!fs || !fs->read_block || !buf) return FS_ERR_INVALID_ARG;
    return fs->read_block(fs->storage_ctx, block, buf);
}

int fs_write_block_i(struct fs *fs, uint32_t block, const uint8_t *buf) {
    if (!fs || !fs->write_block || !buf) return FS_ERR_INVALID_ARG;
    return fs->write_block(fs->storage_ctx, block, buf);
}

/* =========================
 * NAT/SIT I/O
 * ========================= */
static int fs_write_nat(struct fs *fs) {
    if (!fs || !fs->nat) return FS_ERR_INVALID_ARG;
    if (!fs->write_block) return FS_OK; /* allow RAM-only use */

    const uint32_t entries_per_block =
        FS_BLOCK_SIZE / (uint32_t)sizeof(struct fs_nat_entry);
    uint8_t blk[FS_BLOCK_SIZE];

    uint32_t idx = 0;
    for (uint32_t b = 0; b < fs->sb.nat_blocks; b++) {
        memset(blk, 0, sizeof(blk));
        struct fs_nat_entry *out = (struct fs_nat_entry *)blk;

        for (uint32_t i = 0; i < entries_per_block; i++) {
            if (idx < fs->sb.total_inodes) {
                out[i] = fs->nat[idx];
            } else {
                out[i].block_addr = FS_INVALID_BLOCK;
                out[i].version    = 0;
                out[i].type       = 0;
                out[i]._pad       = 0;
            }
            idx++;
        }

        int r = fs_write_block_i(fs, fs->sb.nat_start_block + b, blk);
        if (r != FS_OK) return r;
    }

    fs->nat_dirty = false;
    return FS_OK;
}

static int fs_read_nat(struct fs *fs) {
    if (!fs || !fs->nat) return FS_ERR_INVALID_ARG;
    if (!fs->read_block) return FS_OK;

    const uint32_t entries_per_block =
        FS_BLOCK_SIZE / (uint32_t)sizeof(struct fs_nat_entry);
    uint8_t blk[FS_BLOCK_SIZE];

    uint32_t idx = 0;
    for (uint32_t b = 0; b < fs->sb.nat_blocks; b++) {
        int r = fs_read_block_i(fs, fs->sb.nat_start_block + b, blk);
        if (r != FS_OK) return r;

        const struct fs_nat_entry *in = (const struct fs_nat_entry *)blk;
        for (uint32_t i = 0; i < entries_per_block && idx < fs->sb.total_inodes; i++, idx++) {
            fs->nat[idx] = in[i];
        }
    }

    fs->nat_dirty = false;
    return FS_OK;
}

static int fs_write_sit(struct fs *fs) {
    if (!fs || !fs->sit) return FS_ERR_INVALID_ARG;
    if (!fs->write_block) return FS_OK;

    const uint32_t entries_per_block =
        FS_BLOCK_SIZE / (uint32_t)sizeof(struct fs_sit_entry);
    uint8_t blk[FS_BLOCK_SIZE];

    uint32_t idx = 0;
    for (uint32_t b = 0; b < fs->sb.sit_blocks; b++) {
        memset(blk, 0, sizeof(blk));
        struct fs_sit_entry *out = (struct fs_sit_entry *)blk;

        for (uint32_t i = 0; i < entries_per_block; i++) {
            if (idx < fs->sb.total_segments) {
                out[i] = fs->sit[idx];
            } else {
                out[i].valid_count = 0;
                out[i].flags       = 0;
                out[i].age         = 0;
            }
            idx++;
        }

        int r = fs_write_block_i(fs, fs->sb.sit_start_block + b, blk);
        if (r != FS_OK) return r;
    }

    fs->sit_dirty = false;
    return FS_OK;
}

static int fs_read_sit(struct fs *fs) {
    if (!fs || !fs->sit) return FS_ERR_INVALID_ARG;
    if (!fs->read_block) return FS_OK;

    const uint32_t entries_per_block =
        FS_BLOCK_SIZE / (uint32_t)sizeof(struct fs_sit_entry);
    uint8_t blk[FS_BLOCK_SIZE];

    uint32_t idx = 0;
    for (uint32_t b = 0; b < fs->sb.sit_blocks; b++) {
        int r = fs_read_block_i(fs, fs->sb.sit_start_block + b, blk);
        if (r != FS_OK) return r;

        const struct fs_sit_entry *in = (const struct fs_sit_entry *)blk;
        for (uint32_t i = 0; i < entries_per_block && idx < fs->sb.total_segments; i++, idx++) {
            fs->sit[idx] = in[i];
        }
    }

    fs->sit_dirty = false;
    return FS_OK;
}

/* =========================
 * Simple allocator helpers
 * ========================= */
int fs_mark_block_valid(struct fs *fs, uint32_t block_addr) {
    if (!fs) return FS_ERR_INVALID_ARG;
    if (block_addr >= fs->sb.total_blocks) return FS_ERR_INVALID_BLOCK;

    uint32_t seg = block_addr / FS_BLOCKS_PER_SEGMENT;
    if (seg >= fs->sb.total_segments) return FS_ERR_INVALID_BLOCK;

    if (fs->sit[seg].valid_count < FS_BLOCKS_PER_SEGMENT) {
        fs->sit[seg].valid_count++;
        fs->sit_dirty = true;
        return FS_OK;
    }

    return FS_ERR_CORRUPTED;
}

uint32_t fs_find_first_free_data_block(struct fs *fs) {
    if (!fs) return FS_INVALID_BLOCK;

    for (uint32_t seg = (fs->sb.main_start_block / FS_BLOCKS_PER_SEGMENT);
         seg < fs->sb.total_segments;
         seg++) {
        if (fs->sit[seg].valid_count < FS_BLOCKS_PER_SEGMENT) {
            uint32_t off = fs->sit[seg].valid_count;
            uint32_t blk = seg * FS_BLOCKS_PER_SEGMENT + off;
            if (blk >= fs->sb.main_start_block && blk < fs->sb.total_blocks) {
                return blk;
            }
        }
    }

    return FS_INVALID_BLOCK;
}

/* =========================
 * Superblock / checkpoint I/O
 * ========================= */
static void fs_finalize_superblock_crc(struct fs_superblock *sb) {
    sb->sb_crc32 = 0;
    sb->sb_crc32 = fs_crc32((const uint8_t *)sb, sizeof(*sb));
}

static void fs_finalize_checkpoint_crc(struct fs_checkpoint *cp) {
    cp->cp_crc32 = 0;
    cp->cp_crc32 = fs_crc32((const uint8_t *)cp, sizeof(*cp));
}

static int fs_write_superblock(struct fs *fs) {
    if (!fs) return FS_ERR_INVALID_ARG;
    if (!fs->write_block) return FS_OK;

    uint8_t blk[FS_BLOCK_SIZE];
    fs_finalize_superblock_crc(&fs->sb);
    memcpy(blk, &fs->sb, sizeof(fs->sb));

    int r = fs_write_block_i(fs, FS_SB_BLOCK, blk);
    if (r == FS_OK) fs->sb_dirty = false;
    return r;
}

static int fs_read_superblock(struct fs *fs) {
    if (!fs) return FS_ERR_INVALID_ARG;
    if (!fs->read_block) return FS_ERR_IO;

    uint8_t blk[FS_BLOCK_SIZE];
    int r = fs_read_block_i(fs, FS_SB_BLOCK, blk);
    if (r != FS_OK) return r;

    memcpy(&fs->sb, blk, sizeof(fs->sb));
    if (fs->sb.magic != FS_MAGIC) return FS_ERR_CORRUPTED;
    if (fs->sb.version != FS_VERSION) return FS_ERR_CORRUPTED;
    if (fs->sb.block_size != FS_BLOCK_SIZE) return FS_ERR_CORRUPTED;
    if (fs->sb.segment_size != FS_SEGMENT_SIZE) return FS_ERR_CORRUPTED;

    uint32_t crc = fs->sb.sb_crc32;
    struct fs_superblock tmp = fs->sb;
    tmp.sb_crc32 = 0;
    uint32_t calc = fs_crc32((const uint8_t *)&tmp, sizeof(tmp));
    if (crc != calc) return FS_ERR_CORRUPTED;

    return FS_OK;
}

static int fs_write_checkpoint_block(struct fs *fs, uint32_t which /*0 or 1*/) {
    if (!fs) return FS_ERR_INVALID_ARG;
    if (!fs->write_block) return FS_OK;

    uint8_t blk[FS_BLOCK_SIZE];

    if (which == 0) {
        fs_finalize_checkpoint_crc(&fs->cp0);
        memcpy(blk, &fs->cp0, sizeof(fs->cp0));
        return fs_write_block_i(fs, FS_CP0_BLOCK, blk);
    } else {
        fs_finalize_checkpoint_crc(&fs->cp1);
        memcpy(blk, &fs->cp1, sizeof(fs->cp1));
        return fs_write_block_i(fs, FS_CP1_BLOCK, blk);
    }
}

static int fs_read_checkpoint_block(struct fs *fs, uint32_t which, struct fs_checkpoint *out) {
    if (!fs || !out) return FS_ERR_INVALID_ARG;
    if (!fs->read_block) return FS_ERR_IO;

    uint8_t blk[FS_BLOCK_SIZE];
    uint32_t addr = (which == 0) ? FS_CP0_BLOCK : FS_CP1_BLOCK;

    int r = fs_read_block_i(fs, addr, blk);
    if (r != FS_OK) return r;

    memcpy(out, blk, sizeof(*out));
    uint32_t crc = out->cp_crc32;
    struct fs_checkpoint tmp = *out;
    tmp.cp_crc32 = 0;
    uint32_t calc = fs_crc32((const uint8_t *)&tmp, sizeof(tmp));
    if (crc != calc) return FS_ERR_CORRUPTED;

    return FS_OK;
}

/* =========================
 * Public API - lifecycle
 * ========================= */
int fs_format(struct fs *fs, uint32_t total_blocks) {
    if (!fs) return FS_ERR_INVALID_ARG;
    if (total_blocks < FS_FIXED_METADATA_BLOCKS + 8u) return FS_ERR_INVALID_ARG;

    /* preserve backend */
    void *ctx              = fs->storage_ctx;
    fs_read_block_fn  rfn  = fs->read_block;
    fs_write_block_fn wfn  = fs->write_block;
    fs_erase_sector_fn efn = fs->erase_sector;

    memset(fs, 0, sizeof(*fs));
    fs->storage_ctx = ctx;
    fs->read_block  = rfn;
    fs->write_block = wfn;
    fs->erase_sector = efn;

    /* build SB */
    fs->sb.magic          = FS_MAGIC;
    fs->sb.version        = FS_VERSION;
    fs->sb.block_size     = FS_BLOCK_SIZE;
    fs->sb.segment_size   = (uint16_t)FS_SEGMENT_SIZE;
    fs->sb.total_blocks   = total_blocks;
    fs->sb.total_segments = fs_div_ceil_u32(total_blocks, FS_BLOCKS_PER_SEGMENT);
    fs->sb.total_inodes   = FS_DEFAULT_MAX_INODES;
    fs->sb.root_inode     = FS_ROOT_INODE;

    fs->sb.nat_start_block = FS_FIXED_METADATA_BLOCKS;
    fs->sb.nat_blocks      = fs_nat_blocks_for_inodes(fs->sb.total_inodes);
    fs->sb.sit_start_block = fs->sb.nat_start_block + fs->sb.nat_blocks;
    fs->sb.sit_blocks      = fs_sit_blocks_for_segments(fs->sb.total_segments);
    fs->sb.main_start_block = fs->sb.sit_start_block + fs->sb.sit_blocks;
    if (fs->sb.main_start_block >= fs->sb.total_blocks) {
        return FS_ERR_NO_SPACE;
    }

    fs->sb.creation_time  = fs_time_now_seconds();
    fs->sb.last_sync_time = fs->sb.creation_time;
    fs->sb.mount_count    = 0;
    fs->sb.flags          = 0;

    /* allocate NAT/SIT in RAM */
    fs->nat = (struct fs_nat_entry *)
        calloc(fs->sb.total_inodes, sizeof(struct fs_nat_entry));
    fs->sit = (struct fs_sit_entry *)
        calloc(fs->sb.total_segments, sizeof(struct fs_sit_entry));
    if (!fs->nat || !fs->sit) {
        free(fs->nat); fs->nat = NULL;
        free(fs->sit); fs->sit = NULL;
        return FS_ERR_NO_SPACE;
    }

    for (uint32_t i = 0; i < fs->sb.total_inodes; i++) {
        fs->nat[i].block_addr = FS_INVALID_BLOCK;
        fs->nat[i].version    = 0;
        fs->nat[i].type       = 0;
        fs->nat[i]._pad       = 0;
    }

    /* initial checkpoints */
    memset(&fs->cp0, 0, sizeof(fs->cp0));
    memset(&fs->cp1, 0, sizeof(fs->cp1));
    fs->cp0.checkpoint_num = 1;
    fs->cp1.checkpoint_num = 0;
    fs->cp0.timestamp      = fs->sb.creation_time;
    fs->cp1.timestamp      = fs->sb.creation_time;

    /* Reserve metadata blocks in SIT */
    for (uint32_t b = 0; b < fs->sb.main_start_block; b++) {
        fs_mark_block_valid(fs, b);
    }

    fs->free_blocks_count = fs->sb.total_blocks - fs->sb.main_start_block;
    fs->cp0.free_blocks   = fs->free_blocks_count;
    fs->cp0.next_node_id  = FS_ROOT_INODE + 1u;

    /* Create root inode */
    uint32_t root_blk = fs_find_first_free_data_block(fs);
    if (root_blk == FS_INVALID_BLOCK) return FS_ERR_NO_SPACE;

    struct fs_inode root;
    memset(&root, 0, sizeof(root));
    root.magic         = 0xFA;
    root.inode_version = 1;
    root.mode          = FS_MODE_DIR;
    root.size          = 0;
    root.atime         = fs->sb.creation_time;
    root.mtime         = fs->sb.creation_time;
    root.ctime         = fs->sb.creation_time;
    root.link_count    = 2;
    root.inode_num     = FS_ROOT_INODE;
    root.parent_inode  = FS_ROOT_INODE;
    root.generation    = 1;
    root.inode_crc32   = 0;
    root.inode_crc32   = fs_crc32((const uint8_t *)&root, sizeof(root));

    fs_mark_block_valid(fs, root_blk);
    fs->nat[FS_ROOT_INODE].block_addr = root_blk;
    fs->nat[FS_ROOT_INODE].version    = 1;
    fs->nat[FS_ROOT_INODE].type       = 1; /* inode */
    fs->nat_dirty = true;

    if (fs->write_block) {
        int r = fs_write_block_i(fs, root_blk, (const uint8_t *)&root);
        if (r != FS_OK) return r;
    }

    /* consume one block for root inode */
    fs->free_blocks_count--;
    fs->cp0.free_blocks = fs->free_blocks_count;

    /* Persist metadata */
    fs->sb_dirty  = true;
    fs->cp_dirty  = true;
    fs->sit_dirty = true;

    int r;
    r = fs_write_superblock(fs); if (r != FS_OK) return r;
    r = fs_write_nat(fs);        if (r != FS_OK) return r;
    r = fs_write_sit(fs);        if (r != FS_OK) return r;
    fs_finalize_checkpoint_crc(&fs->cp0);
    r = fs_write_checkpoint_block(fs, 0); if (r != FS_OK) return r;
    fs_finalize_checkpoint_crc(&fs->cp1);
    r = fs_write_checkpoint_block(fs, 1); if (r != FS_OK) return r;

    fs->active_cp = 0;
    fs->sb_dirty = fs->cp_dirty = fs->nat_dirty = fs->sit_dirty = false;
    return FS_OK;
}

int fs_mount(struct fs *fs) {
    if (!fs) return FS_ERR_INVALID_ARG;
    if (!fs->read_block) return FS_ERR_IO;

    int r = fs_read_superblock(fs);
    if (r != FS_OK) return r;

    free(fs->nat); fs->nat = NULL;
    free(fs->sit); fs->sit = NULL;

    fs->nat = (struct fs_nat_entry *)
        calloc(fs->sb.total_inodes, sizeof(struct fs_nat_entry));
    fs->sit = (struct fs_sit_entry *)
        calloc(fs->sb.total_segments, sizeof(struct fs_sit_entry));
    if (!fs->nat || !fs->sit) {
        free(fs->nat); fs->nat = NULL;
        free(fs->sit); fs->sit = NULL;
        return FS_ERR_NO_SPACE;
    }

    struct fs_checkpoint a, b;
    int ra = fs_read_checkpoint_block(fs, 0, &a);
    int rb = fs_read_checkpoint_block(fs, 1, &b);
    if (ra != FS_OK && rb != FS_OK) return FS_ERR_CORRUPTED;

    if (ra == FS_OK && rb == FS_OK) {
        if (a.checkpoint_num >= b.checkpoint_num) {
            fs->cp0 = a; fs->cp1 = b; fs->active_cp = 0;
        } else {
            fs->cp0 = a; fs->cp1 = b; fs->active_cp = 1;
        }
    } else if (ra == FS_OK) {
        fs->cp0 = a; fs->active_cp = 0;
    } else {
        fs->cp1 = b; fs->active_cp = 1;
    }

    r = fs_read_nat(fs); if (r != FS_OK) return r;
    r = fs_read_sit(fs); if (r != FS_OK) return r;

    fs->sb.mount_count++;
    fs->sb_dirty = true;

    fs->free_blocks_count =
        (fs->active_cp == 0 ? fs->cp0.free_blocks : fs->cp1.free_blocks);
    return FS_OK;
}

int fs_sync(struct fs *fs) {
    if (!fs) return FS_ERR_INVALID_ARG;
    int r;

    if (fs->nat_dirty) { r = fs_write_nat(fs); if (r != FS_OK) return r; }
    if (fs->sit_dirty) { r = fs_write_sit(fs); if (r != FS_OK) return r; }

    uint32_t now = fs_time_now_seconds();

    if (fs->active_cp == 0) {
        fs->cp1 = fs->cp0;
        fs->cp1.checkpoint_num = fs->cp0.checkpoint_num + 1u;
        fs->cp1.timestamp      = now;
        fs->cp1.free_blocks    = fs->free_blocks_count;
        fs_finalize_checkpoint_crc(&fs->cp1);
        r = fs_write_checkpoint_block(fs, 1);
        if (r != FS_OK) return r;
        fs->active_cp = 1;
    } else {
        fs->cp0 = fs->cp1;
        fs->cp0.checkpoint_num = fs->cp1.checkpoint_num + 1u;
        fs->cp0.timestamp      = now;
        fs->cp0.free_blocks    = fs->free_blocks_count;
        fs_finalize_checkpoint_crc(&fs->cp0);
        r = fs_write_checkpoint_block(fs, 0);
        if (r != FS_OK) return r;
        fs->active_cp = 0;
    }

    fs->sb.last_sync_time = now;
    fs->sb_dirty = true;
    if (fs->sb_dirty) {
        r = fs_write_superblock(fs);
        if (r != FS_OK) return r;
    }

    fs->cp_dirty = fs->sb_dirty = false;
    return FS_OK;
}

int fs_unmount(struct fs *fs) {
    if (!fs) return FS_ERR_INVALID_ARG;
    int r = fs_sync(fs);
    if (r != FS_OK) return r;

    free(fs->nat); fs->nat = NULL;
    free(fs->sit); fs->sit = NULL;
    return FS_OK;
}

int fs_fsck(struct fs *fs) {
    if (!fs) return FS_ERR_INVALID_ARG;

    if (fs->sb.magic != FS_MAGIC) return FS_ERR_CORRUPTED;
    if (fs->sb.version != FS_VERSION) return FS_ERR_CORRUPTED;
    if (fs->sb.block_size != FS_BLOCK_SIZE) return FS_ERR_CORRUPTED;
    if (fs->sb.segment_size != FS_SEGMENT_SIZE) return FS_ERR_CORRUPTED;
    if (fs->sb.nat_start_block != FS_FIXED_METADATA_BLOCKS) return FS_ERR_CORRUPTED;
    if (fs->sb.main_start_block >= fs->sb.total_blocks) return FS_ERR_CORRUPTED;

    uint32_t nat_end = fs->sb.nat_start_block + fs->sb.nat_blocks;
    uint32_t sit_end = fs->sb.sit_start_block + fs->sb.sit_blocks;
    if (nat_end != fs->sb.sit_start_block) return FS_ERR_CORRUPTED;
    if (sit_end != fs->sb.main_start_block) return FS_ERR_CORRUPTED;
    if (sit_end > fs->sb.total_blocks)      return FS_ERR_CORRUPTED;

    if (fs->nat) {
        if (FS_ROOT_INODE >= fs->sb.total_inodes) return FS_ERR_CORRUPTED;
        if (fs->nat[FS_ROOT_INODE].block_addr == FS_INVALID_BLOCK) return FS_ERR_CORRUPTED;
        if (fs->nat[FS_ROOT_INODE].block_addr < fs->sb.main_start_block)
            return FS_ERR_CORRUPTED;
    }

    return FS_OK;
}
