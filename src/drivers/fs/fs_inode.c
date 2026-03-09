/* fs_inode.c - inode I/O and block mapping */
#include "fs.h"

#include <string.h>



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

/* ------------------------------------------------------------------ */
/*  Helper: allocate and zero a fresh indirect node block              */
/* ------------------------------------------------------------------ */

static int alloc_indirect_node(struct fs *fs, uint32_t *out_blk) {
    uint32_t blk = fs_find_first_free_data_block(fs);
    if (blk == FS_INVALID_BLOCK) return FS_ERR_NO_SPACE;

    /* Fill with 0xFF so every pointer is FS_INVALID_BLOCK */
    uint8_t buf[FS_BLOCK_SIZE];
    memset(buf, 0xFF, sizeof(buf));
    int r = fs_write_block_i(fs, blk, buf);
    if (r != FS_OK) return r;

    fs_mark_block_valid(fs, blk);
    fs->free_blocks_count--;
    *out_blk = blk;
    return FS_OK;
}

/* ------------------------------------------------------------------ */
/*  Helper: read / write an indirect node                              */
/* ------------------------------------------------------------------ */

static int read_indirect_node(struct fs *fs, uint32_t blk,
                              struct fs_indirect_node *node) {
    uint8_t buf[FS_BLOCK_SIZE];
    int r = fs_read_block_i(fs, blk, buf);
    if (r != FS_OK) return r;
    memcpy(node, buf, sizeof(*node));
    return FS_OK;
}

static int write_indirect_node(struct fs *fs, uint32_t blk,
                               const struct fs_indirect_node *node) {
    uint8_t buf[FS_BLOCK_SIZE];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, node, sizeof(*node));
    return fs_write_block_i(fs, blk, buf);
}

/* ------------------------------------------------------------------ */
/*  fs_bmap - map logical block to physical block                      */
/*                                                                     */
/*  Layout:                                                            */
/*    [0 .. 9]            direct blocks                                */
/*    [10 .. 137]         single indirect  (128 ptrs)                  */
/*    [138 .. 16521]      double indirect  (128 * 128 ptrs)            */
/* ------------------------------------------------------------------ */

#define SINGLE_INDIRECT_MAX  (FS_DIRECT_BLOCKS + FS_INDIRECT_PTRS)
#define DOUBLE_INDIRECT_MAX  (SINGLE_INDIRECT_MAX + \
                              (uint32_t)FS_INDIRECT_PTRS * FS_INDIRECT_PTRS)

