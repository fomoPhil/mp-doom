// MPC DOOM sound effects and original FM music: raw ALSA playback on a thread independent of SPI video.
// GPL-2.0-or-later. PCM setup follows MPC Sample Lab's documented raw ioctl path.
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include "mpc_sound.h"
#include "mpc_music.h"
#include "mpc_audio_route.h"

#define OUTPUT_RATE 44100
#define PERIOD 128
#define CHANNELS 16

struct sample { uint32_t length, rate; unsigned char bytes[]; };
struct voice { struct sample *sample; uint64_t position, step; int left, right; };
// Start near the previous 20% output; the first knob movement sets its position.
static atomic_int master_volume = 64;

void mpc_sound_set_master_volume(int value) {
    if (value < 0) value = 0;
    if (value > 127) value = 127;
    atomic_store(&master_volume, value);
}

static struct sample *decode_dmx(const unsigned char *data, size_t size) {
    if (size < 8 || data[0] != 3 || data[1] != 0) return NULL;
    uint32_t rate = data[2] | (uint32_t)data[3] << 8;
    uint32_t length = data[4] | (uint32_t)data[5] << 8 | (uint32_t)data[6] << 16 | (uint32_t)data[7] << 24;
    if (!rate || length <= 48 || length > size - 8) return NULL;
    // DMX ignores 16 padding bytes at either end of a valid sound lump.
    length -= 32;
    struct sample *sample = malloc(sizeof *sample + length);
    if (!sample) return NULL;
    sample->length = length;
    sample->rate = rate;
    memcpy(sample->bytes, data + 24, length);
    return sample;
}

static void set_volume(struct voice *voice, int volume, int separation) {
    if (volume < 0) volume = 0;
    if (volume > 127) volume = 127;
    if (separation < 0) separation = 0;
    if (separation > 254) separation = 254;
    // Fixed-point panning with a 40% output ceiling at maximum Main Volume.
    voice->left = (int)((int64_t)volume * (254 - separation) * 2 * 65536 / (127 * 254 * 5));
    voice->right = (int)((int64_t)volume * separation * 2 * 65536 / (127 * 254 * 5));
}

static void mix(struct voice voices[CHANNELS], const int16_t *music, int32_t *output, int frames) {
    int volume = atomic_load(&master_volume);
    int64_t gain = volume;
    for (int frame = 0; frame < frames; frame++) {
        // Match the SFX ceiling, leaving final saturation to the combined mix.
        int64_t left = music ? (int64_t)music[frame*2] * 65536 * 2 / 5 : 0;
        int64_t right = music ? (int64_t)music[frame*2+1] * 65536 * 2 / 5 : 0;
        for (int channel = 0; channel < CHANNELS; channel++) {
            struct voice *v = &voices[channel];
            if (!v->sample) continue;
            uint64_t index = v->position >> 16;
            if (index >= v->sample->length) { v->sample = NULL; continue; }
            int32_t value = ((int)v->sample->bytes[index] - 128) * 16777216;
            left += (int64_t)value * v->left / 65536;
            right += (int64_t)value * v->right / 65536;
            v->position += v->step;
            if ((v->position >> 16) >= v->sample->length) v->sample = NULL;
        }
        left = left * gain / 127;
        right = right * gain / 127;
        output[frame * 2] = left < INT32_MIN ? INT32_MIN : left > INT32_MAX ? INT32_MAX : (int32_t)left;
        output[frame * 2 + 1] = right < INT32_MIN ? INT32_MIN : right > INT32_MAX ? INT32_MAX : (int32_t)right;
    }
}

