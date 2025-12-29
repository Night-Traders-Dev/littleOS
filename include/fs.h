/* fs.h - littleOS F2FS-inspired filesystem (Phase 2 core) */
#ifndef LITTLEOS_FS_H
#define LITTLEOS_FS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================
 * Constants
 * ========================= */
#define FS_MAGIC                0xF2FEu
#define FS_VERSION              1u

#define FS_BLOCK_SIZE           512u
#define FS_SEGMENT_SIZE         4096u
#define FS_BLOCKS_PER_SEGMENT   (FS_SEGMENT_SIZE / FS_BLOCK_SIZE)

#define FS_DEFAULT_MAX_INODES   256u

#define FS_INVALID_BLOCK        0xFFFFFFFFu
#define FS_INVALID_INODE        0u

/* Fixed block locations */
#define FS_SB_BLOCK             0u
#define FS_CP0_BLOCK            1u
#define FS_CP1_BLOCK            2u
#define FS_FIXED_METADATA_BLOCKS 3u /* SB + CP0 + CP1 */

/* Inode numbering */
#define FS_ROOT_INODE           2u

/* Inode layout limits */
#define FS_DIRECT_BLOCKS        10u
#define FS_INDIRECT_PTRS        (FS_BLOCK_SIZE / 4u)

/* File types */
#define FS_MODE_REG             0x8000u
#define FS_MODE_DIR             0x4000u

/* Open flags */
#define FS_O_RDONLY             0x0000u
#define FS_O_WRONLY             0x0001u
#define FS_O_RDWR               0x0002u
#define FS_O_APPEND             0x0004u
#define FS_O_CREAT              0x0008u
#define FS_O_TRUNC              0x0010u

/* Seek whence */
#define FS_SEEK_SET             0
#define FS_SEEK_CUR             1
#define FS_SEEK_END             2

/* Error codes */
#define FS_OK                   0
#define FS_ERR_NO_SPACE         (-1)
#define FS_ERR_NOT_FOUND        (-2)
#define FS_ERR_EXISTS           (-3)
#define FS_ERR_INVALID_INODE    (-4)
#define FS_ERR_INVALID_BLOCK    (-5)
#define FS_ERR_IO               (-6)
#define FS_ERR_NOT_DIRECTORY    (-7)
#define FS_ERR_PERMISSION       (-8)
#define FS_ERR_CORRUPTED        (-9)
#define FS_ERR_INVALID_ARG      (-10)
#define FS_ERR_UNSUPPORTED      (-11)

/* =========================
 * Storage backend interface
 * ========================= */
typedef int (*fs_read_block_fn)(void *ctx, uint32_t block_addr, uint8_t *buf);
typedef int (*fs_write_block_fn)(void *ctx, uint32_t block_addr, const uint8_t *buf);
typedef int (*fs_erase_sector_fn)(void *ctx, uint32_t sector_addr);

/* =========================
 * On-disk structures
 * ========================= */

#pragma pack(push, 1)

/* Superblock (512 bytes) */
struct fs_superblock {
    uint16_t magic;
    uint16_t version;
    uint16_t block_size;
    uint16_t segment_size;

    uint32_t total_blocks;
    uint32_t total_segments;

    uint32_t total_inodes;
    uint32_t root_inode;

    uint32_t nat_start_block;
    uint32_t nat_blocks;

    uint32_t sit_start_block;
    uint32_t sit_blocks;

    uint32_t main_start_block;

    uint32_t flags;
    uint32_t mount_count;

    uint32_t last_sync_time;
    uint32_t creation_time;

    uint32_t sb_crc32;

    uint8_t  reserved[FS_BLOCK_SIZE - 64];
};
_Static_assert(sizeof(struct fs_superblock) == FS_BLOCK_SIZE,
               "superblock must be 512 bytes");

/* NAT entry (8 bytes). */
struct fs_nat_entry {
    uint32_t block_addr;          /* physical block address */
    uint16_t version;             /* rewrite counter */
    uint8_t  type;                /* 0=none,1=inode,2=indirect,3=data */
    uint8_t  _pad;
};
_Static_assert(sizeof(struct fs_nat_entry) == 8,
               "NAT entry must be 8 bytes");

/* SIT entry (4 bytes) */
struct fs_sit_entry {
    uint16_t valid_count;         /* valid blocks in segment */
    uint8_t  flags;
    uint8_t  age;
};
_Static_assert(sizeof(struct fs_sit_entry) == 4,
               "SIT entry must be 4 bytes");

/* Checkpoint (512 bytes) */
struct fs_checkpoint {
    uint32_t checkpoint_num;
    uint32_t timestamp;

    uint32_t free_blocks;
    uint32_t next_node_id;

    uint32_t active_node_segment;
    uint32_t active_inode_segment;
    uint32_t active_data_segment;

    uint32_t orphan_count;
    uint32_t orphan_inodes[32];

    uint32_t cp_crc32;

    uint8_t  reserved[FS_BLOCK_SIZE - 164];
};
_Static_assert(sizeof(struct fs_checkpoint) == FS_BLOCK_SIZE,
               "checkpoint must be 512 bytes");

/* Inode (512 bytes) */
struct fs_inode {
    uint8_t  magic;
    uint8_t  inode_version;
    uint16_t mode;

    uint32_t size;

    uint32_t atime;
    uint32_t mtime;
    uint32_t ctime;

    uint16_t link_count;
    uint16_t _pad0;

