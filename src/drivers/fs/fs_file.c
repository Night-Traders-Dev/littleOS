/* fs_file.c - path resolution and file operations */
#include "fs.h"

#include <string.h>


/* simple path tokenizer: in-place on a local copy in caller */
static const char *next_component(const char *p, char *out, size_t out_sz) {
    while (*p == '/') p++;
    if (*p == '\0') return NULL;

    size_t i = 0;
    while (*p != '/' && *p != '\0' && i + 1 < out_sz) {
        out[i++] = *p++;
    }
    out[i] = '\0';
    while (*p == '/') p++;
    return p;
}

static uint32_t *fs_active_next_node_id(struct fs *fs) {
    if (!fs) return NULL;
    return (fs->active_cp == 0) ? &fs->cp0.next_node_id : &fs->cp1.next_node_id;
}

static uint32_t fs_find_free_inode_hint(struct fs *fs) {
    uint32_t *hint = fs_active_next_node_id(fs);
    uint32_t start;

    if (!fs || !hint || fs->sb.total_inodes <= (FS_ROOT_INODE + 1u)) {
        return FS_INVALID_INODE;
    }

    start = *hint;
    if (start < (FS_ROOT_INODE + 1u) || start >= fs->sb.total_inodes) {
        start = FS_ROOT_INODE + 1u;
    }

    for (int pass = 0; pass < 2; pass++) {
        uint32_t limit = (pass == 0) ? fs->sb.total_inodes : start;
        for (uint32_t ino = start; ino < limit; ino++) {
            if (fs->nat[ino].block_addr == FS_INVALID_BLOCK) {
                *hint = (ino + 1u < fs->sb.total_inodes) ? (ino + 1u) : (FS_ROOT_INODE + 1u);
                fs->cp_dirty = true;
                return ino;
            }
        }
        start = FS_ROOT_INODE + 1u;
    }

    return FS_INVALID_INODE;
}

/* resolve absolute path; currently only from root */
static int fs_resolve_path(struct fs *fs,
                           const char *path,
                           uint32_t *ino_out,
                           uint32_t *parent_out,
                           char *last_name,
                           size_t last_name_sz) {
    if (!fs || !path || !ino_out) return FS_ERR_INVALID_ARG;
    if (path[0] != '/') return FS_ERR_INVALID_ARG;

    struct fs_inode cur;
    int r = fs_load_inode(fs, FS_ROOT_INODE, &cur);
    if (r != FS_OK) return r;

    uint32_t cur_ino = FS_ROOT_INODE;
    uint32_t parent_ino = FS_ROOT_INODE;

    char comp[64];
    const char *p = path;
    const char *next = next_component(p, comp, sizeof(comp));
    if (!next) {
        *ino_out = cur_ino;
        if (parent_out) *parent_out = parent_ino;
        if (last_name && last_name_sz) last_name[0] = '\0';
        return FS_OK;
    }

    for (;;) {
        parent_ino = cur_ino;
        struct fs_inode dir = cur;

        uint32_t child;
        r = fs_dir_lookup(fs, &dir, comp, &child);
        if (r != FS_OK) {
            /* not found: this is the last component we tried */
            *ino_out = FS_INVALID_INODE;
            if (parent_out) *parent_out = parent_ino;
            if (last_name && last_name_sz) {
                strncpy(last_name, comp, last_name_sz - 1);
                last_name[last_name_sz - 1] = '\0';
            }
            return FS_ERR_NOT_FOUND;
        }

        r = fs_load_inode(fs, child, &cur);
        if (r != FS_OK) return r;
        cur_ino = child;

        p = next;
        next = next_component(p, comp, sizeof(comp));
        if (!next) {
            *ino_out = cur_ino;
            if (parent_out) *parent_out = parent_ino;
            if (last_name && last_name_sz) last_name[0] = '\0';
            return FS_OK;
        }
    }
}

/* ===== public API ===== */

