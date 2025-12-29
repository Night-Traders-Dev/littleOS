/* fs_inode.c - inode I/O and block mapping */
#include "fs.h"

#include <string.h>

/* internal block I/O from fs_core.c */
static int fs_read_block_i(struct fs *fs, uint32_t block, uint8_t *buf);
static int fs_write_block_i(struct fs *fs, uint32_t block, const uint8_t *buf);
static uint32_t fs_find_first_free_data_block(struct fs *fs);
static int fs_mark_block_valid(struct fs *fs, uint32_t block_addr);

/* load inode by ino via NAT */
int fs_load_inode(struct fs *fs, uint32_t ino, struct fs_inode *out) {
    if (!fs || !out) return FS_ERR_INVALID_ARG;
    if (ino >= fs->sb.total_inodes || ino == 0) return FS_ERR_INVALID_INODE;

    uint32_t blk = fs->nat[ino].block_addr;
    if (blk == FS_INVALID_BLOCK) return FS_ERR_INVALID_INODE;

    uint8_t buf[FS_BLOCK_SIZE];
    int r = fs_read_block_i(fs, blk, buf);
    if (r != FS_OK) return r;

    memcpy(out, buf, sizeof(*out));
    if (out->inode_num != ino) return FS_ERR_CORRUPTED;
    return FS_OK;
}

/* write inode to fresh block, update NAT (log structured) */
int fs_store_inode(struct fs *fs, const struct fs_inode *in) {
    if (!fs || !in) return FS_ERR_INVALID_ARG;
    uint32_t ino = in->inode_num;
    if (ino >= fs->sb.total_inodes || ino == 0) return FS_ERR_INVALID_INODE;

    uint32_t blk = fs_find_first_free_data_block(fs);
    if (blk == FS_INVALID_BLOCK) return FS_ERR_NO_SPACE;

    uint8_t buf[FS_BLOCK_SIZE];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, in, sizeof(*in));

    int r = fs_write_block_i(fs, blk, buf);
    if (r != FS_OK) return r;

    fs_mark_block_valid(fs, blk);
    fs->free_blocks_count--;

    fs->nat[ino].block_addr = blk;
    fs->nat[ino].version++;
    fs->nat[ino].type = 1;
    fs->nat_dirty = true;

    return FS_OK;
}

/* Direct-only block mapping for now */
int fs_bmap(struct fs *fs,
            struct fs_inode *ino,
            uint32_t logical_block,
            bool create,
            uint32_t *phys_block) {
    if (!fs || !ino || !phys_block) return FS_ERR_INVALID_ARG;

    /* only direct blocks 0..FS_DIRECT_BLOCKS-1 for now */
    if (logical_block >= FS_DIRECT_BLOCKS) return FS_ERR_UNSUPPORTED;

    uint32_t blk = ino->direct[logical_block];
    if (blk != FS_INVALID_BLOCK && blk != 0) {
        *phys_block = blk;
        return FS_OK;
    }

    if (!create) {
        *phys_block = FS_INVALID_BLOCK;
        return FS_OK;
    }

    /* allocate a new data block */
    blk = fs_find_first_free_data_block(fs);
    if (blk == FS_INVALID_BLOCK) return FS_ERR_NO_SPACE;

    ino->direct[logical_block] = blk;
    fs_mark_block_valid(fs, blk);
    fs->free_blocks_count--;

    *phys_block = blk;
    return FS_OK;
}
