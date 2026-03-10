/* pkg.c - Package/Module Manager for littleOS */
#include <stdio.h>
#include <string.h>
#include "pkg.h"

/* Embedded package scripts */
static const char pkg_hello_code[] =
    "print(\"Hello from littleOS package system!\");\n"
    "print(\"This script was installed via pkg.\");\n";

static const char pkg_sysinfo_code[] =
    "var mem = sys_free_memory();\n"
    "print(\"System Information\");\n"
    "print(\"  Free memory: \" + str(mem) + \" bytes\");\n"
    "print(\"  Uptime: \" + str(sys_uptime_ms() / 1000) + \" seconds\");\n";

static const char pkg_tempmon_code[] =
    "print(\"Temperature Monitor\");\n"
    "var raw = adc_read(4);\n"
    "var voltage = raw * 3.3 / 4096;\n"
    "var temp = 27 - (voltage - 0.706) / 0.001721;\n"
    "print(\"  ADC raw: \" + str(raw));\n"
    "print(\"  Temperature: \" + str(temp) + \" C\");\n";

static const char pkg_benchmark_code[] =
    "var start = sys_uptime_ms();\n"
    "var sum = 0;\n"
    "var i = 0;\n"
    "while (i < 10000) {\n"
    "  sum = sum + i;\n"
    "  i = i + 1;\n"
    "}\n"
    "var elapsed = sys_uptime_ms() - start;\n"
    "print(\"Benchmark: 10000 iterations in \" + str(elapsed) + \" ms\");\n"
    "print(\"Sum: \" + str(sum));\n";

static const char pkg_blink_code[] =
    "print(\"Blink LED (GP25) 5 times\");\n"
    "var i = 0;\n"
    "gpio_init(25);\n"
    "gpio_set_dir(25, 1);\n"
    "while (i < 5) {\n"
    "  gpio_put(25, 1);\n"
    "  sleep(200);\n"
    "  gpio_put(25, 0);\n"
    "  sleep(200);\n"
    "  i = i + 1;\n"
    "}\n"
    "print(\"Done!\");\n";

static const char pkg_uptime_code[] =
    "var ms = sys_uptime_ms();\n"
    "var secs = ms / 1000;\n"
    "var mins = secs / 60;\n"
    "var hours = mins / 60;\n"
    "print(\"Uptime: \" + str(hours) + \"h \" + str(mins % 60) + \"m \" + str(secs % 60) + \"s\");\n";

/* Built-in package registry */
static pkg_entry_t pkg_registry[] = {
    { "hello",     "1.0.0", "Hello World demo script",       PKG_TYPE_SCRIPT, 0, false, pkg_hello_code },
    { "sysinfo",   "1.0.0", "System information display",    PKG_TYPE_SCRIPT, 0, false, pkg_sysinfo_code },
    { "tempmon",   "1.0.0", "Temperature monitor (ADC4)",    PKG_TYPE_SCRIPT, 0, false, pkg_tempmon_code },
    { "benchmark", "1.0.0", "CPU benchmark test",            PKG_TYPE_SCRIPT, 0, false, pkg_benchmark_code },
    { "blink",     "1.0.0", "Blink onboard LED",             PKG_TYPE_SCRIPT, 0, false, pkg_blink_code },
    { "uptime",    "1.0.0", "Display formatted uptime",      PKG_TYPE_SCRIPT, 0, false, pkg_uptime_code },
};

#define PKG_REGISTRY_COUNT (sizeof(pkg_registry) / sizeof(pkg_registry[0]))

static pkg_entry_t *find_pkg(const char *name) {
    for (size_t i = 0; i < PKG_REGISTRY_COUNT; i++) {
        if (strcmp(pkg_registry[i].name, name) == 0)
            return &pkg_registry[i];
    }
    return NULL;
}

int pkg_init(void) {
    /* Calculate sizes */
    for (size_t i = 0; i < PKG_REGISTRY_COUNT; i++) {
        if (pkg_registry[i].code)
            pkg_registry[i].size = (uint16_t)strlen(pkg_registry[i].code);
    }
    return 0;
}