int fs_open(struct fs *fs, const char *path, uint16_t flags, struct fs_file *fd) {
    if (!fs || !path || !fd) return FS_ERR_INVALID_ARG;

    uint32_t ino;
    uint32_t parent;
    char name[64];

    int r = fs_resolve_path(fs, path, &ino, &parent, name, sizeof(name));
    if (r == FS_ERR_NOT_FOUND && (flags & FS_O_CREAT)) {
        /* create new file in parent */
        if (parent == FS_INVALID_INODE) return FS_ERR_INVALID_ARG;

        struct fs_inode parent_ino;
        r = fs_load_inode(fs, parent, &parent_ino);
        if (r != FS_OK) return r;
        if (!(parent_ino.mode & FS_MODE_DIR)) return FS_ERR_NOT_DIRECTORY;

        uint32_t new_ino = fs_find_free_inode_hint(fs);
        if (new_ino == FS_INVALID_INODE) return FS_ERR_NO_SPACE;

        struct fs_inode newi;
        memset(&newi, 0, sizeof(newi));
        newi.magic         = 0xFA;
        newi.inode_version = 2;  /* v2: inline data support */
        newi.mode          = FS_MODE_REG;
        newi.size          = 0;
        newi.inode_flags   = FS_IFLAG_INLINE_DATA;  /* start inline */
        newi.atime = newi.mtime = newi.ctime = (uint32_t)0;
        newi.link_count    = 1;
        newi.inode_num     = new_ino;
        newi.parent_inode  = parent;
        newi.generation    = 1;
        for (uint32_t i = 0; i < FS_DIRECT_BLOCKS; i++)
            newi.direct[i] = FS_INVALID_BLOCK;
        newi.indirect        = FS_INVALID_BLOCK;
        newi.double_indirect = FS_INVALID_BLOCK;

        r = fs_store_inode(fs, &newi);
        if (r != FS_OK) return r;

        /* add entry to parent directory */
        r = fs_dir_add(fs, &parent_ino, name, new_ino, 1);
        if (r != FS_OK) return r;

        /* store parent inode (size/mtime updated) */
        parent_ino.mtime = parent_ino.ctime = (uint32_t)0;
        r = fs_store_inode(fs, &parent_ino);
        if (r != FS_OK) return r;

        ino = new_ino;
    } else if (r != FS_OK) {
        return r;
    }

    fd->inode_num = ino;
    fd->position  = 0;
    fd->flags     = flags;
    return FS_OK;
}

int fs_close(struct fs *fs, struct fs_file *fd) {
    (void)fs;
    (void)fd;
    return FS_OK;
}

int fs_read(struct fs *fs, struct fs_file *fd, uint8_t *buf, uint32_t count) {
    if (!fs || !fd || !buf) return FS_ERR_INVALID_ARG;

    struct fs_inode ino;
    int r = fs_load_inode(fs, fd->inode_num, &ino);
    if (r != FS_OK) return r;

    if (fd->position >= ino.size) return 0;

    uint32_t remaining = ino.size - fd->position;
    if (count > remaining) count = remaining;

    /* Fast path: inline data (no block I/O needed) */
    if (ino.inode_flags & FS_IFLAG_INLINE_DATA) {
        if (fd->position + count > FS_INLINE_DATA_MAX)
            count = (fd->position < FS_INLINE_DATA_MAX) ?
                    FS_INLINE_DATA_MAX - fd->position : 0;
        if (count == 0) return 0;
        memcpy(buf, ino.inline_data + fd->position, count);
        fd->position += count;
        return (int)count;
    }

    /* Standard block-based read */
    uint32_t done = 0;
    uint8_t block_buf[FS_BLOCK_SIZE];

    while (done < count) {
        uint32_t off_in_file  = fd->position + done;
        uint32_t lb           = off_in_file / FS_BLOCK_SIZE;
        uint32_t off_in_block = off_in_file % FS_BLOCK_SIZE;
        uint32_t chunk        = FS_BLOCK_SIZE - off_in_block;
        if (chunk > (count - done)) chunk = count - done;

        uint32_t phys;
        r = fs_bmap(fs, &ino, lb, false, &phys);
        if (r != FS_OK) return r;
        if (phys == FS_INVALID_BLOCK) {
            memset(buf + done, 0, chunk);
        } else {
            r = fs_read_block_i(fs, phys, block_buf);
            if (r != FS_OK) return r;
            memcpy(buf + done, block_buf + off_in_block, chunk);
        }

        done += chunk;
    }

    fd->position += done;
    return (int)done;
}

/* Promote an inline-data file to block-based storage.
 * Called when a write would exceed FS_INLINE_DATA_MAX. */
