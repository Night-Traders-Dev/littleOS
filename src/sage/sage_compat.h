#ifndef LITTLEOS_SAGE_COMPAT_H
#define LITTLEOS_SAGE_COMPAT_H

#ifdef PICO_BUILD

struct timespec;
int sage_pico_nanosleep(const struct timespec* req, struct timespec* rem);

#define nanosleep sage_pico_nanosleep

#endif

#endif
