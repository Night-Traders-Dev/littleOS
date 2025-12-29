// cmd_fs.c - Enhanced shell commands with PERSISTENCE FIXES
// Includes diagnostics for .noinit recovery

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "fs.h"

/* ===== PERSISTENT STORAGE (.noinit section) ===== */

// CRITICAL PERSISTENCE FIXES:
// 1. (used) = Don't optimize away as unused
// 2. (retain) = Keep even if linker doesn't see references
// 3. section(".noinit") = Place in .noinit, not initialized at startup
// 4. NOINIT storage survives software reboot (watchdog 10-50ms)
// 5. Does NOT survive power cycle or full reset

#define NOINIT __attribute__((section(".noinit"), used, retain))
#define FS_BACKEND_SIZE (128 * FS_BLOCK_SIZE)

// Persistent storage array - survives software reboot
NOINIT static uint8_t fs_backend_noinit[FS_BACKEND_SIZE];

// Size marker - CRITICAL: indicates if storage is valid
// When fs_init writes here, it marks storage as valid
// When fs_mount reads non-zero value, it recovers from .noinit
// If this is 0, storage was cleared (full reset or first boot)
NOINIT static uint32_t fs_backend_size_noinit;

/* ===== RAM BACKEND STRUCTURE ===== */

struct ram_backend {
    uint8_t *data;
    uint32_t blocks;
};

static struct ram_backend g_rb = {0};
static struct fs g_fs = {0};
static bool g_fs_initialized = false;
static bool g_fs_mounted = false;

/* ===== BACKEND FUNCTIONS ===== */

static int ram_read_block(void *ctx, uint32_t block_addr, uint8_t *buf) {
    struct ram_backend *rb = (struct ram_backend *)ctx;
    if (!rb || !buf) return FS_ERR_INVALID_ARG;
    if (block_addr >= rb->blocks) return FS_ERR_INVALID_BLOCK;
    memcpy(buf, rb->data + (size_t)block_addr * FS_BLOCK_SIZE, FS_BLOCK_SIZE);
    return FS_OK;
}

static int ram_write_block(void *ctx, uint32_t block_addr, const uint8_t *buf) {
    struct ram_backend *rb = (struct ram_backend *)ctx;
    if (!rb || !buf) return FS_ERR_INVALID_ARG;
    if (block_addr >= rb->blocks) return FS_ERR_INVALID_BLOCK;
    memcpy(rb->data + (size_t)block_addr * FS_BLOCK_SIZE, buf, FS_BLOCK_SIZE);
    return FS_OK;
}

static int ram_erase_sector(void *ctx, uint32_t sector_addr) {
    (void)sector_addr;
    (void)ctx;
    return FS_OK;
}

/* ===== UTILITY FUNCTIONS ===== */

static void dump_superblock(const struct fs_superblock *sb) {
    printf("Superblock:\n");
    printf("  magic = 0x%04X\n", sb->magic);
    printf("  version = %u\n", sb->version);
    printf("  block_size = %u\n", sb->block_size);
    printf("  segment_size = %u\n", sb->segment_size);
    printf("  total_blocks = %u\n", sb->total_blocks);
    printf("  total_segments = %u\n", sb->total_segments);
    printf("  total_inodes = %u\n", sb->total_inodes);
    printf("  root_inode = %u\n", sb->root_inode);
    printf("  nat_start = %u (blocks=%u)\n", sb->nat_start_block, sb->nat_blocks);
    printf("  sit_start = %u (blocks=%u)\n", sb->sit_start_block, sb->sit_blocks);
    printf("  main_start = %u\n", sb->main_start_block);
    printf("  mount_count = %u\n", sb->mount_count);
}

// Diagnostic dump for .noinit persistence
static void dump_noinit_status(void) {
    printf(".noinit Persistence Status:\n");
    printf("  fs_backend_size_noinit = %u\n", fs_backend_size_noinit);
    printf("  fs_backend_noinit[0..15] = ");
    for (int i = 0; i < 16; i++) {
        printf("%02X ", fs_backend_noinit[i]);
    }
    printf("\n");
    
    if (fs_backend_size_noinit == 0) {
        printf("  Status: NO PERSISTENT DATA (first boot or full reset)\n");
    } else if (fs_backend_size_noinit <= 256) {
        printf("  Status: VALID - %u blocks available for recovery\n", fs_backend_size_noinit);
    } else {
        printf("  Status: CORRUPTED - size %u exceeds max 256\n", fs_backend_size_noinit);
    }
}