int pkg_list_available(char *buf, size_t buflen) {
    int pos = 0;
    pos += snprintf(buf + pos, buflen - pos,
        "  %-12s %-8s %-8s  %s\r\n", "Name", "Version", "Status", "Description");
    pos += snprintf(buf + pos, buflen - pos,
        "  %-12s %-8s %-8s  %s\r\n", "----", "-------", "------", "-----------");
    for (size_t i = 0; i < PKG_REGISTRY_COUNT && pos < (int)buflen - 80; i++) {
        pos += snprintf(buf + pos, buflen - pos, "  %-12s %-8s %-8s  %s\r\n",
            pkg_registry[i].name, pkg_registry[i].version,
            pkg_registry[i].installed ? "[inst]" : "[avail]",
            pkg_registry[i].description);
    }
    return pos;
}

int pkg_list_installed(char *buf, size_t buflen) {
    int pos = 0;
    int count = 0;
    for (size_t i = 0; i < PKG_REGISTRY_COUNT && pos < (int)buflen - 80; i++) {
        if (pkg_registry[i].installed) {
            pos += snprintf(buf + pos, buflen - pos, "  %-12s %-8s  %s\r\n",
                pkg_registry[i].name, pkg_registry[i].version,
                pkg_registry[i].description);
            count++;
        }
    }
    if (count == 0)
        pos += snprintf(buf + pos, buflen - pos, "  (no packages installed)\r\n");
    return pos;
}

int pkg_install(const char *name) {
    pkg_entry_t *pkg = find_pkg(name);
    if (!pkg) return -1;
    if (pkg->installed) return -2;

    /* For script packages, save via script storage */
#ifdef SAGE_ENABLED
    extern int script_save(const char *name, const char *code, size_t len);
    if (pkg->type == PKG_TYPE_SCRIPT && pkg->code) {
        int rc = script_save(pkg->name, pkg->code, strlen(pkg->code));
        if (rc < 0) return -3;
    }
#endif

    pkg->installed = true;
    return 0;
}

int pkg_remove(const char *name) {
    pkg_entry_t *pkg = find_pkg(name);
    if (!pkg) return -1;
    if (!pkg->installed) return -2;

#ifdef SAGE_ENABLED
    extern int script_delete(const char *name);
    if (pkg->type == PKG_TYPE_SCRIPT)
        script_delete(pkg->name);
#endif

    pkg->installed = false;
    return 0;
}

int pkg_info(const char *name, pkg_entry_t *info) {
    pkg_entry_t *pkg = find_pkg(name);
    if (!pkg) return -1;
    memcpy(info, pkg, sizeof(pkg_entry_t));
    return 0;
}

int pkg_run(const char *name) {
    pkg_entry_t *pkg = find_pkg(name);
    if (!pkg) return -1;
    if (!pkg->code) return -2;

#ifdef SAGE_ENABLED
    extern void *sage_ctx;
    typedef int sage_result_t;
    extern sage_result_t sage_eval_string(void *ctx, const char *code, size_t len);
    if (sage_ctx) {
        sage_result_t rc = sage_eval_string(sage_ctx, pkg->code, strlen(pkg->code));
        return (rc == 0) ? 0 : -3;
    }
#endif
    printf("SageLang not available\r\n");
    return -4;
}

int pkg_search(const char *keyword, char *buf, size_t buflen) {
    int pos = 0;
    for (size_t i = 0; i < PKG_REGISTRY_COUNT && pos < (int)buflen - 80; i++) {
        if (strstr(pkg_registry[i].name, keyword) ||
            strstr(pkg_registry[i].description, keyword)) {
            pos += snprintf(buf + pos, buflen - pos, "  %-12s  %s\r\n",
                pkg_registry[i].name, pkg_registry[i].description);
        }
    }
    if (pos == 0)
        pos += snprintf(buf + pos, buflen - pos, "  No packages matching '%s'\r\n", keyword);
    return pos;
}

int pkg_get_available_count(void) { return (int)PKG_REGISTRY_COUNT; }

int pkg_get_installed_count(void) {
    int count = 0;
    for (size_t i = 0; i < PKG_REGISTRY_COUNT; i++)
        if (pkg_registry[i].installed) count++;
    return count;
}
