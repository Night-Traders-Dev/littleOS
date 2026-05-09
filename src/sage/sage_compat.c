#include <stdint.h>
#include <time.h>

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#endif

int sage_pico_nanosleep(const struct timespec* req, struct timespec* rem) {
    (void)rem;

    if (req == NULL) {
        return -1;
    }

#ifdef PICO_BUILD
    uint64_t total_us = ((uint64_t)req->tv_sec * 1000000ULL) +
                        ((uint64_t)req->tv_nsec / 1000ULL);
    sleep_us(total_us);
    return 0;
#else
    (void)req;
    return -1;
#endif
}

#ifdef PICO_BUILD
int clock_gettime(int clk_id, struct timespec *tp) {
    (void)clk_id; // Usually CLOCK_MONOTONIC
    if (!tp) return -1;
    
    uint64_t us = time_us_64();
    tp->tv_sec = us / 1000000;
    tp->tv_nsec = (us % 1000000) * 1000;
    return 0;
}
#endif
