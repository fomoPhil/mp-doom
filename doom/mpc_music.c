// GPL-2.0-or-later. MPC transport for Chocolate Doom's GPL FM music driver.
// The generated driver retains its original copyright and license notice.
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "i_sound.h"
#include "opl.h"
#include "opl3.h"
#include "opl_queue.h"
#include "mpc_music.h"

// One owner at a time for driver, emulator and callbacks. No WAD/zone calls
// occur in timer callbacks. Registration/instrument caching stays on the game
// thread, and shutdown cannot release tracks while the audio thread uses them.
static pthread_mutex_t music_lock = PTHREAD_MUTEX_INITIALIZER;
static opl3_chip chip;
static opl_callback_queue_t *queue;
static uint64_t sample_clock;
static const unsigned sample_rate = 44100;
static int paused, initialized;

static uint64_t music_time(void) { return sample_clock * OPL_SECOND / sample_rate; }
// The transport always matches the existing 44100 Hz PCM stream.
void OPL_SetSampleRate(unsigned rate) { (void)rate; }
opl_init_result_t OPL_Init(unsigned port) {
    (void)port;
    queue = OPL_Queue_Create();
    OPL3_Reset(&chip, sample_rate);
    sample_clock = 0; paused = 0; initialized = 1;
    return OPL_INIT_OPL2; // Original DOS Sound Blaster's nine FM voices.
}
void OPL_Shutdown(void) {
    if (queue) OPL_Queue_Destroy(queue);
    queue = NULL; initialized = 0;
}
void OPL_WriteRegister(int reg, int value) { OPL3_WriteRegBuffered(&chip, reg, value); }
void OPL_SetCallback(uint64_t us, opl_callback_t callback, void *data) {
    OPL_Queue_Push(queue, callback, data, music_time() + us);
}
void OPL_ClearCallbacks(void) { OPL_Queue_Clear(queue); }
void OPL_AdjustCallbacks(float factor) { OPL_Queue_AdjustCallbacks(queue, music_time(), factor); }
void OPL_SetPaused(int value) { paused = value; }
// Chocolate's inner critical sections are covered by the outer wrapper lock.
// This transport is private to this driver; it never runs a second timer thread.
void OPL_Lock(void) {}
void OPL_Unlock(void) {}

// Same register reset sequence as the original driver. No hardware port I/O.
void OPL_InitRegisters(int opl3) {
    (void)opl3;
    for (int r=OPL_REGS_LEVEL; r<=OPL_REGS_LEVEL+OPL_NUM_OPERATORS; ++r) OPL_WriteRegister(r,63);
    for (int r=OPL_REGS_ATTACK; r<=OPL_REGS_WAVEFORM+OPL_NUM_OPERATORS; ++r) OPL_WriteRegister(r,0);
    for (int r=1; r<OPL_REGS_LEVEL; ++r) OPL_WriteRegister(r,0);
    OPL_WriteRegister(OPL_REG_TIMER_CTRL,0x60);
    OPL_WriteRegister(OPL_REG_TIMER_CTRL,0x80);
    OPL_WriteRegister(OPL_REG_WAVEFORM_ENABLE,0x20);
    OPL_WriteRegister(OPL_REG_FM_MODE,0x40);
}

typedef enum { opl_doom1_1_666, opl_doom2_1_666, opl_doom_1_9 } opl_driver_ver_t;
// Generated from pinned upstream by doom_music_sources.py, with compatibility
// and in-memory MIDI parsing patches. It is not a second music module.
#include "mpc_oplmusic.inc"

void mpc_music_render(int16_t *output, unsigned frames) {
    pthread_mutex_lock(&music_lock);
    memset(output, 0, frames * 2 * sizeof *output);
    if (!initialized) { pthread_mutex_unlock(&music_lock); return; }
    unsigned filled = 0;
    while (filled < frames) {
        if (!paused) {
            // Callbacks can schedule more callbacks at this exact time.
            while (!OPL_Queue_IsEmpty(queue) && OPL_Queue_Peek(queue) <= music_time()) {
                opl_callback_t callback; void *data;
                OPL_Queue_Pop(queue, &callback, &data);
                callback(data);
            }
        }
        unsigned count = frames - filled;
        if (!paused && !OPL_Queue_IsEmpty(queue)) {
            uint64_t delta = OPL_Queue_Peek(queue) - music_time();
            uint64_t until = (delta * sample_rate + OPL_SECOND - 1) / OPL_SECOND;
            if (until < count) count = (unsigned)until;
        }
        OPL3_GenerateStream(&chip, output + filled*2, count);
        filled += count;
        if (!paused) sample_clock += count;
    }
    // FM maximum attenuation still leaves a faint residual. A zero setting
    // must be digital silence while timing continues for later unmuting.
    if (!current_music_volume || paused) memset(output, 0, frames * 2 * sizeof *output);
    pthread_mutex_unlock(&music_lock);
}

static boolean init(void) {
    pthread_mutex_lock(&music_lock);
    boolean result = I_OPL_InitMusic();
    pthread_mutex_unlock(&music_lock);
    if (result) puts("DOOM music: original WAD MUS + GENMIDI, nine-voice Sound Blaster FM, 44100Hz");
    return result;
}
static void shutdown_music(void) { pthread_mutex_lock(&music_lock); I_OPL_ShutdownMusic(); pthread_mutex_unlock(&music_lock); }
static void volume(int value) {
    if (value<0) value=0;
    if (value>127) value=127;
    pthread_mutex_lock(&music_lock);
    I_OPL_SetMusicVolume(value);
    pthread_mutex_unlock(&music_lock);
}
static void pause_music(void) { pthread_mutex_lock(&music_lock); I_OPL_PauseSong(); pthread_mutex_unlock(&music_lock); }
static void resume_music(void) { pthread_mutex_lock(&music_lock); I_OPL_ResumeSong(); pthread_mutex_unlock(&music_lock); }
static void *register_song(void *data, int length) {
    pthread_mutex_lock(&music_lock);
    void *handle = I_OPL_RegisterSong(data,length);
    pthread_mutex_unlock(&music_lock);
    return handle;
}
static void unregister_song(void *handle) { pthread_mutex_lock(&music_lock); I_OPL_UnRegisterSong(handle); pthread_mutex_unlock(&music_lock); }
static void play_song(void *handle, boolean loop) { pthread_mutex_lock(&music_lock); I_OPL_PlaySong(handle,loop); pthread_mutex_unlock(&music_lock); }
static void stop_song(void) { pthread_mutex_lock(&music_lock); I_OPL_StopSong(); pthread_mutex_unlock(&music_lock); }
static boolean is_playing(void) { pthread_mutex_lock(&music_lock); boolean result=music_initialized && num_tracks>0 && running_tracks>0; pthread_mutex_unlock(&music_lock); return result; }
music_module_t DG_music_module = {NULL,0,init,shutdown_music,volume,pause_music,resume_music,
    register_song,unregister_song,play_song,stop_song,is_playing,NULL};
