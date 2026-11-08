/* doomgeneric platform entry point. Linux runtime writes belong in /run/mpc-doom. */
#include "doomgeneric.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include "doomkeys.h"

static volatile sig_atomic_t quitting;
static struct timespec started;
static unsigned frames;
static void on_signal(int number) { (void)number; quitting = 1; }

#if !defined(MPC_HEADLESS) || defined(MPC_INPUT_BRIDGE_TEST)
#include "mpc_input.h"
#include "mpc_sound.h"
#include "d_event.h"
#endif
#ifndef MPC_HEADLESS
#include "mpc_display.h"
#include "i_sound.h"
#include "mpc_opening_pixels.inc"
static void cleanup(void)
{
    I_ShutdownSound();
    mpc_input_close();
    mpc_display_close();
    printf("MPC_DOOM_EXIT frames=%u\n", frames);
}
#endif

void DG_Init(void)
{
    clock_gettime(CLOCK_MONOTONIC, &started);
    struct sigaction action = {0};
    action.sa_handler = on_signal;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGHUP, &action, NULL);
#ifndef MPC_HEADLESS
    atexit(cleanup);
    if (mpc_input_open() < 0 || mpc_display_open() < 0) exit(1);
    if (mpc_display_native_frame(mpc_opening_pixels) < 0) exit(1);
    struct timespec opening_start, now;
    if (clock_gettime(CLOCK_MONOTONIC, &opening_start) < 0) exit(1);
    do {
        mpc_input_poll_power();
        if (quitting || mpc_input_quit()) exit(0);
        if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) exit(1);
        if (now.tv_sec - opening_start.tv_sec > 1 ||
            (now.tv_sec - opening_start.tv_sec == 1 && now.tv_nsec >= opening_start.tv_nsec)) break;
        struct timespec delay = {0, 10000000L};
        while (nanosleep(&delay, &delay) < 0 && errno == EINTR && !quitting) {}
    } while (!quitting);
    if (quitting) exit(0);
#endif
    puts("MPC_DOOM_INITIALIZED");
}

void DG_DrawFrame(void)
{
    ++frames;
#ifdef MPC_HEADLESS
    if (frames == 120 && !getenv("MPC_TEST_QUIT")) {
        FILE *out = fopen("gameplay.ppm", "wb");
        if (!out) { perror("gameplay.ppm"); exit(1); }
        fprintf(out, "P6\n320 200\n255\n");
        for (unsigned i = 0; i < 320 * 200; ++i) {
            uint32_t pixel = DG_ScreenBuffer[i];
            unsigned char rgb[] = {pixel >> 16, pixel >> 8, pixel};
            if (fwrite(rgb, 1, 3, out) != 3) exit(1);
        }
        if (fclose(out)) exit(1);
        puts("HEADLESS_GAMEPLAY_FRAME_OK");
        quitting = 1;
    }
#else
    if (mpc_display_frame(DG_ScreenBuffer) < 0) quitting = 1;
#endif
}

uint32_t DG_GetTicksMs(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint32_t)((now.tv_sec - started.tv_sec) * 1000 + (now.tv_nsec - started.tv_nsec) / 1000000);
}

void DG_SleepMs(uint32_t ms)
{
    struct timespec delay = {ms / 1000, (ms % 1000) * 1000000L};
    while (nanosleep(&delay, &delay) < 0 && errno == EINTR && !quitting) {}
}

int DG_GetKey(int *pressed, unsigned char *key)
{
#if defined(MPC_HEADLESS) && !defined(MPC_INPUT_BRIDGE_TEST)
    static unsigned step;
    if (getenv("MPC_TEST_QUIT") && frames >= 120 && step < 4) {
        *pressed = !(step & 1);
        *key = step < 2 ? KEY_F10 : 'y';
        ++step;
        return 1;
    }
    return 0;
#else
    // I_GetEvent drains keys before DOOM processes its 64-entry event ring.
    // Yield at 32 events, retaining queued input for the next engine poll.
    static unsigned events;
    for (;;) {
        if (events == 32) { events = 0; return 0; }
        int result = mpc_input_key(pressed, key);
        int volume = mpc_input_volume();
        if (volume >= 0) mpc_sound_set_master_volume(volume);
        if (mpc_input_quit()) quitting = 1;
        int delta = mpc_input_turn();
        if (delta) {
            event_t event = {.type=ev_mouse, .data2=delta * 128};
            D_PostEvent(&event);
            ++events;
            continue;
        }
        if (result) ++events;
        else events = 0;
        return result;
    }
#endif
}

void DG_SetWindowTitle(const char *title) { printf("DOOM_TITLE %s\n", title); }

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    doomgeneric_Create(argc, argv);
    while (!quitting) doomgeneric_Tick();
    return 0;
}