#ifdef SELFTEST
int main(void) {
    unsigned char dmx[72] = {3, 0, 0x11, 0x2b, 64, 0, 0, 0};
    memset(dmx + 8, 128, 64);
    dmx[24] = 255;
    struct sample *s = decode_dmx(dmx, sizeof dmx);
    assert(s && s->length == 32 && s->rate == 11025 && s->bytes[0] == 255);
    struct voice voices[16] = {0};
    int32_t out[2 * 128];
    memset(s->bytes, 255, s->length);
    voices[0] = (struct voice){.sample=s, .step=65536};
    set_volume(&voices[0], 127, 0);
    mpc_sound_set_master_volume(0);
    mix(voices, NULL, out, 1);
    assert(out[0] == 0 && out[1] == 0); // Main Volume must mute the PCM.
    assert(voices[0].position == 65536 && voices[0].sample == s);
    mpc_sound_set_master_volume(64);
    mix(voices, NULL, out, 1);
    int32_t quiet = out[0];
    assert(quiet > 0 && out[1] == 0);
    mpc_sound_set_master_volume(127);
    mix(voices, NULL, out, 1);
    int32_t full = out[0];
    assert(full > quiet && out[1] == 0 && voices[0].position == 3 * 65536);
    // Half travel should provide roughly half the maximum amplitude, not a quarter.
    assert(quiet >= (int64_t)full * 49 / 100 && quiet <= (int64_t)full * 52 / 100);
    mpc_sound_set_master_volume(-1);
    mix(voices, NULL, out, 1);
    assert(out[0] == 0 && out[1] == 0);
    mpc_sound_set_master_volume(1000);
    mix(voices, NULL, out, 1);
    assert(out[0] == full && out[1] == 0);
    for (int i = 0; i < CHANNELS; ++i) {
        voices[i] = (struct voice){.sample=s, .step=65536};
        set_volume(&voices[i], 127, 0);
    }
    mpc_sound_set_master_volume(16);
    mix(voices, NULL, out, 1);
    assert(out[0] == (int64_t)full * CHANNELS * 16 / 127);
    mpc_sound_set_master_volume(127);
    memset(voices, 0, sizeof voices);
    memset(s->bytes, 128, s->length);
    s->bytes[0] = 255;
    voices[0].sample = s;
    voices[0].step = ((uint64_t)s->rate << 16) / OUTPUT_RATE;
    set_volume(&voices[0], 127, 0);
    mix(voices, NULL, out, 128);
    assert(out[0] > 0 && out[1] == 0);
    assert(out[6] == out[0] && out[8] == 0);
    assert(!voices[0].sample);
    mix(voices, NULL, out, 128);
    for (int i = 0; i < 256; i++) assert(out[i] == 0);
    voices[0] = (struct voice){.sample=s, .step=65536};
    set_volume(&voices[0], 127, 254);
    mix(voices, NULL, out, 1);
    assert(out[0] == 0 && out[1] > 0);
    voices[0] = (struct voice){.sample=s, .step=65536};
    set_volume(&voices[0], 0, 127);
    mix(voices, NULL, out, 1);
    assert(out[0] == 0 && out[1] == 0);
    for (int i = 0; i < CHANNELS; i++) {
        voices[i] = (struct voice){.sample=s, .step=65536};
        set_volume(&voices[i], 127, 0);
    }
    mix(voices, NULL, out, 1);
    assert(out[0] == INT32_MAX && out[1] == 0);
    s->bytes[0] = 0;
    for (int i = 0; i < CHANNELS; i++) voices[i].position = 0;
    mix(voices, NULL, out, 1);
    assert(out[0] == INT32_MIN && out[1] == 0);
    dmx[4] = 65;
    assert(!decode_dmx(dmx, sizeof dmx));
    dmx[4] = 48;
    assert(!decode_dmx(dmx, sizeof dmx));
    dmx[4] = 64; dmx[2] = dmx[3] = 0;
    assert(!decode_dmx(dmx, sizeof dmx));
    // Music joins SFX before Main Volume; muting must affect both paths.
    memset(voices, 0, sizeof voices);
    int16_t tune[] = {16384, -16384};
    mpc_sound_set_master_volume(127);
    mix(voices, tune, out, 1);
    int32_t music_full=out[0];
    assert(music_full>0 && out[1]==-music_full);
    mpc_sound_set_master_volume(64);
    mix(voices, tune, out, 1);
    assert(out[0]==(int64_t)music_full*64/127);
    mpc_sound_set_master_volume(0);
    mix(voices, tune, out, 1);
    assert(out[0]==0 && out[1]==0);
    memset(s->bytes, 255, s->length);
    voices[0]=(struct voice){.sample=s,.step=65536};
    set_volume(&voices[0],127,0);
    mpc_sound_set_master_volume(127);
    mix(voices,tune,out,1);
    assert(out[0]==(int64_t)full+music_full && out[1]==-music_full);
    set_volume(&voices[0],0,0);
    mix(voices,tune,out,1);
    assert(out[0]==music_full && out[1]==-music_full); // SFX mute leaves music.
    free(s);
    puts("MPC sound mixer selftest ok");
    return 0;
}
#else
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sound/asound.h>
#include "i_sound.h"
#include "w_wad.h"
#include "z_zone.h"