static void cmd_fs_usage(void) {
    printf("Usage:\n");
    printf("  fs init <blocks>        - allocate and format filesystem\n");
    printf("  fs mount                - mount filesystem (auto-recovery)\n");
    printf("  fs fsck                 - run filesystem check\n");
    printf("  fs sync                 - persist checkpoints\n");
    printf("  fs info                 - print superblock info\n");
    printf("  fs status               - show persistence status (NEW)\n");
    printf("  fs touch <path>         - create empty file\n");
    printf("  fs cat <path>           - read file contents\n");
    printf("  fs write <path> <str>   - write string to file\n");
    printf("  fs mkdir <path>         - create directory\n");
    printf("  fs ls [path]            - list directory (default /)\n");
    printf("  fs rm <path>            - delete file\n");
    printf("  fs rmdir <path>         - delete empty directory\n");
    printf("  fs stat <path>          - show file/directory stats\n");
    printf("  fs find <path> [name]   - find files in directory\n");
}

/* ===== FILESYSTEM COMMANDS ===== */

static int cmd_fs_init(uint32_t blocks) {
    if (blocks == 0 || blocks > 256) {
        printf("fs: invalid block count (max 256)\n");
        return FS_ERR_INVALID_ARG;
    }

    if (blocks * FS_BLOCK_SIZE > FS_BACKEND_SIZE) {
        printf("fs: too large for .noinit (%u blocks * %u = %u bytes, max %u)\n",
            blocks, FS_BLOCK_SIZE, blocks * FS_BLOCK_SIZE, FS_BACKEND_SIZE);
        return FS_ERR_NO_SPACE;
    }

    g_rb.data = fs_backend_noinit;
    g_rb.blocks = blocks;
    
    // CRITICAL: Write size marker to .noinit
    // This marks the storage as initialized and tells fs_mount() how many blocks to recover
    fs_backend_size_noinit = blocks;
    
    // Memory barrier: ensure size marker is written before zeroing data
    __asm__ volatile("" : : "m" (fs_backend_size_noinit) : "memory");

    memset(g_rb.data, 0, (size_t)blocks * FS_BLOCK_SIZE);
    memset(&g_fs, 0, sizeof(g_fs));

    fs_set_storage_backend(&g_fs, &g_rb,
        ram_read_block,
        ram_write_block,
        ram_erase_sector);

    int r = fs_format(&g_fs, blocks);
    if (r != FS_OK) {
        printf("fs: fs_format failed: %d\n", r);
        return r;
    }

    r = fs_sync(&g_fs);
    if (r != FS_OK) {
        printf("fs: fs_sync after format failed: %d\n", r);
        return r;
    }

    g_fs_initialized = true;
    g_fs_mounted = false;
    printf("fs: formatted RAM FS (%u blocks, %u bytes, persistent .noinit)\n",
        blocks, blocks * FS_BLOCK_SIZE);
    printf("fs: size marker written to .noinit (%u)\n", fs_backend_size_noinit);
    return FS_OK;
}

static int cmd_fs_mount(void) {
    // CRITICAL: Read size marker from .noinit to check persistence
    uint32_t recovered_blocks = fs_backend_size_noinit;
    
    printf("fs: checking .noinit recovery: size_marker=%u\n", recovered_blocks);
    
    if (recovered_blocks > 0 && recovered_blocks <= 256) {
        // VALID PERSISTENT DATA - recover from .noinit
        g_rb.data = fs_backend_noinit;
        g_rb.blocks = recovered_blocks;
        printf("fs: ✓ recovered %u blocks from .noinit\n", g_rb.blocks);
    } else if (g_rb.data && g_rb.blocks > 0) {
        // Fallback: already in memory (no reboot yet)
        printf("fs: using in-memory backend (%u blocks)\n", g_rb.blocks);
    } else {
        // NO PERSISTENCE - first boot or full reset
        printf("fs: ✗ no persistent FS found (size_marker=0)\n");
        printf("fs: run 'fs init <blocks>' to create filesystem\n");
        return FS_ERR_INVALID_ARG;
    }

    memset(&g_fs, 0, sizeof(g_fs));
    fs_set_storage_backend(&g_fs, &g_rb,
        ram_read_block,
        ram_write_block,
        ram_erase_sector);

    int r = fs_mount(&g_fs);
    if (r != FS_OK) {
        printf("fs: fs_mount failed: %d\n", r);
        g_fs_mounted = false;
        return r;
    }

    g_fs_initialized = true;
    g_fs_mounted = true;
    printf("fs: mounted (mount_count=%u)\n", g_fs.sb.mount_count);
    return FS_OK;
}

