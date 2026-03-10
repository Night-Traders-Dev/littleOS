/* cmd_pkg.c - Package manager shell commands */
#include <stdio.h>
#include <string.h>
#include "pkg.h"

int cmd_pkg(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: pkg <command> [args]\r\n");
        printf("Commands:\r\n");
        printf("  list              - List available packages\r\n");
        printf("  installed         - List installed packages\r\n");
        printf("  install NAME      - Install a package\r\n");
        printf("  remove NAME       - Remove a package\r\n");
        printf("  info NAME         - Show package details\r\n");
        printf("  run NAME          - Run a package\r\n");
        printf("  search KEYWORD    - Search packages\r\n");
        return 0;
    }

    if (strcmp(argv[1], "list") == 0) {
        printf("=== Available Packages (%d) ===\r\n", pkg_get_available_count());
        char buf[1024];
        pkg_list_available(buf, sizeof(buf));
        printf("%s", buf);
        return 0;
    }

    if (strcmp(argv[1], "installed") == 0) {
        printf("=== Installed Packages (%d) ===\r\n", pkg_get_installed_count());
        char buf[512];
        pkg_list_installed(buf, sizeof(buf));
        printf("%s", buf);
        return 0;
    }

    if (strcmp(argv[1], "install") == 0) {
        if (argc < 3) {
            printf("Usage: pkg install <name>\r\n");
            return 1;
        }
        int rc = pkg_install(argv[2]);
        switch (rc) {
            case 0:  printf("Installed '%s'\r\n", argv[2]); break;
            case -1: printf("Package '%s' not found\r\n", argv[2]); break;
            case -2: printf("Package '%s' already installed\r\n", argv[2]); break;
            default: printf("Install failed: %d\r\n", rc); break;
        }
        return rc;
    }

    if (strcmp(argv[1], "remove") == 0) {
        if (argc < 3) {
            printf("Usage: pkg remove <name>\r\n");
            return 1;
        }
        int rc = pkg_remove(argv[2]);
        switch (rc) {
            case 0:  printf("Removed '%s'\r\n", argv[2]); break;
            case -1: printf("Package '%s' not found\r\n", argv[2]); break;
            case -2: printf("Package '%s' not installed\r\n", argv[2]); break;
            default: printf("Remove failed: %d\r\n", rc); break;
        }
        return rc;
    }

    if (strcmp(argv[1], "info") == 0) {
        if (argc < 3) {
            printf("Usage: pkg info <name>\r\n");
            return 1;
        }
        pkg_entry_t info;
        int rc = pkg_info(argv[2], &info);
        if (rc < 0) {
            printf("Package '%s' not found\r\n", argv[2]);
            return 1;
        }
        printf("Package: %s\r\n", info.name);
        printf("  Version:     %s\r\n", info.version);
        printf("  Type:        %s\r\n",
            info.type == PKG_TYPE_SCRIPT ? "script" :
            info.type == PKG_TYPE_PIO ? "pio" :
            info.type == PKG_TYPE_CONFIG ? "config" : "module");
        printf("  Size:        %u bytes\r\n", info.size);
        printf("  Description: %s\r\n", info.description);
        printf("  Installed:   %s\r\n", info.installed ? "yes" : "no");
        return 0;
    }

    if (strcmp(argv[1], "run") == 0) {
        if (argc < 3) {
            printf("Usage: pkg run <name>\r\n");
            return 1;
        }
        int rc = pkg_run(argv[2]);
        if (rc < 0) {
            if (rc == -1) printf("Package '%s' not found\r\n", argv[2]);
            else if (rc == -4) printf("SageLang not available\r\n");
            else printf("Run failed: %d\r\n", rc);
        }
        return rc;
    }

    if (strcmp(argv[1], "search") == 0) {
        if (argc < 3) {
            printf("Usage: pkg search <keyword>\r\n");
            return 1;
        }
        char buf[512];
        pkg_search(argv[2], buf, sizeof(buf));
        printf("Search results for '%s':\r\n%s", argv[2], buf);
        return 0;
    }

    printf("Unknown subcommand: %s\r\n", argv[1]);
    return 1;
}