int fs_bmap(struct fs *fs,
            struct fs_inode *ino,
            uint32_t logical_block,
            bool create,
            uint32_t *phys_block) {
    if (!fs || !ino || !phys_block) return FS_ERR_INVALID_ARG;

    /* -------------------------------------------------------------- */
    /*  Direct blocks: 0 .. FS_DIRECT_BLOCKS-1                        */
    /* -------------------------------------------------------------- */
    if (logical_block < FS_DIRECT_BLOCKS) {
        uint32_t blk = ino->direct[logical_block];
        if (blk != FS_INVALID_BLOCK && blk != 0) {
            *phys_block = blk;
            return FS_OK;
        }

        if (!create) {
            *phys_block = FS_INVALID_BLOCK;
            return FS_OK;
        }

        blk = fs_find_first_free_data_block(fs);
        if (blk == FS_INVALID_BLOCK) return FS_ERR_NO_SPACE;

        ino->direct[logical_block] = blk;
        fs_mark_block_valid(fs, blk);
        fs->free_blocks_count--;

        *phys_block = blk;
        return FS_OK;
    }

    /* -------------------------------------------------------------- */
    /*  Single indirect: FS_DIRECT_BLOCKS .. SINGLE_INDIRECT_MAX-1    */
    /* -------------------------------------------------------------- */
    if (logical_block < SINGLE_INDIRECT_MAX) {
        uint32_t idx = logical_block - FS_DIRECT_BLOCKS;

        /* Ensure the indirect block itself exists */
        if (ino->indirect == FS_INVALID_BLOCK) {
            if (!create) {
                *phys_block = FS_INVALID_BLOCK;
                return FS_OK;
            }
            int r = alloc_indirect_node(fs, &ino->indirect);
            if (r != FS_OK) return r;
        }

        struct fs_indirect_node node;
        int r = read_indirect_node(fs, ino->indirect, &node);
        if (r != FS_OK) return r;

        uint32_t blk = node.ptrs[idx];
        if (blk != FS_INVALID_BLOCK && blk != 0) {
            *phys_block = blk;
            return FS_OK;
        }
        if (!create) {
            *phys_block = FS_INVALID_BLOCK;
            return FS_OK;
        }

        /* Allocate a data block for this slot */
        blk = fs_find_first_free_data_block(fs);
        if (blk == FS_INVALID_BLOCK) return FS_ERR_NO_SPACE;
        fs_mark_block_valid(fs, blk);
        fs->free_blocks_count--;

        node.ptrs[idx] = blk;
        r = write_indirect_node(fs, ino->indirect, &node);
        if (r != FS_OK) return r;

        *phys_block = blk;
        return FS_OK;
    }

    /* -------------------------------------------------------------- */
    /*  Double indirect: SINGLE_INDIRECT_MAX .. DOUBLE_INDIRECT_MAX-1 */
    /* -------------------------------------------------------------- */
    if (logical_block < DOUBLE_INDIRECT_MAX) {
        uint32_t adjusted = logical_block - SINGLE_INDIRECT_MAX;
        uint32_t l1_idx = adjusted / FS_INDIRECT_PTRS;
        uint32_t l2_idx = adjusted % FS_INDIRECT_PTRS;

        /* Ensure the double-indirect root block exists */
        if (ino->double_indirect == FS_INVALID_BLOCK) {
            if (!create) {
                *phys_block = FS_INVALID_BLOCK;
                return FS_OK;
            }
            int r = alloc_indirect_node(fs, &ino->double_indirect);
            if (r != FS_OK) return r;
        }

        /* Read the first-level indirect node */
        struct fs_indirect_node l1;
        int r = read_indirect_node(fs, ino->double_indirect, &l1);
        if (r != FS_OK) return r;

        /* Ensure the second-level indirect node exists */
        if (l1.ptrs[l1_idx] == FS_INVALID_BLOCK) {
            if (!create) {
                *phys_block = FS_INVALID_BLOCK;
                return FS_OK;
            }
            r = alloc_indirect_node(fs, &l1.ptrs[l1_idx]);
            if (r != FS_OK) return r;

            r = write_indirect_node(fs, ino->double_indirect, &l1);
            if (r != FS_OK) return r;
        }

        /* Read the second-level indirect node */
        struct fs_indirect_node l2;
        r = read_indirect_node(fs, l1.ptrs[l1_idx], &l2);
        if (r != FS_OK) return r;

        uint32_t blk = l2.ptrs[l2_idx];
        if (blk != FS_INVALID_BLOCK && blk != 0) {
            *phys_block = blk;
            return FS_OK;
        }
        if (!create) {
            *phys_block = FS_INVALID_BLOCK;
            return FS_OK;
        }

        /* Allocate a data block for this slot */
        blk = fs_find_first_free_data_block(fs);
        if (blk == FS_INVALID_BLOCK) return FS_ERR_NO_SPACE;
        fs_mark_block_valid(fs, blk);
        fs->free_blocks_count--;

        l2.ptrs[l2_idx] = blk;
        r = write_indirect_node(fs, l1.ptrs[l1_idx], &l2);
        if (r != FS_OK) return r;

        *phys_block = blk;
        return FS_OK;
    }

    /* Beyond double indirect range */
    return FS_ERR_UNSUPPORTED;
}
