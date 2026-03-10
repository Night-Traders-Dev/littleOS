/* pkg.h - Package/Module Manager for littleOS */
#ifndef LITTLEOS_PKG_H
#define LITTLEOS_PKG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PKG_MAX_INSTALLED   16
#define PKG_NAME_LEN        24
#define PKG_VERSION_LEN     8
#define PKG_DESC_LEN        64

typedef enum {
    PKG_TYPE_SCRIPT = 0,
    PKG_TYPE_PIO,
    PKG_TYPE_CONFIG,
    PKG_TYPE_MODULE
} pkg_type_t;

typedef struct {
    char name[PKG_NAME_LEN];
    char version[PKG_VERSION_LEN];
    char description[PKG_DESC_LEN];
    pkg_type_t type;
    uint16_t size;
    bool installed;
    const char *code;       /* Embedded source code */
} pkg_entry_t;

int  pkg_init(void);
int  pkg_list_available(char *buf, size_t buflen);
int  pkg_list_installed(char *buf, size_t buflen);
int  pkg_install(const char *name);
int  pkg_remove(const char *name);
int  pkg_info(const char *name, pkg_entry_t *info);
int  pkg_run(const char *name);
int  pkg_search(const char *keyword, char *buf, size_t buflen);
int  pkg_get_available_count(void);
int  pkg_get_installed_count(void);

#ifdef __cplusplus
}
#endif

#endif /* LITTLEOS_PKG_H */
