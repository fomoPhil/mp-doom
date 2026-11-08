// SPDX-License-Identifier: GPL-2.0-or-later
#define _POSIX_C_SOURCE 200809L
#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "mpc_save.h"

static char directory[PATH_MAX];

static void path(char *result, const char *name)
{
    int size = snprintf(result, PATH_MAX, "%s/%s", directory, name);
    assert(size > 0 && size < PATH_MAX);
}

static void write_file(const char *name, const void *bytes, size_t length)
{
    FILE *stream = fopen(name, "wb");
    assert(stream && fwrite(bytes, 1, length, stream) == length);
    assert(fclose(stream) == 0);
}

static void expect_file(const char *name, const void *bytes, size_t length)
{
    unsigned char buffer[128];
    assert(length <= sizeof buffer);
    FILE *stream = fopen(name, "rb");
    assert(stream && fread(buffer, 1, sizeof buffer, stream) == length);
    assert(!memcmp(buffer, bytes, length) && fclose(stream) == 0);
}

static void expect_closed(int fd)
{
    errno = 0;
    assert(fcntl(fd, F_GETFD) == -1 && errno == EBADF);
}

static void complete_replacement(void)
{
    char temp[PATH_MAX], final[PATH_MAX];
    path(temp, "complete-temp.dsg"); path(final, "complete-final.dsg");
    const unsigned char contents[] = {1, 2, 0, 4, 255, 0x1d};
    write_file(final, "previous", 8);
    FILE *stream = fopen(temp, "wb"); assert(stream);
    int fd = fileno(stream);
    assert(fwrite(contents, 1, sizeof contents, stream) == sizeof contents);
    assert(mpc_save_close(stream, 0) == 0);
    expect_closed(fd);
    expect_file(temp, contents, sizeof contents);
    assert(mpc_save_replace(temp, final) == 0);
    expect_file(final, contents, sizeof contents);
    assert(access(temp, F_OK) == -1 && errno == ENOENT);
    assert(unlink(final) == 0);
}

static void write_failure_is_not_success(void)
{
    char temp[PATH_MAX]; path(temp, "partial-temp.dsg");
    FILE *stream = fopen(temp, "wb"); assert(stream);
    int fd = fileno(stream);
    assert(fwrite("partial", 1, 7, stream) == 7);
    errno = ENOSPC;
    assert(mpc_save_close(stream, 1) == -1 && errno == ENOSPC);
    expect_closed(fd);
    expect_file(temp, "partial", 7);
    assert(unlink(temp) == 0);
}

static void pipe_write_failure_closes_stream(void)
{
    int descriptors[2]; assert(pipe(descriptors) == 0);
    assert(close(descriptors[0]) == 0);
    FILE *stream = fdopen(descriptors[1], "wb"); assert(stream);
    assert(setvbuf(stream, NULL, _IOFBF, 4096) == 0);
    assert(fwrite("save", 1, 4, stream) == 4);
    errno = 0;
    assert(mpc_save_close(stream, 0) == -1 && errno == EPIPE);
    expect_closed(descriptors[1]);
}

static void existing_stream_error_is_not_success(void)
{
    int descriptors[2]; assert(pipe(descriptors) == 0);
    assert(close(descriptors[0]) == 0);
    FILE *stream = fdopen(descriptors[1], "wb"); assert(stream);
    assert(setvbuf(stream, NULL, _IOFBF, 4096) == 0);
    assert(fwrite("save", 1, 4, stream) == 4);
    assert(fflush(stream) == EOF && ferror(stream));
    errno = 0;
    assert(mpc_save_close(stream, 0) == -1 && errno == EIO);
    expect_closed(descriptors[1]);
}

static void first_error_survives_close_failure(void)
{
    int descriptors[2]; assert(pipe(descriptors) == 0);
    assert(close(descriptors[0]) == 0);
    FILE *stream = fdopen(descriptors[1], "wb"); assert(stream);
    assert(setvbuf(stream, NULL, _IOFBF, 4096) == 0);
    assert(fwrite("save", 1, 4, stream) == 4);
    errno = ENOSPC;
    assert(mpc_save_close(stream, 1) == -1 && errno == ENOSPC);
    expect_closed(descriptors[1]);
}

static void sync_failure_closes_stream(void)
{
    int descriptors[2]; assert(pipe(descriptors) == 0);
    FILE *stream = fdopen(descriptors[1], "wb"); assert(stream);
    assert(fwrite("save", 1, 4, stream) == 4);
    errno = 0;
    assert(mpc_save_close(stream, 0) == -1 && errno == EINVAL);
    expect_closed(descriptors[1]);
    char bytes[8];
    assert(read(descriptors[0], bytes, sizeof bytes) == 4 && !memcmp(bytes, "save", 4));
    assert(read(descriptors[0], bytes, sizeof bytes) == 0);
    assert(close(descriptors[0]) == 0);
}

static void rename_failure_preserves_previous_save(void)
{
    char missing[PATH_MAX], final[PATH_MAX];
    path(missing, "missing-temp.dsg"); path(final, "protected-final.dsg");
    write_file(final, "previous", 8);
    errno = 0;
    assert(mpc_save_replace(missing, final) == -1 && errno == ENOENT);
    expect_file(final, "previous", 8);
    assert(unlink(final) == 0);
}

static void missing_directory_preserves_temporary_save(void)
{
    char temp[PATH_MAX], final[PATH_MAX];
    path(temp, "retained-temp.dsg"); path(final, "missing/slot.dsg");
    write_file(temp, "new", 3);
    errno = 0;
    assert(mpc_save_replace(temp, final) == -1 && errno == ENOENT);
    expect_file(temp, "new", 3);
    assert(unlink(temp) == 0);
}

static void relative_destination_is_supported(void)
{
    int previous = open(".", O_RDONLY | O_DIRECTORY); assert(previous >= 0);
    assert(chdir(directory) == 0);
    FILE *stream = fopen("relative-temp.dsg", "wb"); assert(stream);
    assert(fwrite("complete", 1, 8, stream) == 8);
    assert(mpc_save_close(stream, 0) == 0);
    assert(mpc_save_replace("relative-temp.dsg", "relative-final.dsg") == 0);
    expect_file("relative-final.dsg", "complete", 8);
    assert(unlink("relative-final.dsg") == 0);
    assert(fchdir(previous) == 0 && close(previous) == 0);
}

int main(int argc, char **argv)
{
    signal(SIGPIPE, SIG_IGN);
    const char *tmp = getenv("TMPDIR"); if (!tmp) tmp = "/tmp";
    int size = snprintf(directory, sizeof directory, "%s/mpc-save-test-XXXXXX", tmp);
    assert(size > 0 && (size_t)size < sizeof directory && mkdtemp(directory));
    struct test { const char *name; void (*run)(void); } tests[] = {
        {"complete-replacement", complete_replacement},
        {"write-failure", write_failure_is_not_success},
        {"pipe-write-failure", pipe_write_failure_closes_stream},
        {"existing-stream-error", existing_stream_error_is_not_success},
        {"first-error", first_error_survives_close_failure},
        {"sync-failure", sync_failure_closes_stream},
        {"rename-failure", rename_failure_preserves_previous_save},
        {"missing-directory", missing_directory_preserves_temporary_save},
        {"relative-path", relative_destination_is_supported}
    };
    for (size_t i = 0; i < sizeof tests / sizeof tests[0]; ++i) {
        if (argc > 1 && strcmp(argv[1], tests[i].name)) continue;
        tests[i].run(); printf("SAVE_TEST_OK %s\n", tests[i].name);
    }
    assert(rmdir(directory) == 0);
    return 0;
}