static int cmd_fs_fsck(void) {
    if (!g_fs_initialized) {
        printf("fs: not initialized/mounted\n");
        return FS_ERR_INVALID_ARG;
    }

    int r = fs_fsck(&g_fs);
    if (r != FS_OK) {
        printf("fs: fs_fsck failed: %d\n", r);
        return r;
    }

    printf("fs: fsck OK\n");
    return FS_OK;
}

static int cmd_fs_sync(void) {
    if (!g_fs_initialized) {
        printf("fs: not initialized/mounted\n");
        return FS_ERR_INVALID_ARG;
    }

    int r = fs_sync(&g_fs);
    if (r != FS_OK) {
        printf("fs: fs_sync failed: %d\n", r);
        return r;
    }

    printf("fs: sync OK (checkpoints written)\n");
    return FS_OK;
}

static int cmd_fs_info(void) {
    if (!g_fs_initialized) {
        printf("fs: not initialized/mounted\n");
        return FS_ERR_INVALID_ARG;
    }

    dump_superblock(&g_fs.sb);
    printf("Runtime:\n");
    printf("  free_blocks = %u\n", g_fs.free_blocks_count);
    printf("  active_cp = %u\n", (unsigned)g_fs.active_cp);
    printf("  mounted = %s\n", g_fs_mounted ? "yes" : "no");
    printf("  backend = %u blocks\n", g_rb.blocks);
    return FS_OK;
}

// NEW DIAGNOSTIC COMMAND
static int cmd_fs_status(void) {
    printf("Filesystem Persistence Status\n");
    printf("════════════════════════════════════════════════════════════\n");
    dump_noinit_status();
    printf("\n");
    
    if (g_fs_initialized && g_fs_mounted) {
        dump_superblock(&g_fs.sb);
    } else {
        printf("Filesystem Status: NOT MOUNTED\n");
    }
    
    return FS_OK;
}

/* ===== HIGH-LEVEL FILE OPERATIONS ===== */

static int cmd_fs_touch(const char *path) {
    if (!g_fs_initialized) {
        printf("fs: filesystem not initialized/mounted\n");
        return FS_ERR_INVALID_ARG;
    }

    struct fs_file fd;
    int r = fs_open(&g_fs, path, FS_O_CREAT | FS_O_TRUNC | FS_O_WRONLY, &fd);
    if (r != FS_OK) {
        printf("fs: touch '%s' failed: %d\n", path, r);
        return r;
    }

    fs_close(&g_fs, &fd);
    printf("fs: created '%s'\n", path);
    return FS_OK;
}

static int cmd_fs_cat(const char *path) {
    if (!g_fs_initialized) {
        printf("fs: filesystem not initialized/mounted\n");
        return FS_ERR_INVALID_ARG;
    }

    struct fs_file fd;
    int r = fs_open(&g_fs, path, FS_O_RDONLY, &fd);
    if (r != FS_OK) {
        printf("fs: open '%s' failed: %d\n", path, r);
        return r;
    }

    uint8_t buf[128];
    for (;;) {
        int n = fs_read(&g_fs, &fd, buf, sizeof(buf));
        if (n < 0) {
            printf("fs: read error: %d\n", n);
            fs_close(&g_fs, &fd);
            return n;
        }

        if (n == 0) break;
        fwrite(buf, 1, (size_t)n, stdout);
    }

    printf("\n");
    fs_close(&g_fs, &fd);
    return FS_OK;
}

static int cmd_fs_write_str(const char *path, const char *str) {
    if (!g_fs_initialized) {
        printf("fs: filesystem not initialized/mounted\n");
        return FS_ERR_INVALID_ARG;
    }

    struct fs_file fd;
    int r = fs_open(&g_fs, path, FS_O_CREAT | FS_O_TRUNC | FS_O_WRONLY, &fd);
    if (r != FS_OK) {
        printf("fs: open '%s' failed: %d\n", path, r);
        return r;
    }

    uint32_t len = (uint32_t)strlen(str);
    int n = fs_write(&g_fs, &fd, (const uint8_t *)str, len);
    if (n < 0) {
        printf("fs: write error: %d\n", n);
        fs_close(&g_fs, &fd);
        return n;
    }

    fs_close(&g_fs, &fd);
    printf("fs: wrote %u bytes to '%s'\n", len, path);
    return FS_OK;
}