    uint32_t direct[FS_DIRECT_BLOCKS];

    uint32_t indirect;
    uint32_t double_indirect;

    uint32_t inode_num;
    uint32_t parent_inode;

    uint32_t generation;
    uint32_t inode_crc32;

    uint8_t  reserved[FS_BLOCK_SIZE - 88];
};
_Static_assert(sizeof(struct fs_inode) == FS_BLOCK_SIZE,
               "inode must be 512 bytes");

/* Indirect node */
struct fs_indirect_node {
    uint32_t ptrs[FS_INDIRECT_PTRS];
};
_Static_assert(sizeof(struct fs_indirect_node) == FS_BLOCK_SIZE,
               "indirect node must be 512 bytes");

#pragma pack(pop)

/* =========================
 * In-memory structures
 * ========================= */

struct fs_file {
    uint32_t inode_num;
    uint32_t position;
    uint16_t flags;
    uint16_t _pad;
};

struct fs_dirent {
    uint16_t entry_size;
    uint32_t inode_num;
    uint8_t  name_len;
    uint8_t  type;     /* 1=file,2=dir */
    uint32_t hash;
    /* char name[]; */
} __attribute__((packed));

struct fs {
    /* backend */
    void *storage_ctx;
    fs_read_block_fn  read_block;
    fs_write_block_fn write_block;
    fs_erase_sector_fn erase_sector;

    /* cached on-disk state */
    struct fs_superblock sb;
    struct fs_checkpoint cp0;
    struct fs_checkpoint cp1;
    uint8_t active_cp; /* 0 or 1 */

    /* in-memory NAT/SIT */
    struct fs_nat_entry *nat; /* [total_inodes] */
    struct fs_sit_entry *sit; /* [total_segments] */

    /* counters */
    uint32_t free_blocks_count;

    /* dirty flags */
    bool sb_dirty;
    bool cp_dirty;
    bool nat_dirty;
    bool sit_dirty;
};

/* =========================
 * Helpers (inline)
 * ========================= */

static inline uint32_t fs_div_ceil_u32(uint32_t a, uint32_t b) {
    return (a + b - 1u) / b;
}

static inline uint32_t fs_nat_blocks_for_inodes(uint32_t total_inodes) {
    const uint32_t entries_per_block =
        FS_BLOCK_SIZE / (uint32_t)sizeof(struct fs_nat_entry);
    return fs_div_ceil_u32(total_inodes, entries_per_block);
}

static inline uint32_t fs_sit_blocks_for_segments(uint32_t total_segments) {
    const uint32_t entries_per_block =
        FS_BLOCK_SIZE / (uint32_t)sizeof(struct fs_sit_entry);
    return fs_div_ceil_u32(total_segments, entries_per_block);
}

/* =========================
 * Public API
 * ========================= */

/* Backend setup */
void fs_set_storage_backend(struct fs *fs,
                            void *ctx,
                            fs_read_block_fn rfn,
                            fs_write_block_fn wfn,
                            fs_erase_sector_fn efn);

/* Lifecycle */
int fs_format(struct fs *fs, uint32_t total_blocks);
int fs_mount(struct fs *fs);
int fs_sync(struct fs *fs);
int fs_unmount(struct fs *fs);
int fs_fsck(struct fs *fs);

/* Path-based API */
int fs_open(struct fs *fs, const char *path, uint16_t flags, struct fs_file *fd);
int fs_close(struct fs *fs, struct fs_file *fd);
int fs_read(struct fs *fs, struct fs_file *fd, uint8_t *buf, uint32_t count);
int fs_write(struct fs *fs, struct fs_file *fd, const uint8_t *buf, uint32_t count);
int fs_seek(struct fs *fs, struct fs_file *fd, int32_t offset, int whence);

int fs_mkdir(struct fs *fs, const char *path);
int fs_opendir(struct fs *fs, const char *path, struct fs_file *fd);
int fs_readdir(struct fs *fs, struct fs_file *fd, struct fs_dirent *entry);
int fs_unlink(struct fs *fs, const char *path);

/* ====== low-level inode / dir helpers (optional external use) ====== */

/* Inode I/O */
int fs_load_inode(struct fs *fs, uint32_t ino, struct fs_inode *out);
int fs_store_inode(struct fs *fs, const struct fs_inode *in);

/* Block mapping (direct only for now) */
int fs_bmap(struct fs *fs,
            struct fs_inode *ino,
            uint32_t logical_block,
            bool create,
            uint32_t *phys_block);

/* Directory helpers */
int fs_dir_lookup(struct fs *fs,
                  struct fs_inode *dir_ino,
                  const char *name,
                  uint32_t *child_ino);

int fs_dir_add(struct fs *fs,
               struct fs_inode *dir_ino,
               const char *name,
               uint32_t child_ino,
               uint8_t type);
/* ===== internal shared helpers (used across fs_*.c) ===== */

/* low-level block I/O (implemented in fs_core.c) */
int fs_read_block_i(struct fs *fs, uint32_t block, uint8_t *buf);
int fs_write_block_i(struct fs *fs, uint32_t block, const uint8_t *buf);

/* simple allocator helpers (fs_core.c) */
uint32_t fs_find_first_free_data_block(struct fs *fs);
int      fs_mark_block_valid(struct fs *fs, uint32_t block_addr);


#ifdef __cplusplus
}
#endif

#endif /* LITTLEOS_FS_H */