int use_libsamplerate;
float libsamplerate_scale = 1.0f;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t audio_thread;
static atomic_int audio_running;
static int pcm_fd = -1, thread_started, use_prefix;
static int route_fd = -1;
static struct mpc_audio_route route;
static struct voice voices[CHANNELS];
static struct sample **samples;
static unsigned cache_entries;
static size_t cache_bytes;

static int route_read(void *context, enum mpc_route_control control, int *value) {
    static const char *names[] = {"Builtin Speaker Switch", "Headphones Jack", "Line Out Jack"};
    struct snd_ctl_elem_value item = {0};
    item.id.iface = control == MPC_ROUTE_SPEAKER ? SNDRV_CTL_ELEM_IFACE_MIXER : SNDRV_CTL_ELEM_IFACE_CARD;
    strcpy((char *)item.id.name, names[control]);
    if (ioctl(*(int *)context, SNDRV_CTL_IOCTL_ELEM_READ, &item) < 0) return -1;
    *value = !!item.value.integer.value[0];
    return 0;
}

static int route_write(void *context, enum mpc_route_control control, int value) {
    if (control != MPC_ROUTE_SPEAKER) { errno = EINVAL; return -1; }
    struct snd_ctl_elem_value item = {0};
    item.id.iface = SNDRV_CTL_ELEM_IFACE_MIXER;
    strcpy((char *)item.id.name, "Builtin Speaker Switch");
    item.value.integer.value[0] = !!value;
    if (ioctl(*(int *)context, SNDRV_CTL_IOCTL_ELEM_WRITE, &item) < 0) return -1;
    int actual;
    if (route_read(context, control, &actual) < 0) return -1;
    if (actual != !!value) { errno = EIO; return -1; }
    return 0;
}

static void route_close(void) {
    if (route_fd < 0) return;
    if (mpc_route_restore(&route) < 0) perror("restore speaker route");
    close(route_fd);
    route_fd = -1;
}

static int route_open(void) {
    route_fd = open("/dev/snd/by-path/platform-sound", O_RDWR | O_CLOEXEC);
    if (route_fd < 0) return -1;
    struct mpc_route_io io = {&route_fd, route_read, route_write};
    if (mpc_route_begin(&route, io) < 0) { route_close(); return -1; }
    printf("MPC speaker route: inherited=%d active=%d; headphone/line-out detection enabled\n", route.saved, route.current);
    return 0;
}

static void hw_mask(struct snd_pcm_hw_params *params, int field, unsigned value) {
    struct snd_mask *mask = &params->masks[field - SNDRV_PCM_HW_PARAM_FIRST_MASK];
    memset(mask, 0, sizeof *mask);
    mask->bits[value >> 5] = 1u << (value & 31);
}

