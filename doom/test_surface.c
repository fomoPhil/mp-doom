/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _POSIX_C_SOURCE 200809L
#include "mpc_surface.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static unsigned int checks;

#define CHECK(condition) do { \
    ++checks; \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

/* The normal build uses real pipes. A second build can substitute just
 * the implementation's POSIX calls to check errors without a device:
 * compile mpc_surface.c with -Dwrite=mpc_test_write
 * -Dreadlink=mpc_test_readlink -Dopen=mpc_test_open, then link this file
 * compiled with -DMPC_SURFACE_TEST_SYSCALLS. */
#ifdef MPC_SURFACE_TEST_SYSCALLS
enum { SCRIPT_FD = 1000001 };
struct write_step {
    ssize_t amount;
    int error;
};
static const struct write_step *write_steps;
static size_t write_step_count;
static size_t write_step_index;
static unsigned char written[64];
static size_t written_count;
static const char *link_target;
static const char *expected_device;
static int expected_flags;
static int readlink_interrupts;
static int readlink_error;
static int open_interrupts;
static int open_error;
static unsigned int open_calls;

ssize_t mpc_test_write(int fd, const void *bytes, size_t count)
{
    struct write_step step;
    if (fd != SCRIPT_FD)
        return write(fd, bytes, count);
    CHECK(write_step_index < write_step_count);
    step = write_steps[write_step_index++];
    if (step.amount > 0) {
        CHECK((size_t)step.amount <= count);
        CHECK(written_count + (size_t)step.amount <= sizeof written);
        memcpy(written + written_count, bytes, (size_t)step.amount);
        written_count += (size_t)step.amount;
    } else if (step.amount < 0) {
        errno = step.error;
    }
    return step.amount;
}

ssize_t mpc_test_readlink(const char *path, char *buffer, size_t size)
{
    size_t count;
    CHECK(strcmp(path, "/dev/snd/by-path/platform-fe670000.serial") == 0);
    if (readlink_interrupts > 0) {
        --readlink_interrupts;
        errno = EINTR;
        return -1;
    }
    if (readlink_error) {
        errno = readlink_error;
        return -1;
    }
    CHECK(link_target != NULL);
    count = strlen(link_target);
    if (count > size)
        count = size;
    memcpy(buffer, link_target, count);
    return (ssize_t)count;
}

int mpc_test_open(const char *path, int flags, ...)
{
    ++open_calls;
    CHECK(expected_device != NULL);
    CHECK(strcmp(path, expected_device) == 0);
    CHECK(flags == expected_flags);
    if (open_interrupts > 0) {
        --open_interrupts;
        errno = EINTR;
        return -1;
    }
    if (open_error) {
        errno = open_error;
        return -1;
    }
    return SCRIPT_FD;
}

static void script_writes(const struct write_step *steps, size_t count)
{
    write_steps = steps;
    write_step_count = count;
    write_step_index = 0;
    written_count = 0;
}

static void test_write_errors(void)
{
    static const struct write_step retry[] = {
        {-1, EINTR}, {2, 0}, {-1, EINTR}, {3, 0}, {7, 0}
    };
    static const struct write_step blocked[] = {{1, 0}, {-1, EAGAIN}};
    static const struct write_step zero[] = {{0, 0}};
    static const struct write_step closed[] = {{-1, EBADF}};
    static const unsigned char expected[] = {
        0xf0, 0x47, 0x7f, 0x5c, 0x65, 0, 4, 3, 8, 16, 32, 0xf7
    };

    script_writes(retry, sizeof retry / sizeof retry[0]);
    CHECK(mpc_surface_pad_color(SCRIPT_FD, 3, 8, 16, 32) == 0);
    CHECK(write_step_index == write_step_count);
    CHECK(written_count == sizeof expected);
    CHECK(memcmp(written, expected, sizeof expected) == 0);

    script_writes(blocked, sizeof blocked / sizeof blocked[0]);
    CHECK(mpc_surface_led(SCRIPT_FD, 111, 127) == -1);
    CHECK(errno == EAGAIN);
    CHECK(write_step_index == write_step_count);
    CHECK(written_count == 1 && written[0] == 0xb0);

    script_writes(zero, sizeof zero / sizeof zero[0]);
    CHECK(mpc_surface_led(SCRIPT_FD, 111, 127) == -1);
    CHECK(errno == EIO);
    CHECK(write_step_index == write_step_count);

    script_writes(closed, sizeof closed / sizeof closed[0]);
    CHECK(mpc_surface_clear(SCRIPT_FD) == -1);
    CHECK(errno == EBADF);
    CHECK(write_step_index == write_step_count);
}

