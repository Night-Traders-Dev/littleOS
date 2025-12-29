#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fs.h"

/* Simple RAM “block device” backend */

struct ram_backend {
    uint8_t  *data;
    uint32_t  blocks;  /* number of 512-byte blocks */
};

static struct ram_backend g_rb = {0};
static struct fs          g_fs = {0};
static bool               g_fs_initialized = false;
static bool               g_fs_mounted     = false;

/* Backend functions */

static int ram_read_block(void *ctx, uint32_t block_addr, uint8_t *buf)
{
    struct ram_backend *rb = (struct ram_backend *)ctx;

    if (!rb || !buf) return FS_ERR_INVALID_ARG;
    if (block_addr >= rb->blocks) return FS_ERR_INVALID_BLOCK;

    memcpy(buf, rb->data + (size_t)block_addr * FS_BLOCK_SIZE, FS_BLOCK_SIZE);
    return FS_OK;
}

static int ram_write_block(void *ctx, uint32_t block_addr, const uint8_t *buf)
{
    struct ram_backend *rb = (struct ram_backend *)ctx;

    if (!rb || !buf) return FS_ERR_INVALID_ARG;
    if (block_addr >= rb->blocks) return FS_ERR_INVALID_BLOCK;

    memcpy(rb->data + (size_t)block_addr * FS_BLOCK_SIZE, buf, FS_BLOCK_SIZE);
    return FS_OK;
}

/* Optional erase – for RAM we just ignore (no-op) */
static int ram_erase_sector(void *ctx, uint32_t sector_addr)
{
    (void)sector_addr;
    (void)ctx;
    return FS_OK;
}

/* Pretty-print minimal superblock state */
static void dump_superblock(const struct fs_superblock *sb)
{
    printf("Superblock:\n");
    printf("  magic          = 0x%04X\n", sb->magic);
    printf("  version        = %u\n", sb->version);
    printf("  block_size     = %u\n", sb->block_size);
    printf("  segment_size   = %u\n", sb->segment_size);
    printf("  total_blocks   = %u\n", sb->total_blocks);
    printf("  total_segments = %u\n", sb->total_segments);
    printf("  total_inodes   = %u\n", sb->total_inodes);
    printf("  root_inode     = %u\n", sb->root_inode);
    printf("  nat_start      = %u (blocks=%u)\n",
           sb->nat_start_block, sb->nat_blocks);
    printf("  sit_start      = %u (blocks=%u)\n",
           sb->sit_start_block, sb->sit_blocks);
    printf("  main_start     = %u\n", sb->main_start_block);
    printf("  mount_count    = %u\n", sb->mount_count);
}

/* Helpers */

static void cmd_fs_usage(void)
{
    printf("Usage:\n");
    printf("  fs init <blocks>   - allocate RAM and format filesystem\n");
    printf("  fs mount           - mount from RAM backend\n");
    printf("  fs fsck            - run fs_fsck()\n");
    printf("  fs sync            - fs_sync()\n");
    printf("  fs info            - print superblock info\n");
}

/* Initialize RAM backend + format filesystem */
static int cmd_fs_init(uint32_t blocks)
{
    if (g_rb.data) {
        free(g_rb.data);
        g_rb.data = NULL;
        g_rb.blocks = 0;
    }

    g_rb.blocks = blocks;
    g_rb.data = (uint8_t *)malloc((size_t)blocks * FS_BLOCK_SIZE);
    if (!g_rb.data) {
        printf("fs: RAM backend allocation failed\n");
        g_rb.blocks = 0;
        return FS_ERR_NO_SPACE;
    }

    memset(g_rb.data, 0, (size_t)blocks * FS_BLOCK_SIZE);
    memset(&g_fs, 0, sizeof(g_fs));

    fs_set_storage_backend(&g_fs, &g_rb,
                           ram_read_block,
                           ram_write_block,
                           ram_erase_sector);

    int r = fs_format(&g_fs, blocks);
    if (r != FS_OK) {
        printf("fs: fs_format failed: %d\n", r);
        free(g_rb.data);
        g_rb.data = NULL;
        g_rb.blocks = 0;
        return r;
    }

    r = fs_sync(&g_fs);
    if (r != FS_OK) {
        printf("fs: fs_sync after format failed: %d\n", r);
        free(g_rb.data);
        g_rb.data = NULL;
        g_rb.blocks = 0;
        return r;
    }

    g_fs_initialized = true;
    g_fs_mounted = false;
    printf("fs: formatted RAM FS (%u blocks, %u bytes)\n",
           blocks, blocks * FS_BLOCK_SIZE);
    return FS_OK;
}

/* Mount using a fresh fs struct (simulating reboot) */
static int cmd_fs_mount(void)
{
    if (!g_rb.data || g_rb.blocks == 0) {
        printf("fs: backend not initialized, run 'fs init <blocks>' first\n");
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
    printf("fs: mounted\n");
    return FS_OK;
}

static int cmd_fs_fsck(void)
{
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

static int cmd_fs_sync(void)
{
    if (!g_fs_initialized) {
        printf("fs: not initialized/mounted\n");
        return FS_ERR_INVALID_ARG;
    }

    int r = fs_sync(&g_fs);
    if (r != FS_OK) {
        printf("fs: fs_sync failed: %d\n", r);
        return r;
    }

    printf("fs: sync OK\n");
    return FS_OK;
}

static int cmd_fs_info(void)
{
    if (!g_fs_initialized) {
        printf("fs: not initialized/mounted\n");
        return FS_ERR_INVALID_ARG;
    }

    dump_superblock(&g_fs.sb);
    printf("Runtime:\n");
    printf("  free_blocks = %u\n", g_fs.free_blocks_count);
    printf("  active_cp   = %u\n", (unsigned)g_fs.active_cp);
    printf("  mounted     = %s\n", g_fs_mounted ? "yes" : "no");
    return FS_OK;
}

/* Shell entry point:
 *   argv[0] = "fs"
 *   argv[1] = subcommand
 */
int cmd_fs(int argc, char **argv)
{
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

    cmd_fs_usage();
    return -1;
}
