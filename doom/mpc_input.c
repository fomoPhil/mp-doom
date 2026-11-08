/* Physical MPC inputs, contextual STOP Back and power-release exit. */
#include "mpc_input.h"
#include "mpc_input_logic.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/input.h>
#include <poll.h>
#include <sound/asound.h>
#include <sys/ioctl.h>

static struct controls controls;
static struct mpc_midi_parser parser;
static int surface = -1, power_key = -1;

static void light(int pad, int down)
{
    int key = control_pad_key(pad);
    int level = down ? 110 : 18;
    if (!key) mpc_surface_pad_color(surface, pad, 0, 0, 0);
    else if (key == KEY_FIRE) mpc_surface_pad_color(surface, pad, level, 0, 0);
    else if (key == KEY_USE) mpc_surface_pad_color(surface, pad, level, level / 2, 0);
    else if (key >= KEY_STRAFE_L && key <= KEY_DOWNARROW && key != KEY_TAB && pad >= 4 && pad <= 9)
        mpc_surface_pad_color(surface, pad, 0, level, level / 2);
    else mpc_surface_pad_color(surface, pad, 0, 0, level);
}

void mpc_input_close(void)
{
    if (surface >= 0) { mpc_surface_clear(surface); close(surface); surface = -1; }
    if (power_key >= 0) { close(power_key); power_key = -1; }
}

int mpc_input_open(void)
{
    surface = mpc_surface_open(O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (surface < 0) { perror("MPC surface open"); return -1; }
    // Stock uses timestamp framing, which survives closing the raw MIDI port.
    // Our parser accepts MIDI bytes, so explicitly remove timestamp headers.
    struct snd_rawmidi_params params = {
        .stream = SNDRV_RAWMIDI_STREAM_INPUT,
        .buffer_size = 4096, .avail_min = 1,
        .mode = SNDRV_RAWMIDI_MODE_FRAMING_NONE | SNDRV_RAWMIDI_MODE_CLOCK_NONE
    };
    if (ioctl(surface, SNDRV_RAWMIDI_IOCTL_PARAMS, &params) < 0) {
        perror("plain MIDI input setup");
        mpc_input_close();
        return -1;
    }
    power_key = open("/dev/input/by-path/platform-fdd40000.i2c-platform-rk805-pwrkey.2.auto-event", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (power_key < 0) { perror("power key"); mpc_input_close(); return -1; }
    mpc_surface_clear(surface);
    for (int p = 0; p < 16; ++p) light(p, 0);
    mpc_surface_led(surface, 81, 8);
    puts("MPC input: encoder=aim/push-fire, push=select in menus; pad4=fire; pads 5/6/7/10=movement; SHIFT=run toggle; PLAY=select; STOP=back; PLUS=yes MINUS=no; Power=stock");
    return 0;
}

static void pump_power(short revents)
{
    if (revents & (POLLERR | POLLHUP | POLLNVAL)) controls.quit = 1;
    if (revents & POLLIN) {
        struct input_event events[16];
        ssize_t n = read(power_key, events, sizeof events);
        if (n == 0 || (n < 0 && errno != EINTR && errno != EAGAIN)) controls.quit = 1;
        for (ssize_t i = 0; i < n / (ssize_t)sizeof events[0]; ++i)
            if (control_power(events[i].type, events[i].code, events[i].value)) {
                puts("Power released: returning to stock");
                controls.quit = 1;
            }
    }
}

void mpc_input_poll_power(void)
{
    struct pollfd fd = {power_key, POLLIN, 0};
    int ready = poll(&fd, 1, 0);
    if (ready < 0) { if (errno != EINTR) controls.quit = 1; return; }
    pump_power(fd.revents);
}

static void pump(void)
{

    struct pollfd fds[] = {{surface, POLLIN, 0}, {power_key, POLLIN, 0}};
    int ready = poll(fds, 2, 0);
    if (ready < 0) { if (errno != EINTR) controls.quit = 1; return; }
    if ((fds[0].revents | fds[1].revents) & (POLLERR | POLLHUP | POLLNVAL)) controls.quit = 1;
    pump_power(fds[1].revents);
    if (fds[0].revents & POLLIN) {
        unsigned char data[512];
        ssize_t n = read(surface, data, sizeof data);
        if (n == 0 || (n < 0 && errno != EINTR && errno != EAGAIN)) controls.quit = 1;
        unsigned char message[3];
        for (ssize_t i = 0; i < n; ++i) if (mpc_midi_feed(&parser, data[i], message) == 1) {
            control_message(&controls, message);

            if (message[0] == 0x99 || message[0] == 0x89) {
                int p = mpc_pad_index(message[1]);
                if (p >= 0) light(p, message[0] == 0x99 && message[2] > 0);
            }
        }
    }
}

int mpc_input_key(int *down, unsigned char *key)
{
    pump();
    return control_pop(&controls, down, key);
}
int mpc_input_turn(void) { int turn = controls.turn; controls.turn = 0; return turn; }
int mpc_input_volume(void) { return control_take_volume(&controls); }
int mpc_input_quit(void) { return controls.quit; }
