#include <errno.h>
#include <stddef.h>
#include <sys/stat.h>

/* newlib's __libc_init_array calls these. They normally come from crti,
 * which -nostartfiles leaves out. */
void _init(void) {}

void _fini(void) {}

// The image does not have a heap. A call that reaches _sbrk is a defect.
void* _sbrk(ptrdiff_t increment) {
    (void)increment;
    errno = ENOMEM;
    return (void*)-1;
}

int _write(int fd, char* buffer, int length) {
    (void)fd;
    (void)buffer;
    return length;
}

int _read(int fd, char* buffer, int length) {
    (void)fd;
    (void)buffer;
    (void)length;
    return -1;
}

int _close(int fd) {
    (void)fd;
    return -1;
}

int _fstat(int fd, struct stat* status) {
    (void)fd;
    if (status != NULL) {
        status->st_mode = S_IFCHR;
    }
    return 0;
}

int _isatty(int fd) {
    (void)fd;
    return 1;
}

int _lseek(int fd, int offset, int whence) {
    (void)fd;
    (void)offset;
    (void)whence;
    return -1;
}

int _kill(int pid, int signal) {
    (void)pid;
    (void)signal;
    errno = EINVAL;
    return -1;
}

int _getpid(void) { return 1; }

void _exit(int status) {
    (void)status;
    while (1) {
    }
}