static void hw_int(struct snd_pcm_hw_params *params, int field, unsigned value) {
    struct snd_interval *interval = &params->intervals[field - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
    interval->min = interval->max = value;
    interval->integer = 1;
}

static int pcm_open(void) {
    char link[64], path[64];
    ssize_t length = readlink("/dev/snd/by-path/platform-sound", link, sizeof link - 1);
    if (length <= 0) { perror("I2S card link"); return -1; }
    link[length] = 0;
    char *card = strstr(link, "controlC");
    if (!card || card[8] < '0' || card[8] > '9') return -1;
    snprintf(path, sizeof path, "/dev/snd/pcmC%dD0p", atoi(card + 8));
    int fd = open(path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) { perror(path); return -1; }
    struct snd_pcm_hw_params params = {0};
    for (int n = SNDRV_PCM_HW_PARAM_FIRST_MASK; n <= SNDRV_PCM_HW_PARAM_LAST_MASK; n++)
        memset(params.masks[n - SNDRV_PCM_HW_PARAM_FIRST_MASK].bits, 0xff, sizeof params.masks[0].bits);
    for (int n = SNDRV_PCM_HW_PARAM_FIRST_INTERVAL; n <= SNDRV_PCM_HW_PARAM_LAST_INTERVAL; n++)
        params.intervals[n - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL].max = ~0u;
    params.rmask = params.info = ~0u;
    hw_mask(&params, SNDRV_PCM_HW_PARAM_ACCESS, SNDRV_PCM_ACCESS_RW_INTERLEAVED);
    hw_mask(&params, SNDRV_PCM_HW_PARAM_FORMAT, SNDRV_PCM_FORMAT_S32_LE);
    hw_mask(&params, SNDRV_PCM_HW_PARAM_SUBFORMAT, SNDRV_PCM_SUBFORMAT_STD);
    hw_int(&params, SNDRV_PCM_HW_PARAM_CHANNELS, 2);
    hw_int(&params, SNDRV_PCM_HW_PARAM_RATE, OUTPUT_RATE);
    hw_int(&params, SNDRV_PCM_HW_PARAM_PERIOD_SIZE, PERIOD);
    hw_int(&params, SNDRV_PCM_HW_PARAM_BUFFER_SIZE, PERIOD * 3);
    if (ioctl(fd, SNDRV_PCM_IOCTL_HW_PARAMS, &params) < 0 || ioctl(fd, SNDRV_PCM_IOCTL_PREPARE) < 0) {
        perror("DOOM PCM setup"); close(fd); return -1;
    }
    return fd;
}

static int pcm_write(const int32_t *output, unsigned frames) {
    unsigned done = 0;
    while (done < frames && atomic_load(&audio_running)) {
        struct snd_xferi transfer = {.buf = (void *)(output + done * 2), .frames = frames - done};
        int status = ioctl(pcm_fd, SNDRV_PCM_IOCTL_WRITEI_FRAMES, &transfer);
        int error = status < 0 ? errno : transfer.result < 0 ? (int)-transfer.result : 0;
        if (error == EINTR) continue;
        if (error == EPIPE) {
            if (ioctl(pcm_fd, SNDRV_PCM_IOCTL_PREPARE) < 0) return -1;
            continue;
        }
        if (error == EAGAIN || (!error && transfer.result == 0)) {
            struct pollfd pfd = {.fd = pcm_fd, .events = POLLOUT};
            int ready = poll(&pfd, 1, 50);
            if (ready < 0 && errno != EINTR) return -1;
            if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                if (ioctl(pcm_fd, SNDRV_PCM_IOCTL_PREPARE) < 0) return -1;
            }
            continue;
        }
        if (error) { errno = error; return -1; }
        if ((unsigned long)transfer.result > frames - done) return -1;
        done += (unsigned)transfer.result;
    }
    return 0;
}

static void *audio_main(void *unused) {
    (void)unused;
    int32_t output[PERIOD * 2];
    int16_t music[PERIOD * 2];
    while (atomic_load(&audio_running)) {
        mpc_music_render(music, PERIOD);
        pthread_mutex_lock(&lock);
        mix(voices, music, output, PERIOD);
        pthread_mutex_unlock(&lock);
        if (pcm_write(output, PERIOD) < 0) { perror("DOOM PCM write"); break; }
    }
    atomic_store(&audio_running, 0);
    return NULL;
}

static boolean sound_init(boolean prefix) {
    use_prefix = prefix;
    cache_entries = numlumps;
    samples = calloc(cache_entries, sizeof *samples);
    if (!samples) return false;
    pcm_fd = pcm_open();
    if (pcm_fd < 0) { free(samples); samples = NULL; return false; }
    if (route_open() < 0) {
        perror("DOOM speaker route");
        close(pcm_fd); pcm_fd = -1; free(samples); samples = NULL;
        return false;
    }
    atomic_store(&audio_running, 1);
    int error = pthread_create(&audio_thread, NULL, audio_main, NULL);
    if (error) {
        fprintf(stderr, "DOOM audio thread: %s\n", strerror(error));
        atomic_store(&audio_running, 0);
        close(pcm_fd); pcm_fd = -1; route_close(); free(samples); samples = NULL;
        return false;
    }
    thread_started = 1;
    puts("DOOM sound effects: I2S 44100Hz, independent audio thread; MAIN VOLUME=CC19, linear knob, 40% output ceiling, about 20% until first knob movement; original WAD FM music shares this PCM output");
    return true;
}