static void test_discovery(void)
{
    static const char *invalid[] = {
        "", "controlC", "controlC-1", "controlC+2", "controlC1extra",
        "midiC2D0", "controlC2147483648", "controlC999999999999999999999"
    };
    char long_target[4096];
    size_t i;

    expected_flags = O_RDWR | O_NONBLOCK;
    link_target = "../controlC0";
    expected_device = "/dev/snd/midiC0D0";
    CHECK(mpc_surface_open(expected_flags) == SCRIPT_FD);
    link_target = "/dev/snd/controlC12";
    expected_device = "/dev/snd/midiC12D0";
    readlink_interrupts = 1;
    open_interrupts = 1;
    CHECK(mpc_surface_open(expected_flags) == SCRIPT_FD);
    link_target = "controlC2147483647";
    expected_device = "/dev/snd/midiC2147483647D0";
    CHECK(mpc_surface_open(expected_flags) == SCRIPT_FD);

    open_error = EACCES;
    CHECK(mpc_surface_open(expected_flags) == -1);
    CHECK(errno == EACCES);
    open_error = 0;
    open_calls = 0;
    for (i = 0; i < sizeof invalid / sizeof invalid[0]; ++i) {
        link_target = invalid[i];
        CHECK(mpc_surface_open(expected_flags) == -1);
        CHECK(errno == EINVAL);
    }
    memset(long_target, '1', sizeof long_target - 1);
    long_target[sizeof long_target - 1] = '\0';
    link_target = long_target;
    CHECK(mpc_surface_open(expected_flags) == -1);
    CHECK(errno == ENAMETOOLONG);
    readlink_error = ENOENT;
    CHECK(mpc_surface_open(expected_flags) == -1);
    CHECK(errno == ENOENT);
    readlink_error = 0;
    CHECK(open_calls == 0);
}
#endif

static void test_pads(void)
{
    static const unsigned char expected[16] = {
        37, 36, 42, 82, 40, 38, 46, 44,
        48, 47, 45, 43, 49, 55, 51, 53
    };
    int note;
    size_t pad;

    CHECK(memcmp(mpc_pad_notes, expected, sizeof expected) == 0);
    for (note = 0; note <= 255; ++note) {
        int index = -1;
        for (pad = 0; pad < sizeof expected; ++pad) {
            if (expected[pad] == note)
                index = (int)pad;
        }
        CHECK(mpc_pad_index((unsigned char)note) == index);
    }
}

static void read_bytes(int fd, unsigned char *bytes, size_t count)
{
    size_t used = 0;
    while (used < count) {
        ssize_t amount = read(fd, bytes + used, count - used);
        if (amount < 0 && errno == EINTR)
            continue;
        CHECK(amount > 0);
        used += (size_t)amount;
    }
}

static void test_output(void)
{
    static const unsigned char led[] = {0xb0, 111, 127};
    static const unsigned char color[] = {
        0xf0, 0x47, 0x7f, 0x5c, 0x65, 0x00, 0x04,
        15, 1, 64, 127, 0xf7
    };
    static const unsigned char buttons[24] = {
        6, 7, 1, 0, 49, 35, 9, 11, 12, 14, 75, 74,
        73, 55, 54, 13, 36, 40, 43, 53, 80, 81, 82, 111
    };
    unsigned char received[24 * 3 + 16 * 12];
    int fds[2];
    size_t offset;
    size_t i;

    CHECK(pipe(fds) == 0);
    CHECK(mpc_surface_led(fds[1], 111, 127) == 0);
    read_bytes(fds[0], received, sizeof led);
    CHECK(memcmp(received, led, sizeof led) == 0);
    CHECK(mpc_surface_pad_color(fds[1], 15, 1, 64, 127) == 0);
    read_bytes(fds[0], received, sizeof color);
    CHECK(memcmp(received, color, sizeof color) == 0);
    CHECK(mpc_surface_clear(fds[1]) == 0);
    read_bytes(fds[0], received, sizeof received);
    offset = 0;
    for (i = 0; i < sizeof buttons; ++i) {
        CHECK(received[offset++] == 0xb0);
        CHECK(received[offset++] == buttons[i]);
        CHECK(received[offset++] == 0);
    }
    for (i = 0; i < 16; ++i) {
        static const unsigned char prefix[] = {
            0xf0, 0x47, 0x7f, 0x5c, 0x65, 0, 4
        };
        CHECK(memcmp(received + offset, prefix, sizeof prefix) == 0);
        offset += sizeof prefix;
        CHECK(received[offset++] == i);
        CHECK(received[offset++] == 0);
        CHECK(received[offset++] == 0);
        CHECK(received[offset++] == 0);
        CHECK(received[offset++] == 0xf7);
    }
    CHECK(offset == sizeof received);
    CHECK(fcntl(fds[0], F_SETFL, O_NONBLOCK) == 0);
    CHECK(read(fds[0], received, 1) == -1);
    CHECK(errno == EAGAIN || errno == EWOULDBLOCK);
    CHECK(close(fds[0]) == 0);
    CHECK(close(fds[1]) == 0);
}