static int fs_promote_inline(struct fs *fs, struct fs_inode *ino) {
    uint32_t old_size = ino->size;
    uint8_t tmp[FS_INLINE_DATA_MAX];

    if (old_size > 0) {
        memcpy(tmp, ino->inline_data, old_size);
    }

    /* Clear inline flag and data */
    ino->inode_flags &= (uint16_t)~FS_IFLAG_INLINE_DATA;
    memset(ino->inline_data, 0, FS_INLINE_DATA_MAX);
    ino->size = 0;

    /* Write old data out to blocks */
    if (old_size > 0) {
        /* Temporarily store inode so bmap can allocate blocks */
        int r = fs_store_inode(fs, ino);
        if (r != FS_OK) return r;

        /* Re-load to get fresh block address */
        r = fs_load_inode(fs, ino->inode_num, ino);
        if (r != FS_OK) return r;

        uint32_t done = 0;
        uint8_t block_buf[FS_BLOCK_SIZE];
        while (done < old_size) {
            uint32_t lb = done / FS_BLOCK_SIZE;
            uint32_t off = done % FS_BLOCK_SIZE;
            uint32_t chunk = FS_BLOCK_SIZE - off;
            if (chunk > old_size - done) chunk = old_size - done;

            uint32_t phys;
            r = fs_bmap(fs, ino, lb, true, &phys);
            if (r != FS_OK) return r;

            if (chunk != FS_BLOCK_SIZE) {
                memset(block_buf, 0, FS_BLOCK_SIZE);
            }
            memcpy(block_buf + off, tmp + done, chunk);
            r = fs_write_block_i(fs, phys, block_buf);
            if (r != FS_OK) return r;

            done += chunk;
        }
        ino->size = old_size;
    }

    return FS_OK;
}

int fs_write(struct fs *fs, struct fs_file *fd, const uint8_t *buf, uint32_t count) {
    if (!fs || !fd || !buf) return FS_ERR_INVALID_ARG;

    struct fs_inode ino;
    int r = fs_load_inode(fs, fd->inode_num, &ino);
    if (r != FS_OK) return r;

    /* Inline data fast path: write fits entirely within the inode */
    if ((ino.inode_flags & FS_IFLAG_INLINE_DATA) ||
        (ino.size == 0 && fd->position == 0 &&
         !(ino.mode & FS_MODE_DIR) &&
         fd->position + count <= FS_INLINE_DATA_MAX)) {

        /* New file: auto-enable inline if it fits */
        if (!(ino.inode_flags & FS_IFLAG_INLINE_DATA) && ino.size == 0) {
            ino.inode_flags |= FS_IFLAG_INLINE_DATA;
        }

        if ((ino.inode_flags & FS_IFLAG_INLINE_DATA) &&
            fd->position + count <= FS_INLINE_DATA_MAX) {
            memcpy(ino.inline_data + fd->position, buf, count);
            fd->position += count;
            if (fd->position > ino.size) ino.size = fd->position;
            ino.mtime = ino.ctime = (uint32_t)0;
            r = fs_store_inode(fs, &ino);
            if (r != FS_OK) return r;
            return (int)count;
        }

        /* Inline data would overflow: promote to block-based */
        if (ino.inode_flags & FS_IFLAG_INLINE_DATA) {
            r = fs_promote_inline(fs, &ino);
            if (r != FS_OK) return r;
        }
    }

    /* Standard block-based write */
    uint32_t done = 0;
    uint8_t block_buf[FS_BLOCK_SIZE];

    while (done < count) {
        uint32_t off_in_file  = fd->position + done;
        uint32_t lb           = off_in_file / FS_BLOCK_SIZE;
        uint32_t off_in_block = off_in_file % FS_BLOCK_SIZE;
        uint32_t chunk        = FS_BLOCK_SIZE - off_in_block;
        if (chunk > (count - done)) chunk = count - done;

        uint32_t phys;
        r = fs_bmap(fs, &ino, lb, true, &phys);
        if (r != FS_OK) return r;
        if (phys == FS_INVALID_BLOCK) return FS_ERR_CORRUPTED;

        if (chunk != FS_BLOCK_SIZE) {
            /* read-modify-write */
            r = fs_read_block_i(fs, phys, block_buf);
            if (r != FS_OK) return r;
        }

        memcpy(block_buf + off_in_block, buf + done, chunk);
        r = fs_write_block_i(fs, phys, block_buf);
        if (r != FS_OK) return r;

        done += chunk;
    }

    fd->position += done;
    if (fd->position > ino.size) ino.size = fd->position;
    ino.mtime = ino.ctime = (uint32_t)0;

    r = fs_store_inode(fs, &ino);
    if (r != FS_OK) return r;

    return (int)done;
}

