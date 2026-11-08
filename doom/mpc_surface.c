/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _POSIX_C_SOURCE 200809L
#include "mpc_surface.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

const unsigned char mpc_pad_notes[16] = {
    37, 36, 42, 82, 40, 38, 46, 44,
    48, 47, 45, 43, 49, 55, 51, 53
};

int mpc_pad_index(unsigned char note)
{
    int pad;
    for (pad = 0; pad < 16; ++pad) {
        if (mpc_pad_notes[pad] == note)
            return pad;
    }
    return -1;
}

int mpc_surface_open(int flags)
{
    static const char endpoint[] =
        "/dev/snd/by-path/platform-fe670000.serial";
    char target[4096];
    char device[64];
    const char *name;
    const char *digit;
    ssize_t length;
    unsigned int card = 0;
    int result;

    if (flags & O_CREAT) {
        errno = EINVAL;
        return -1;
    }
    do {
        length = readlink(endpoint, target, sizeof target - 1);
    } while (length < 0 && errno == EINTR);
    if (length < 0)
        return -1;
    if ((size_t)length == sizeof target - 1) {
        errno = ENAMETOOLONG;
        return -1;
    }
    target[length] = '\0';
    name = strrchr(target, '/');
    name = name ? name + 1 : target;
    if (strncmp(name, "controlC", 8) != 0 || name[8] == '\0') {
        errno = EINVAL;
        return -1;
    }
    for (digit = name + 8; *digit; ++digit) {
        unsigned int value;
        if (*digit < '0' || *digit > '9') {
            errno = EINVAL;
            return -1;
        }
        value = (unsigned int)(*digit - '0');
        if (card > ((unsigned int)INT_MAX - value) / 10) {
            errno = EINVAL;
            return -1;
        }
        card = card * 10 + value;
    }
    result = snprintf(device, sizeof device, "/dev/snd/midiC%uD0", card);
    if (result < 0 || (size_t)result >= sizeof device) {
        errno = ENAMETOOLONG;
        return -1;
    }
    do {
        result = open(device, flags);
    } while (result < 0 && errno == EINTR);
    return result;
}

static int write_packet(int fd, const unsigned char *bytes, size_t length)
{
    size_t sent = 0;
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    while (sent < length) {
        ssize_t result = write(fd, bytes + sent, length - sent);
        if (result > 0) {
            sent += (size_t)result;
        } else if (result < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        } else {
            errno = EIO;
            return -1;
        }
    }
    return 0;
}

static int midi_value(int value)
{
    return value >= 0 && value <= 127;
}

int mpc_surface_led(int fd, int note, int value)
{
    unsigned char packet[3];
    if (!midi_value(note) || !midi_value(value)) {
        errno = EINVAL;
        return -1;
    }
    packet[0] = 0xb0;
    packet[1] = (unsigned char)note;
    packet[2] = (unsigned char)value;
    return write_packet(fd, packet, sizeof packet);
}

int mpc_surface_pad_color(int fd, int pad, int red, int green, int blue)
{
    unsigned char packet[12] = {
        0xf0, 0x47, 0x7f, 0x5c, 0x65, 0, 4, 0, 0, 0, 0, 0xf7
    };
    if (pad < 0 || pad >= 16 || !midi_value(red) ||
        !midi_value(green) || !midi_value(blue)) {
        errno = EINVAL;
        return -1;
    }
    packet[7] = (unsigned char)pad;
    packet[8] = (unsigned char)red;
    packet[9] = (unsigned char)green;
    packet[10] = (unsigned char)blue;
    return write_packet(fd, packet, sizeof packet);
}

int mpc_surface_clear(int fd)
{
    static const unsigned char buttons[24] = {
        6, 7, 1, 0, 49, 35, 9, 11, 12, 14, 75, 74,
        73, 55, 54, 13, 36, 40, 43, 53, 80, 81, 82, 111
    };
    size_t button;
    int pad;

    for (button = 0; button < sizeof buttons; ++button) {
        if (mpc_surface_led(fd, buttons[button], 0) < 0)
            return -1;
    }
    for (pad = 0; pad < 16; ++pad) {
        if (mpc_surface_pad_color(fd, pad, 0, 0, 0) < 0)
            return -1;
    }
    return 0;
}

int mpc_midi_feed(struct mpc_midi_parser *parser, unsigned char byte,
                  unsigned char message[3])
{
    unsigned char needed;

    if (!parser || !message) {
        errno = EINVAL;
        return -1;
    }
    if (byte >= 0xf8)
        return 0;
    if (byte & 0x80) {
        parser->status = byte < 0xf0 ? byte : 0;
        parser->data_count = 0;
        return 0;
    }
    if (parser->status == 0)
        return 0;

    needed = ((parser->status & 0xe0) == 0xc0) ? 1 : 2;
    parser->data[parser->data_count++] = byte;
    if (parser->data_count < needed)
        return 0;

    message[0] = parser->status;
    message[1] = parser->data[0];
    message[2] = needed == 2 ? parser->data[1] : 0;
    parser->data_count = 0;
    return 1;
}
