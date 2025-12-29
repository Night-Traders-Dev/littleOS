/* fs_dir.c - directory layout and helpers */
#include "fs.h"

#include <string.h>

/* simple hash: djb2 on name */
static uint32_t fs_name_hash(const char *name) {
    uint32_t h = 5381u;
    unsigned char c;
    while ((c = (unsigned char)*name++) != 0) {
        h = ((h << 5) + h) + c;
    }
    return h;
}

/* Scan directory inode for name; dir_ino must be DIR. */
int fs_dir_lookup(struct fs *fs,
                  struct fs_inode *dir_ino,
                  const char *name,
                  uint32_t *child_ino) {
    if (!fs || !dir_ino || !name || !child_ino) return FS_ERR_INVALID_ARG;
    if (!(dir_ino->mode & FS_MODE_DIR)) return FS_ERR_NOT_DIRECTORY;

    uint32_t hash = fs_name_hash(name);
    uint32_t blocks = (dir_ino->size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;

    uint8_t buf[FS_BLOCK_SIZE];

    for (uint32_t lb = 0; lb < blocks; lb++) {
        uint32_t phys;
        int r = fs_bmap(fs, dir_ino, lb, false, &phys);
        if (r != FS_OK) return r;
        if (phys == FS_INVALID_BLOCK) continue;

        r = fs_read_block_i(fs, phys, buf);
        if (r != FS_OK) return r;

        uint32_t off = 0;
        while (off + sizeof(struct fs_dirent) <= FS_BLOCK_SIZE) {
            struct fs_dirent *de = (struct fs_dirent *)(buf + off);
            if (de->entry_size == 0) break;
            if (de->name_len == 0) {
                off += de->entry_size;
                continue;
            }
            if (de->hash == hash && de->name_len < de->entry_size) {
                const char *de_name = (const char *)(de + 1);
                if (strncmp(de_name, name, de->name_len) == 0 &&
                    name[de->name_len] == '\0') {
                    *child_ino = de->inode_num;
                    return FS_OK;
                }
            }
            off += de->entry_size;
        }
    }

    return FS_ERR_NOT_FOUND;
}

/* Append a dirent to directory, growing size as needed */
int fs_dir_add(struct fs *fs,
               struct fs_inode *dir_ino,
               const char *name,
               uint32_t child_ino,
               uint8_t type) {
    if (!fs || !dir_ino || !name) return FS_ERR_INVALID_ARG;
    if (!(dir_ino->mode & FS_MODE_DIR)) return FS_ERR_NOT_DIRECTORY;

    uint8_t name_len = (uint8_t)strlen(name);
    if (name_len == 0) return FS_ERR_INVALID_ARG;

    uint32_t hash = fs_name_hash(name);

    /* Required size for this entry (header + name, 4-byte aligned) */
    uint16_t rec_len = (uint16_t)(sizeof(struct fs_dirent) + name_len);
    if (rec_len & 3u) rec_len = (uint16_t)((rec_len + 3u) & ~3u);

    uint32_t file_blocks = (dir_ino->size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;
    uint8_t  buf[FS_BLOCK_SIZE];

    for (uint32_t lb = 0;; lb++) {
        uint32_t phys;
        int r = fs_bmap(fs, dir_ino, lb, true, &phys);
        if (r != FS_OK) return r;
        if (phys == FS_INVALID_BLOCK) return FS_ERR_NO_SPACE;

        if (lb >= file_blocks) {
            /* new block */
            memset(buf, 0, sizeof(buf));
        } else {
            r = fs_read_block_i(fs, phys, buf);
            if (r != FS_OK) return r;
        }

        uint32_t off = 0;
        while (off + sizeof(struct fs_dirent) <= FS_BLOCK_SIZE) {
            struct fs_dirent *de = (struct fs_dirent *)(buf + off);
            if (de->entry_size == 0) {
                /* free space from here to end of block */
                uint16_t remaining = (uint16_t)(FS_BLOCK_SIZE - off);
                if (remaining < rec_len) break;

                de->entry_size = remaining;
                de->inode_num  = child_ino;
                de->name_len   = name_len;
                de->type       = type;
                de->hash       = hash;
                char *dst = (char *)(de + 1);
                memcpy(dst, name, name_len);
                goto write_out;
            }

            /* occupied entry: see if we can split its slack space */
            uint16_t used = (uint16_t)(sizeof(struct fs_dirent) + de->name_len);
            if (used & 3u) used = (uint16_t)((used + 3u) & ~3u);
            if (de->entry_size > used && (de->entry_size - used) >= rec_len) {
                uint16_t new_len = de->entry_size - used;
                de->entry_size   = used;

                struct fs_dirent *new_de =
                    (struct fs_dirent *)((uint8_t *)de + used);
                new_de->entry_size = new_len;
                new_de->inode_num  = child_ino;
                new_de->name_len   = name_len;
                new_de->type       = type;
                new_de->hash       = hash;
                char *dst = (char *)(new_de + 1);
                memcpy(dst, name, name_len);
                goto write_out;
            }

            off += de->entry_size;
        }

        /* no space in this block; loop to next logical block */
        file_blocks = (lb + 1);
        continue;
    }

write_out: {
        /* update directory size if needed */
        uint32_t needed_size =
            (uint32_t)((int32_t)(dir_ino->size) < 0 ? 0 : dir_ino->size);
        uint32_t block_end = (uint32_t)(((dir_ino->size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE)
                                        * FS_BLOCK_SIZE);
        if (block_end < (FS_BLOCK_SIZE * (1u))) {
            /* nothing */
            ;
        }
        /* more simply, size = max(size, (lb+1)*block_size) */
        /* but we know directory is append-only for now */
        /* Set size conservatively */
        uint32_t new_size = (uint32_t)((dir_ino->size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE)
                            * FS_BLOCK_SIZE;
        if (new_size == 0) new_size = FS_BLOCK_SIZE;
        if (new_size > dir_ino->size) dir_ino->size = new_size;
    }

    /* write back the modified block */
    uint32_t cur_blocks = (dir_ino->size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;
    uint32_t last_lb = (cur_blocks == 0) ? 0 : (cur_blocks - 1);
    uint32_t last_phys;
    int r2 = fs_bmap(fs, dir_ino, last_lb, false, &last_phys);
    if (r2 != FS_OK || last_phys == FS_INVALID_BLOCK) return FS_ERR_CORRUPTED;

    r2 = fs_write_block_i(fs, last_phys, buf);
    if (r2 != FS_OK) return r2;

    return FS_OK;
}