int fs_seek(struct fs *fs, struct fs_file *fd, int32_t offset, int whence) {
    (void)fs;
    if (!fd) return FS_ERR_INVALID_ARG;

    struct fs_inode ino;
    int r = fs_load_inode(fs, fd->inode_num, &ino);
    if (r != FS_OK) return r;

    int64_t base = 0;
    if (whence == FS_SEEK_SET) base = 0;
    else if (whence == FS_SEEK_CUR) base = fd->position;
    else if (whence == FS_SEEK_END) base = ino.size;
    else return FS_ERR_INVALID_ARG;

    int64_t np = base + offset;
    if (np < 0) return FS_ERR_INVALID_ARG;
    fd->position = (uint32_t)np;
    return FS_OK;
}

/* basic mkdir: only creates empty dir inode, no "."/".." for now */
int fs_mkdir(struct fs *fs, const char *path) {
    if (!fs || !path) return FS_ERR_INVALID_ARG;

    uint32_t ino;
    uint32_t parent;
    char name[64];
    int r = fs_resolve_path(fs, path, &ino, &parent, name, sizeof(name));
    if (r == FS_OK) return FS_ERR_EXISTS;
    if (r != FS_ERR_NOT_FOUND) return r;
    if (parent == FS_INVALID_INODE) return FS_ERR_INVALID_ARG;

    struct fs_inode parent_ino;
    r = fs_load_inode(fs, parent, &parent_ino);
    if (r != FS_OK) return r;
    if (!(parent_ino.mode & FS_MODE_DIR)) return FS_ERR_NOT_DIRECTORY;

    uint32_t new_ino = fs_find_free_inode_hint(fs);
    if (new_ino == FS_INVALID_INODE) return FS_ERR_NO_SPACE;

    struct fs_inode dir;
    memset(&dir, 0, sizeof(dir));
    dir.magic         = 0xFA;
    dir.inode_version = 1;
    dir.mode          = FS_MODE_DIR;
    dir.size          = 0;
    dir.atime = dir.mtime = dir.ctime = (uint32_t)0;
    dir.link_count    = 2; /* '.' and '..' logically */
    dir.inode_num     = new_ino;
    dir.parent_inode  = parent;
    dir.generation    = 1;
    for (uint32_t i = 0; i < FS_DIRECT_BLOCKS; i++)
        dir.direct[i] = FS_INVALID_BLOCK;
    dir.indirect        = FS_INVALID_BLOCK;
    dir.double_indirect = FS_INVALID_BLOCK;

    r = fs_store_inode(fs, &dir);
    if (r != FS_OK) return r;

    r = fs_dir_add(fs, &parent_ino, name, new_ino, 2);
    if (r != FS_OK) return r;

    parent_ino.mtime = parent_ino.ctime = (uint32_t)0;
    r = fs_store_inode(fs, &parent_ino);
    if (r != FS_OK) return r;

    return FS_OK;
}

/* opendir/readdir/unlink can be layered similarly; still minimal here */

int fs_opendir(struct fs *fs, const char *path, struct fs_file *fd) {
    if (!fs || !path || !fd) return FS_ERR_INVALID_ARG;

    uint32_t ino;
    int r = fs_resolve_path(fs, path, &ino, NULL, NULL, 0);
    if (r != FS_OK) return r;

    struct fs_inode dir;
    r = fs_load_inode(fs, ino, &dir);
    if (r != FS_OK) return r;
    if (!(dir.mode & FS_MODE_DIR)) return FS_ERR_NOT_DIRECTORY;

    fd->inode_num = ino;
    fd->position  = 0;
    fd->flags     = FS_O_RDONLY;
    return FS_OK;
}