static void test_invalid_arguments(void)
{
    struct mpc_midi_parser parser = {0};
    unsigned char message[3];
    unsigned char byte;
    int fds[2];
    int bad[] = {-1, 128};
    size_t i;

    CHECK(pipe(fds) == 0);
    for (i = 0; i < sizeof bad / sizeof bad[0]; ++i) {
        errno = 0;
        CHECK(mpc_surface_led(fds[1], bad[i], 0) == -1);
        CHECK(errno == EINVAL);
        errno = 0;
        CHECK(mpc_surface_led(fds[1], 0, bad[i]) == -1);
        CHECK(errno == EINVAL);
        errno = 0;
        CHECK(mpc_surface_pad_color(fds[1], 0, bad[i], 0, 0) == -1);
        CHECK(errno == EINVAL);
        errno = 0;
        CHECK(mpc_surface_pad_color(fds[1], 0, 0, bad[i], 0) == -1);
        CHECK(errno == EINVAL);
        errno = 0;
        CHECK(mpc_surface_pad_color(fds[1], 0, 0, 0, bad[i]) == -1);
        CHECK(errno == EINVAL);
    }
    CHECK(mpc_surface_pad_color(fds[1], -1, 0, 0, 0) == -1);
    CHECK(errno == EINVAL);
    CHECK(mpc_surface_pad_color(fds[1], 16, 0, 0, 0) == -1);
    CHECK(errno == EINVAL);
    CHECK(mpc_surface_led(-1, 0, 0) == -1);
    CHECK(errno == EBADF);
    CHECK(mpc_surface_pad_color(-1, 0, 0, 0, 0) == -1);
    CHECK(errno == EBADF);
    CHECK(mpc_surface_clear(-1) == -1);
    CHECK(errno == EBADF);
    CHECK(mpc_surface_open(O_CREAT | O_RDWR) == -1);
    CHECK(errno == EINVAL);
    CHECK(mpc_midi_feed(NULL, 0, message) == -1);
    CHECK(errno == EINVAL);
    CHECK(mpc_midi_feed(&parser, 0, NULL) == -1);
    CHECK(errno == EINVAL);
    CHECK(fcntl(fds[0], F_SETFL, O_NONBLOCK) == 0);
    CHECK(read(fds[0], &byte, 1) == -1);
    CHECK(errno == EAGAIN || errno == EWOULDBLOCK);
    CHECK(close(fds[0]) == 0);
    CHECK(close(fds[1]) == 0);
}

static void test_full_pipe(void)
{
    unsigned char bytes[512] = {0};
    int fds[2];
    ssize_t amount;

    CHECK(pipe(fds) == 0);
    CHECK(fcntl(fds[1], F_SETFL, O_NONBLOCK) == 0);
    do {
        amount = write(fds[1], bytes, sizeof bytes);
    } while (amount > 0 || (amount < 0 && errno == EINTR));
    CHECK(amount == -1);
    CHECK(errno == EAGAIN || errno == EWOULDBLOCK);
    CHECK(mpc_surface_led(fds[1], 0, 1) == -1);
    CHECK(errno == EAGAIN || errno == EWOULDBLOCK);
    CHECK(mpc_surface_pad_color(fds[1], 0, 1, 2, 3) == -1);
    CHECK(errno == EAGAIN || errno == EWOULDBLOCK);
    CHECK(mpc_surface_clear(fds[1]) == -1);
    CHECK(errno == EAGAIN || errno == EWOULDBLOCK);
    CHECK(close(fds[0]) == 0);
    CHECK(close(fds[1]) == 0);
}

