/* Exercise the production DG_Init path with hardware replaced by stubs. */
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "doomgeneric.h"
#include "d_event.h"
#include "i_sound.h"
#include "mpc_display.h"
#include "mpc_input.h"
#include "mpc_sound.h"
#include "mpc_opening_pixels.inc"

static struct timespec flushed_at;
static int polls;
static int power_released;
pixel_t *DG_ScreenBuffer;

int mpc_input_open(void) { return 0; }
void mpc_input_close(void) { puts("CLOSE_INPUT"); }
int mpc_display_open(void) { return 0; }
void mpc_display_close(void) { puts("CLOSE_DISPLAY"); }

int mpc_display_native_frame(const uint32_t *pixels)
{
    int nonzero = 0;
    for (size_t i = 0; i < 320 * 240; ++i) {
        if (pixels[i] != mpc_opening_pixels[i]) abort();
        nonzero |= pixels[i] != 0;
    }
    if (!nonzero) abort();
    if (clock_gettime(CLOCK_MONOTONIC, &flushed_at) < 0) abort();
    puts("FLUSHED_REAL_IMAGE");
    return getenv("FLUSH_FAIL") ? -1 : 0;
}

int mpc_display_frame(const uint32_t *pixels)
{
    (void)pixels;
    abort();
}

void mpc_input_poll_power(void)
{
    ++polls;
    if (polls == 3 && getenv("POWER")) power_released = 1;
    if (polls == 3 && getenv("SIGNAL")) raise(SIGTERM);
}

int mpc_input_quit(void) { return power_released; }
int mpc_input_key(int *down, unsigned char *key)
{
    (void)down;
    (void)key;
    abort();
}
int mpc_input_turn(void) { return 0; }
int mpc_input_volume(void) { return -1; }
void mpc_sound_set_master_volume(int volume) { (void)volume; }
void D_PostEvent(event_t *event) { (void)event; abort(); }
void I_ShutdownSound(void) {}
void doomgeneric_Create(int argc, char **argv) { (void)argc; (void)argv; }
void doomgeneric_Tick(void) {}

int main(void)
{
    struct timespec now;
    setvbuf(stdout, NULL, _IONBF, 0);
    DG_Init();
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) return 1;
    long elapsed_ms = (now.tv_sec - flushed_at.tv_sec) * 1000 +
                      (now.tv_nsec - flushed_at.tv_nsec) / 1000000;
    printf("ELAPSED_MS %ld\n", elapsed_ms);
    return elapsed_ms < 1000 || elapsed_ms > 2000 || polls < 3;
}