/* Simple linear readdir over packed entries; ignores "."/".." since not created */
int fs_readdir(struct fs *fs, struct fs_file *fd, struct fs_dirent *entry) {
    if (!fs || !fd || !entry) return FS_ERR_INVALID_ARG;

    struct fs_inode dir;
    int r = fs_load_inode(fs, fd->inode_num, &dir);
    if (r != FS_OK) return r;
    if (!(dir.mode & FS_MODE_DIR)) return FS_ERR_NOT_DIRECTORY;

    if (fd->position >= dir.size) return FS_ERR_NOT_FOUND;

    uint32_t off_in_file = fd->position;
    uint32_t lb          = off_in_file / FS_BLOCK_SIZE;
    uint32_t off_in_block = off_in_file % FS_BLOCK_SIZE;

    uint32_t phys;
    r = fs_bmap(fs, &dir, lb, false, &phys);
    if (r != FS_OK) return r;
    if (phys == FS_INVALID_BLOCK) return FS_ERR_CORRUPTED;

    uint8_t buf[FS_BLOCK_SIZE];
    r = fs_read_block_i(fs, phys, buf);
    if (r != FS_OK) return r;

    while (off_in_block + sizeof(struct fs_dirent) <= FS_BLOCK_SIZE) {
        struct fs_dirent *de = (struct fs_dirent *)(buf + off_in_block);
        if (de->entry_size == 0) break;
        if (de->name_len != 0) {
            memcpy(entry, de, sizeof(*entry));
            fd->position += de->entry_size;
            return FS_OK;
        }
        fd->position += de->entry_size;
        off_in_block += de->entry_size;
    }

    return FS_ERR_NOT_FOUND;
}

/* Remove a dirent from a directory block.
 * Zeroes the name_len to mark the slot as deleted; the entry_size is
 * preserved so the linked-list of variable-length entries stays intact. */
static int fs_dir_remove(struct fs *fs, struct fs_inode *dir_ino,
                         const char *name, uint32_t target_ino) {
    uint32_t hash = 0;
    {
        const char *p = name;
        uint32_t h = 5381u;
        unsigned char c;
        while ((c = (unsigned char)*p++) != 0)
            h = ((h << 5) + h) + c;
        hash = h;
    }

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
            if (de->name_len != 0 && de->inode_num == target_ino &&
                de->hash == hash) {
                /* Mark entry as deleted */
                de->name_len  = 0;
                de->inode_num = 0;
                de->type      = 0;
                de->hash      = 0;
                return fs_write_block_i(fs, phys, buf);
            }
            off += de->entry_size;
        }
    }

    return FS_ERR_NOT_FOUND;
}

/* Free all data blocks owned by an inode (direct + indirect + double indirect).
 * Decrements SIT valid_count and increments free_blocks_count for each freed
 * block, then invalidates the inode's NAT entry. */
