// SPDX-License-Identifier: GPL-2.0-or-later
#define _POSIX_C_SOURCE 200809L
#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif
#include "mpc_save.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int sync_fd(int fd)
{
    int result;
    do result = fsync(fd); while (result < 0 && errno == EINTR);
    return result;
}

int mpc_save_close(FILE *stream, int write_failed)
{
    if (!stream) { errno = EINVAL; return -1; }
    int error = (write_failed || ferror(stream)) ? (errno ? errno : EIO) : 0;
    if (fflush(stream) != 0 && !error) error = errno ? errno : EIO;
    if (!error) {
        int fd = fileno(stream);
        if (fd < 0 || sync_fd(fd) < 0) error = errno ? errno : EIO;
    }
    // fclose still owns the stream, even when an earlier check failed.
    if (fclose(stream) != 0 && !error) error = errno ? errno : EIO;
    if (error) { errno = error; return -1; }
    return 0;
}

int mpc_save_replace(const char *temp, const char *final)
{
    if (!temp || !*temp || !final || !*final) { errno = EINVAL; return -1; }
    const char *slash = strrchr(final, '/');
    size_t length = slash && slash != final ? (size_t)(slash - final) : 1;
    char *parent = malloc(length + 1);
    if (!parent) { errno = ENOMEM; return -1; }
    if (slash) memcpy(parent, final, length);
    else parent[0] = '.';
    parent[length] = '\0';
    // Open before publication, so lookup/permission failures retain both files.
    int directory_fd = open(parent, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    int error = errno;
    free(parent);
    if (directory_fd < 0) { errno = error; return -1; }
    if (rename(temp, final) < 0) {
        error = errno;
        close(directory_fd);
        errno = error;
        return -1;
    }
    // Publication already happened. A later error cannot promise the old slot.
    error = sync_fd(directory_fd) < 0 ? (errno ? errno : EIO) : 0;
    if (close(directory_fd) < 0 && !error) error = errno ? errno : EIO;
    if (error) { errno = error; return -1; }
    return 0;
}
