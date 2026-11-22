// SPDX-License-Identifier: MIT
//
// Minimal newlib system-call layer (C23). The firmware does no file I/O and no heap
// allocation; these stubs exist only so that newlib's libc and libm link without
// libnosys, and every one of them fails loudly rather than silently succeeding.

#include <errno.h>
#include <stddef.h>
#include <sys/stat.h>

int _close(int fd);
int _fstat(int fd, struct stat* st);
int _isatty(int fd);
int _lseek(int fd, int offset, int whence);
int _read(int fd, char* buffer, int length);
int _write(int fd, const char* buffer, int length);
int _kill(int pid, int signal);
int _getpid(void);
void* _sbrk(ptrdiff_t increment);
[[noreturn]] void _exit(int code);
[[noreturn]] extern void ebc_platform_exit(int code);

int _close(int fd) { (void)fd; errno = EBADF; return -1; }

int _fstat(int fd, struct stat* st) {
  (void)fd;
  st->st_mode = S_IFCHR;
  return 0;
}

int _isatty(int fd) { (void)fd; return 1; }
int _lseek(int fd, int offset, int whence) { (void)fd; (void)offset; (void)whence; return 0; }
int _read(int fd, char* buffer, int length) { (void)fd; (void)buffer; (void)length; return 0; }
int _write(int fd, const char* buffer, int length) {
  (void)fd; (void)buffer;
  return length;
}
int _kill(int pid, int signal) { (void)pid; (void)signal; errno = EINVAL; return -1; }
int _getpid(void) { return 1; }

// No heap: the real-time image must not allocate. A link against malloc still
// works, but every allocation fails, which a test would notice immediately.
void* _sbrk(ptrdiff_t increment) {
  (void)increment;
  errno = ENOMEM;
  return (void*)-1;
}

[[noreturn]] void _exit(int code) { ebc_platform_exit(code); }