static void fs_free_inode_blocks(struct fs *fs, struct fs_inode *ino) {
    /* Direct blocks */
    for (uint32_t i = 0; i < FS_DIRECT_BLOCKS; i++) {
        uint32_t blk = ino->direct[i];
        if (blk != FS_INVALID_BLOCK && blk != 0) {
            if (fs_mark_block_free(fs, blk) == FS_OK) {
                fs->free_blocks_count++;
            }
            ino->direct[i] = FS_INVALID_BLOCK;
        }
    }

    /* Single indirect */
    if (ino->indirect != FS_INVALID_BLOCK) {
        struct fs_indirect_node node;
        uint8_t nbuf[FS_BLOCK_SIZE];
        if (fs_read_block_i(fs, ino->indirect, nbuf) == FS_OK) {
            memcpy(&node, nbuf, sizeof(node));
            for (uint32_t i = 0; i < FS_INDIRECT_PTRS; i++) {
                uint32_t blk = node.ptrs[i];
                if (blk != FS_INVALID_BLOCK && blk != 0) {
                    if (fs_mark_block_free(fs, blk) == FS_OK) {
                        fs->free_blocks_count++;
                    }
                }
            }
        }
        /* Free the indirect node block itself */
        if (fs_mark_block_free(fs, ino->indirect) == FS_OK) {
            fs->free_blocks_count++;
        }
        ino->indirect = FS_INVALID_BLOCK;
    }

    /* Double indirect — free leaf data blocks + L2 nodes + L1 node */
    if (ino->double_indirect != FS_INVALID_BLOCK) {
        struct fs_indirect_node l1;
        uint8_t l1buf[FS_BLOCK_SIZE];
        if (fs_read_block_i(fs, ino->double_indirect, l1buf) == FS_OK) {
            memcpy(&l1, l1buf, sizeof(l1));
            for (uint32_t i = 0; i < FS_INDIRECT_PTRS; i++) {
                if (l1.ptrs[i] == FS_INVALID_BLOCK) continue;

                struct fs_indirect_node l2;
                uint8_t l2buf[FS_BLOCK_SIZE];
                if (fs_read_block_i(fs, l1.ptrs[i], l2buf) == FS_OK) {
                    memcpy(&l2, l2buf, sizeof(l2));
                    for (uint32_t j = 0; j < FS_INDIRECT_PTRS; j++) {
                        uint32_t blk = l2.ptrs[j];
                        if (blk != FS_INVALID_BLOCK && blk != 0) {
                            if (fs_mark_block_free(fs, blk) == FS_OK) {
                                fs->free_blocks_count++;
                            }
                        }
                    }
                }
                /* Free the L2 node */
                if (fs_mark_block_free(fs, l1.ptrs[i]) == FS_OK) {
                    fs->free_blocks_count++;
                }
            }
        }
        /* Free the L1 (double-indirect root) node */
        if (fs_mark_block_free(fs, ino->double_indirect) == FS_OK) {
            fs->free_blocks_count++;
        }
        ino->double_indirect = FS_INVALID_BLOCK;
    }

    /* Invalidate the inode's own block in NAT */
    uint32_t ino_num = ino->inode_num;
    if (ino_num > 0 && ino_num < fs->sb.total_inodes) {
        uint32_t iblk = fs->nat[ino_num].block_addr;
        if (iblk != FS_INVALID_BLOCK) {
            if (fs_mark_block_free(fs, iblk) == FS_OK) {
                fs->free_blocks_count++;
            }
        }
        fs->nat[ino_num].block_addr = FS_INVALID_BLOCK;
        fs->nat[ino_num].type       = 0;
        fs->nat_dirty = true;
    }
}

int fs_unlink(struct fs *fs, const char *path) {
    if (!fs || !path) return FS_ERR_INVALID_ARG;

    /* Resolve the target and its parent */
    uint32_t ino;
    uint32_t parent;
    char name[64];
    int r = fs_resolve_path(fs, path, &ino, &parent, name, sizeof(name));
    if (r != FS_OK) return r;

    /* Can't unlink root */
    if (ino == FS_ROOT_INODE) return FS_ERR_PERMISSION;

    /* Load the target inode */
    struct fs_inode target;
    r = fs_load_inode(fs, ino, &target);
    if (r != FS_OK) return r;

    /* Don't unlink non-empty directories */
    if ((target.mode & FS_MODE_DIR) && target.size > 0)
        return FS_ERR_PERMISSION;

    /* Load parent and remove the directory entry */
    struct fs_inode parent_ino;
    r = fs_load_inode(fs, parent, &parent_ino);
    if (r != FS_OK) return r;

    /* Resolve the last component name for removal */
    /* If name is empty (exact match), re-derive from path */
    char rm_name[64];
    if (name[0] == '\0') {
        /* Extract last component from path */
        const char *p = path + strlen(path) - 1;
        while (p > path && *p == '/') p--;
        const char *end = p + 1;
        while (p > path && *(p - 1) != '/') p--;
        size_t len = (size_t)(end - p);
        if (len >= sizeof(rm_name)) len = sizeof(rm_name) - 1;
        memcpy(rm_name, p, len);
        rm_name[len] = '\0';
    } else {
        strncpy(rm_name, name, sizeof(rm_name) - 1);
        rm_name[sizeof(rm_name) - 1] = '\0';
    }

    r = fs_dir_remove(fs, &parent_ino, rm_name, ino);
    if (r != FS_OK) return r;

    /* Decrement link count */
    if (target.link_count > 0) target.link_count--;

    if (target.link_count == 0) {
        /* No more references — free all blocks */
        fs_free_inode_blocks(fs, &target);
    } else {
        /* Still has links — just update the inode */
        r = fs_store_inode(fs, &target);
        if (r != FS_OK) return r;
    }

    /* Update parent timestamps */
    parent_ino.mtime = parent_ino.ctime = (uint32_t)0;
    r = fs_store_inode(fs, &parent_ino);

    return r;
}
