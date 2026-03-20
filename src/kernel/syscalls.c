/* syscalls.c - Newlib syscall stubs for bare-metal RP2040/RP2350
 *
 * Newlib's reentrant wrappers (_stat_r, _unlink_r) reference _stat and
 * _unlink even when nothing in the application calls them directly.
 * Without these stubs the linker emits "warning: _stat/_unlink is not
 * implemented and will always fail".
 *
 * These must be strong (not weak) symbols so they override newlib's
 * internal warning stubs which are linked from libg.a after our objects. */

#include <sys/stat.h>
#include <errno.h>

int _stat(const char *path, struct stat *buf) {
    (void)path;
    (void)buf;
    errno = ENOSYS;
    return -1;
}

int _unlink(const char *path) {
    (void)path;
    errno = ENOSYS;
    return -1;
}