static int cmd_fs_mkdir_cmd(const char *path) {
    if (!g_fs_initialized) {
        printf("fs: filesystem not initialized/mounted\n");
        return FS_ERR_INVALID_ARG;
    }

    int r = fs_mkdir(&g_fs, path);
    if (r != FS_OK) {
        printf("fs: mkdir '%s' failed: %d\n", path, r);
        return r;
    }

    printf("fs: created directory '%s'\n", path);
    return FS_OK;
}

static int cmd_fs_ls(const char *path) {
    if (!g_fs_initialized) {
        printf("fs: filesystem not initialized/mounted\n");
        return FS_ERR_INVALID_ARG;
    }

    const char *p = path && path[0] ? path : "/";
    struct fs_file dir;
    int r = fs_opendir(&g_fs, p, &dir);
    if (r != FS_OK) {
        printf("fs: opendir '%s' failed: %d\n", p, r);
        return r;
    }

    printf("Listing '%s':\n", p);
    struct fs_dirent de;
    int count = 0;
    while ((r = fs_readdir(&g_fs, &dir, &de)) == FS_OK) {
        const char *type = (de.type == 2) ? "dir" : "file";
        printf("  ino=%u type=%-4s\n", de.inode_num, type);
        count++;
    }

    if (r != FS_ERR_NOT_FOUND && r != FS_OK) {
        printf("fs: readdir error: %d\n", r);
    }

    printf("fs: %d entries\n", count);
    fs_close(&g_fs, &dir);
    return FS_OK;
}

/* ===== DELETE/REMOVE OPERATIONS ===== */

static int cmd_fs_rm(const char *path) {
    if (!g_fs_initialized) {
        printf("fs: filesystem not initialized/mounted\n");
        return FS_ERR_INVALID_ARG;
    }

    int r = fs_unlink(&g_fs, path);
    if (r != FS_OK) {
        printf("fs: rm '%s' failed: %d\n", path, r);
        return r;
    }

    printf("fs: deleted file '%s'\n", path);
    return FS_OK;
}

static int cmd_fs_rmdir(const char *path) {
    if (!g_fs_initialized) {
        printf("fs: filesystem not initialized/mounted\n");
        return FS_ERR_INVALID_ARG;
    }

    // Try to unlink directory (must be empty)
    int r = fs_unlink(&g_fs, path);
    if (r == FS_ERR_NOT_DIRECTORY) {
        printf("fs: '%s' is not a directory\n", path);
        return r;
    } else if (r != FS_OK) {
        printf("fs: rmdir '%s' failed: %d\n", path, r);
        return r;
    }

    printf("fs: removed directory '%s'\n", path);
    return FS_OK;
}

/* ===== STAT OPERATION ===== */

static int cmd_fs_stat(const char *path) {
    if (!g_fs_initialized) {
        printf("fs: filesystem not initialized/mounted\n");
        return FS_ERR_INVALID_ARG;
    }

    // Open file to get inode info
    struct fs_file fd;
    int r = fs_open(&g_fs, path, FS_O_RDONLY, &fd);
    if (r != FS_OK) {
        printf("fs: stat '%s' failed: %d\n", path, r);
        return r;
    }

    // Load inode for detailed information
    struct fs_inode inode;
    r = fs_load_inode(&g_fs, fd.inode_num, &inode);
    if (r != FS_OK) {
        printf("fs: load inode failed: %d\n", r);
        fs_close(&g_fs, &fd);
        return r;
    }

    const char *type = (inode.mode & FS_MODE_DIR) ? "directory" : "file";
    printf("File: %s\n", path);
    printf("  Type: %s\n", type);
    printf("  Inode: %u\n", inode.inode_num);
    printf("  Size: %u bytes\n", inode.size);
    printf("  Mode: 0x%04X\n", inode.mode);
    printf("  Links: %u\n", inode.link_count);
    printf("  Parent: %u\n", inode.parent_inode);
    printf("  Created: %u\n", inode.ctime);
    printf("  Modified: %u\n", inode.mtime);
    printf("  Accessed: %u\n", inode.atime);

    fs_close(&g_fs, &fd);
    return FS_OK;
}