static void check_stream(const unsigned char *bytes, size_t byte_count,
                         const unsigned char (*expected)[3], size_t messages)
{
    struct mpc_midi_parser parser = {0};
    unsigned char message[3] = {0xaa, 0xaa, 0xaa};
    size_t received = 0;
    size_t i;

    for (i = 0; i < byte_count; ++i) {
        int result = mpc_midi_feed(&parser, bytes[i], message);
        CHECK(result == 0 || result == 1);
        if (result == 1) {
            CHECK(received < messages);
            CHECK(memcmp(message, expected[received], 3) == 0);
            ++received;
        }
    }
    CHECK(received == messages);
}

static void test_parser(void)
{
    static const unsigned char pads[] = {
        0x99, 37, 127, 36, 0, 0x89, 37, 0, 0xa9, 42, 50
    };
    static const unsigned char pad_messages[][3] = {
        {0x99, 37, 127}, {0x99, 36, 0}, {0x89, 37, 0}, {0xa9, 42, 50}
    };
    static const unsigned char buttons_knobs[] = {
        0x90, 6, 127, 0x80, 6, 0,
        0xb0, 19, 64, 16, 1, 17, 2, 18, 3, 20, 127, 100, 65
    };
    static const unsigned char control_messages[][3] = {
        {0x90, 6, 127}, {0x80, 6, 0}, {0xb0, 19, 64}, {0xb0, 16, 1},
        {0xb0, 17, 2}, {0xb0, 18, 3}, {0xb0, 20, 127}, {0xb0, 100, 65}
    };
    static const unsigned char realtime[] = {
        0xf8, 0x92, 0xf9, 10, 0xfa, 11, 0xfb, 12, 0xfc, 13,
        0xfd, 14, 0xfe, 15, 0xff, 16, 17
    };
    static const unsigned char realtime_messages[][3] = {
        {0x92, 10, 11}, {0x92, 12, 13}, {0x92, 14, 15}, {0x92, 16, 17}
    };
    static const unsigned char single_data[] = {
        0xc0, 5, 6, 0xd9, 127, 0xf8, 0, 0xef, 1, 2
    };
    static const unsigned char single_messages[][3] = {
        {0xc0, 5, 0}, {0xc0, 6, 0}, {0xd9, 127, 0}, {0xd9, 0, 0},
        {0xef, 1, 2}
    };
    static const unsigned char replaced_status[] = {
        10, 11, 0x90, 60, 0x81, 61, 1, 0x90, 0xc2, 9, 0x90, 70
    };
    static const unsigned char replaced_messages[][3] = {
        {0x81, 61, 1}, {0xc2, 9, 0}
    };
    static const unsigned char system[] = {
        0x90, 60, 1, 0xf0, 0x47, 0xf8, 0x7f, 0x00, 0xf7, 61, 2,
        0x91, 62, 3, 0xf1, 0x01, 63, 4,
        0x92, 64, 0xf2, 1, 2, 65, 5,
        0x93, 66, 6, 0xf3, 7, 8, 9,
        0x94, 67, 7, 0xf4, 68, 8,
        0x95, 69, 9, 0xf5, 70, 10,
        0x96, 71, 11, 0xf6, 72, 12,
        0x97, 73, 13, 0xf7, 74, 14
    };
    static const unsigned char system_messages[][3] = {
        {0x90, 60, 1}, {0x91, 62, 3}, {0x93, 66, 6},
        {0x94, 67, 7}, {0x95, 69, 9}, {0x96, 71, 11}, {0x97, 73, 13}
    };

    check_stream(pads, sizeof pads, pad_messages, sizeof pad_messages / 3);
    check_stream(buttons_knobs, sizeof buttons_knobs, control_messages,
                 sizeof control_messages / 3);
    check_stream(realtime, sizeof realtime, realtime_messages,
                 sizeof realtime_messages / 3);
    check_stream(single_data, sizeof single_data, single_messages,
                 sizeof single_messages / 3);
    check_stream(replaced_status, sizeof replaced_status, replaced_messages,
                 sizeof replaced_messages / 3);
    check_stream(system, sizeof system, system_messages, sizeof system_messages / 3);
}

int main(void)
{
    test_pads();
    test_output();
    test_invalid_arguments();
    test_full_pipe();
    test_parser();
#ifdef MPC_SURFACE_TEST_SYSCALLS
    test_write_errors();
    test_discovery();
#endif
    printf("surface tests passed (%u checks)\n", checks);
    return EXIT_SUCCESS;
}