static void sound_shutdown(void) {
    atomic_store(&audio_running, 0);
    if (thread_started) pthread_join(audio_thread, NULL);
    thread_started = 0;
    if (pcm_fd >= 0) { ioctl(pcm_fd, SNDRV_PCM_IOCTL_DROP); close(pcm_fd); pcm_fd = -1; }
    route_close();
    memset(voices, 0, sizeof voices);
    if (samples) {
        for (unsigned i = 0; i < cache_entries; i++) free(samples[i]);
        free(samples); samples = NULL;
    }
    cache_entries = 0; cache_bytes = 0;
}

static int get_lump(sfxinfo_t *sfx) {
    char name[9];
    if (sfx->link) sfx = sfx->link;
    snprintf(name, sizeof name, "%s%.6s", use_prefix ? "ds" : "", sfx->name);
    return W_GetNumForName(name);
}

static struct sample *cache_sound(int lump) {
    if (lump < 0 || (unsigned)lump >= cache_entries || !samples) return NULL;
    if (samples[lump]) return samples[lump];
    int length = W_LumpLength((unsigned)lump);
    // The first build keeps every owned cache sample until shutdown, capped at 64MB.
    if (length <= 8 || (size_t)length > 64u * 1024 * 1024 - cache_bytes) return NULL;
    const unsigned char *data = W_CacheLumpNum(lump, PU_STATIC);
    struct sample *sample = decode_dmx(data, (size_t)length);
    W_ReleaseLumpNum(lump);
    if (sample) { samples[lump] = sample; cache_bytes += sizeof *sample + sample->length; }
    return sample;
}

static int start_sound(sfxinfo_t *sfx, int channel, int volume, int separation) {
    if (channel < 0 || channel >= CHANNELS || !atomic_load(&audio_running)) return -1;
    // Main thread owns the WAD/zone API. The audio thread sees only malloc-owned samples.
    struct sample *sample = cache_sound(sfx->lumpnum);
    if (!sample) return -1;
    pthread_mutex_lock(&lock);
    voices[channel] = (struct voice){.sample = sample, .step = ((uint64_t)sample->rate << 16) / OUTPUT_RATE};
    set_volume(&voices[channel], volume, separation);
    pthread_mutex_unlock(&lock);
    return channel;
}

static void stop_sound(int channel) {
    if (channel < 0 || channel >= CHANNELS) return;
    pthread_mutex_lock(&lock);
    voices[channel].sample = NULL;
    pthread_mutex_unlock(&lock);
}

static void update_params(int channel, int volume, int separation) {
    if (channel < 0 || channel >= CHANNELS) return;
    pthread_mutex_lock(&lock);
    set_volume(&voices[channel], volume, separation);
    pthread_mutex_unlock(&lock);
}

static boolean playing(int channel) {
    if (channel < 0 || channel >= CHANNELS || !atomic_load(&audio_running)) return false;
    pthread_mutex_lock(&lock);
    boolean result = voices[channel].sample != NULL;
    pthread_mutex_unlock(&lock);
    return result;
}

static void update(void) {
    if (route_fd >= 0 && mpc_route_refresh(&route) < 0)
        perror("refresh speaker route");
}
static void precache(sfxinfo_t *sounds, int count) { (void)sounds; (void)count; }
static snddevice_t devices[] = {SNDDEVICE_SB, SNDDEVICE_PAS, SNDDEVICE_GUS, SNDDEVICE_WAVEBLASTER, SNDDEVICE_SOUNDCANVAS, SNDDEVICE_AWE32};
sound_module_t DG_sound_module = {devices, sizeof devices / sizeof devices[0], sound_init, sound_shutdown,
    get_lump, update, update_params, start_sound, stop_sound, playing, precache};

#endif