/* ===== FIND OPERATION ===== */

static int cmd_fs_find(const char *path, const char *pattern) {
    if (!g_fs_initialized) {
        printf("fs: filesystem not initialized/mounted\n");
        return FS_ERR_INVALID_ARG;
    }

    const char *p = path && path[0] ? path : "/";
    struct fs_file dir;
    int r = fs_opendir(&g_fs, p, &dir);
    if (r != FS_OK) {
        printf("fs: opendir '%s' failed: %d\n", p, r);
        return r;
    }

    printf("Searching in '%s':\n", p);
    struct fs_dirent de;
    int count = 0;
    while ((r = fs_readdir(&g_fs, &dir, &de)) == FS_OK) {
        const char *type = (de.type == 2) ? "dir" : "file";
        
        // If pattern given, only show matches
        if (pattern && strstr((char*)&de.inode_num, pattern) == NULL) {
            continue;
        }

        printf("  [%s] ino=%u\n", type, de.inode_num);
        count++;
    }

    if (r != FS_ERR_NOT_FOUND && r != FS_OK) {
        printf("fs: readdir error: %d\n", r);
    }

    printf("fs: %d matches\n", count);
    fs_close(&g_fs, &dir);
    return FS_OK;
}

/* ===== SHELL ENTRY POINT ===== */

int cmd_fs(int argc, char **argv) {
    if (argc < 2) {
        cmd_fs_usage();
        return -1;
    }

    const char *sub = argv[1];

    if (strcmp(sub, "init") == 0) {
        if (argc < 3) {
            printf("fs: missing <blocks>\n");
            cmd_fs_usage();
            return -1;
        }
        uint32_t blocks = (uint32_t)strtoul(argv[2], NULL, 0);
        return cmd_fs_init(blocks);
    }

    if (strcmp(sub, "mount") == 0) {
        return cmd_fs_mount();
    }

    if (strcmp(sub, "fsck") == 0) {
        return cmd_fs_fsck();
    }

    if (strcmp(sub, "sync") == 0) {
        return cmd_fs_sync();
    }

    if (strcmp(sub, "info") == 0) {
        return cmd_fs_info();
    }

    if (strcmp(sub, "status") == 0) {
        return cmd_fs_status();
    }

    if (strcmp(sub, "touch") == 0) {
        if (argc < 3) {
            printf("Usage: fs touch <path>\n");
            return -1;
        }
        return cmd_fs_touch(argv[2]);
    }

    if (strcmp(sub, "cat") == 0) {
        if (argc < 3) {
            printf("Usage: fs cat <path>\n");
            return -1;
        }
        return cmd_fs_cat(argv[2]);
    }

    if (strcmp(sub, "write") == 0) {
        if (argc < 4) {
            printf("Usage: fs write <path> <string>\n");
            return -1;
        }
        return cmd_fs_write_str(argv[2], argv[3]);
    }

    if (strcmp(sub, "mkdir") == 0) {
        if (argc < 3) {
            printf("Usage: fs mkdir <path>\n");
            return -1;
        }
        return cmd_fs_mkdir_cmd(argv[2]);
    }

    if (strcmp(sub, "ls") == 0) {
        const char *p = (argc >= 3) ? argv[2] : "/";
        return cmd_fs_ls(p);
    }

    if (strcmp(sub, "rm") == 0) {
        if (argc < 3) {
            printf("Usage: fs rm <path>\n");
            return -1;
        }
        return cmd_fs_rm(argv[2]);
    }

    if (strcmp(sub, "rmdir") == 0) {
        if (argc < 3) {
            printf("Usage: fs rmdir <path>\n");
            return -1;
        }
        return cmd_fs_rmdir(argv[2]);
    }

    if (strcmp(sub, "stat") == 0) {
        if (argc < 3) {
            printf("Usage: fs stat <path>\n");
            return -1;
        }
        return cmd_fs_stat(argv[2]);
    }

    if (strcmp(sub, "find") == 0) {
        const char *p = (argc >= 3) ? argv[2] : "/";
        const char *pat = (argc >= 4) ? argv[3] : NULL;
        return cmd_fs_find(p, pat);
    }

    cmd_fs_usage();
    return -1;
}