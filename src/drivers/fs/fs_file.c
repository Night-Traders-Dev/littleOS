/* fs_file.c - path resolution and file operations */
#include "fs.h"

#include <string.h>

/* from other modules */
int fs_load_inode(struct fs *fs, uint32_t ino, struct fs_inode *out);
int fs_store_inode(struct fs *fs, const struct fs_inode *in);
int fs_bmap(struct fs *fs,
            struct fs_inode *ino,
            uint32_t logical_block,
            bool create,
            uint32_t *phys_block);
int fs_dir_lookup(struct fs *fs,
                  struct fs_inode *dir_ino,
                  const char *name,
                  uint32_t *child_ino);
int fs_dir_add(struct fs *fs,
               struct fs_inode *dir_ino,
               const char *name,
               uint32_t child_ino,
               uint8_t type);

/* shared low-level block I/O */
static int fs_read_block_i(struct fs *fs, uint32_t block, uint8_t *buf);
static int fs_write_block_i(struct fs *fs, uint32_t block, const uint8_t *buf);

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

        /* find free inode number: naive scan */
        uint32_t new_ino = FS_INVALID_INODE;
        for (uint32_t i = 1; i < fs->sb.total_inodes; i++) {
            if (fs->nat[i].block_addr == FS_INVALID_BLOCK) {
                new_ino = i;
                break;
            }
        }
        if (new_ino == FS_INVALID_INODE) return FS_ERR_NO_SPACE;

        struct fs_inode newi;
        memset(&newi, 0, sizeof(newi));
        newi.magic         = 0xFA;
        newi.inode_version = 1;
        newi.mode          = FS_MODE_REG;
        newi.size          = 0;
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

int fs_write(struct fs *fs, struct fs_file *fd, const uint8_t *buf, uint32_t count) {
    if (!fs || !fd || !buf) return FS_ERR_INVALID_ARG;

    struct fs_inode ino;
    int r = fs_load_inode(fs, fd->inode_num, &ino);
    if (r != FS_OK) return r;

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

    /* find free inode */
    uint32_t new_ino = FS_INVALID_INODE;
    for (uint32_t i = 1; i < fs->sb.total_inodes; i++) {
        if (fs->nat[i].block_addr == FS_INVALID_BLOCK) {
            new_ino = i;
            break;
        }
    }
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

int fs_unlink(struct fs *fs, const char *path) {
    (void)fs;
    (void)path;
    /* TODO: walk parent dir, zero dirent, dec link, possibly free blocks */
    return FS_ERR_UNSUPPORTED;
}
