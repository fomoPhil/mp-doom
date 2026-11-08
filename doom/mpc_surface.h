/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MPC_SURFACE_H
#define MPC_SURFACE_H

#ifdef __cplusplus
extern "C" {
#endif

extern const unsigned char mpc_pad_notes[16];

/* Return the zero-based pad index, or -1 when the note is not a pad. */
int mpc_pad_index(unsigned char note);

/* Resolve the control endpoint's card and open its raw MIDI device.
 * O_CREAT is rejected; all other flags are passed to open(). */
int mpc_surface_open(int flags);

/* Values and colors are 0..127. Pad indices are 0..15.
 * These return 0 on success, or -1 with errno set. A failed write can
 * leave a partial packet on the descriptor. EAGAIN is never retried. */
int mpc_surface_led(int fd, int note, int value);
int mpc_surface_pad_color(int fd, int pad, int red, int green, int blue);
int mpc_surface_clear(int fd);

/* Initialize with {0}. Parser state belongs to one MIDI byte stream. */
struct mpc_midi_parser {
    unsigned char status;
    unsigned char data[2];
    unsigned char data_count;
};

/* Return 1 for a complete channel message, 0 otherwise, or -1/EINVAL
 * for a null argument. C0/D0 messages have a zero third byte.
 * System and realtime messages are not returned. */
int mpc_midi_feed(struct mpc_midi_parser *parser, unsigned char byte,
                  unsigned char message[3]);

#ifdef __cplusplus
}
#endif

#endif
