#ifndef LITTLEOS_SAGE_COMPAT_H
#define LITTLEOS_SAGE_COMPAT_H

#ifdef PICO_BUILD

struct timespec;
int sage_pico_nanosleep(const struct timespec* req, struct timespec* rem);

#define nanosleep sage_pico_nanosleep

// Shims for dirent.h (not supported by bare-metal newlib on RISC-V)
#ifndef _DIRENT_H_
#define _DIRENT_H_

typedef void DIR;
struct dirent {
    char d_name[256];
};

static inline DIR* opendir(const char* name) { (void)name; return (DIR*)0; }
static inline struct dirent* readdir(DIR* dirp) { (void)dirp; return (struct dirent*)0; }
static inline int closedir(DIR* dirp) { (void)dirp; return -1; }

#endif // _DIRENT_H_

// Shim for clock_gettime
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

int clock_gettime(int clk_id, struct timespec *tp);

#endif

#endif
