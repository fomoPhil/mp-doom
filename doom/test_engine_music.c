// Native full-engine music checks; no hardware/ALSA device access.
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "doomgeneric.h"
#include "i_sound.h"
#include "s_sound.h"
#include "w_wad.h"
#include "z_zone.h"
#include "mpc_music.h"

// The native test bypasses only hardware SFX output, never the music backend.
static boolean sound_init(boolean prefix) { (void)prefix; return true; }
static void no_op(void) {}
static int lump(sfxinfo_t *sfx) { (void)sfx; return 0; }
static void params(int c,int v,int p) { (void)c;(void)v;(void)p; }
static int start(sfxinfo_t *s,int c,int v,int p) { (void)s;(void)v;(void)p;return c; }
static void stop(int c) { (void)c; }
static boolean playing(int c) { (void)c;return false; }
static void cache(sfxinfo_t *s,int n) { (void)s;(void)n; }
sound_module_t DG_sound_module={NULL,0,sound_init,no_op,lump,no_op,params,start,stop,playing,cache};
int use_libsamplerate;
float libsamplerate_scale=1.0f;

static uint64_t render(unsigned frames, FILE *wav) {
    int16_t pcm[256*2]; uint64_t energy=0;
    while(frames) {
        unsigned count=frames>256?256:frames;
        mpc_music_render(pcm,count);
        for(unsigned i=0;i<count*2;++i) energy += abs((int)pcm[i]);
        if(wav) assert(fwrite(pcm,sizeof(*pcm)*2,count,wav)==count);
        frames-=count;
    }
    return energy;
}
static void *song(char *name) {
    int n=W_GetNumForName(name);
    void *data=W_CacheLumpNum(n,PU_STATIC);
    void *handle=I_RegisterSong(data,W_LumpLength(n));
    W_ReleaseLumpNum(n); assert(handle);return handle;
}
static void *background(void *unused) {
    (void)unused;
    for(int i=0;i<3000;++i) render(128,NULL);
    return NULL;
}
static void wav_header(FILE *f,unsigned frames) {
    uint32_t values[]={36+frames*4,16,44100,44100*4,frames*4};
    uint16_t pcm=1,channels=2,align=4,bits=16;
    assert(fwrite("RIFF",1,4,f)==4);fwrite(values,4,1,f);fwrite("WAVEfmt ",1,8,f);
    fwrite(values+1,4,1,f);fwrite(&pcm,2,1,f);fwrite(&channels,2,1,f);
    fwrite(values+2,4,2,f);fwrite(&align,2,1,f);fwrite(&bits,2,1,f);
    fwrite("data",1,4,f);fwrite(values+4,4,1,f);
}
int main(int argc,char **argv) {
    doomgeneric_Create(argc,argv);
    S_StopMusic();
    void *handle=song("D_E1M1");
    I_SetMusicVolume(127);I_PlaySong(handle,true);
    assert(I_MusicIsPlaying());
    FILE *wav=fopen("original-e1m1-fm.wav","wb");assert(wav);wav_header(wav,44100*10);
    uint64_t loud=render(44100*10,wav);assert(!fclose(wav));assert(loud>1000000);
    I_SetMusicVolume(0); render(44100,NULL);
    assert(render(44100,NULL)==0 && "Music volume zero must mute all voices");
    I_SetMusicVolume(127);assert(render(44100*2,NULL)>1000000);
    I_PauseSong();render(44100*5,NULL);
    assert(render(44100,NULL)==0 && "Paused song must stop sounding after release tails");
    I_ResumeSong();assert(render(44100*3,NULL)>1000000);
    I_StopSong();I_UnRegisterSong(handle);
    assert(!I_MusicIsPlaying());
    // Exercise every actual song in whichever WAD is supplied.
    unsigned tracks=0;
    for(unsigned n=0;n<numlumps;++n) {
        if(strncmp(lumpinfo[n].name,"D_",2)) continue;
        if(W_LumpLength(n)<4) continue;
        byte *data=W_CacheLumpNum(n,PU_STATIC);
        if(memcmp(data,"MUS\032",4)) { W_ReleaseLumpNum(n);continue; }
        handle=I_RegisterSong(data,W_LumpLength(n));W_ReleaseLumpNum(n);assert(handle);
        I_PlaySong(handle,true);uint64_t energy=render(44100*4,NULL);
        printf("MUSIC_TRACK %.8s energy=%llu\n",lumpinfo[n].name,(unsigned long long)energy);
        assert(energy>10000);I_StopSong();I_UnRegisterSong(handle);++tracks;
    }
    assert(tracks>=13);
    // A short MIDI exercises real event scheduling, loop restart and end state.
    unsigned char midi[]={ 'M','T','h','d',0,0,0,6,0,0,0,1,0,70,
        'M','T','r','k',0,0,0,12, 0,0x90,60,100, 14,0x80,60,0, 0,0xff,0x2f,0 };
    handle=I_RegisterSong(midi,sizeof midi);assert(handle);
    I_PlaySong(handle,true);assert(render(44100*3,NULL)>1000000);
    assert(I_MusicIsPlaying());I_StopSong();
    I_PlaySong(handle,false);render(44100,NULL);assert(!I_MusicIsPlaying());
    I_StopSong();I_UnRegisterSong(handle);
    pthread_t thread;assert(!pthread_create(&thread,NULL,background,NULL));
    for(int i=0;i<30;++i) {
        handle=song(i%2?"D_E1M1":"D_E1M2");I_PlaySong(handle,true);
        I_SetMusicVolume(i*4);I_PauseSong();I_ResumeSong();I_StopSong();I_UnRegisterSong(handle);
    }
    // doomgeneric's I_ShutdownMusic is an empty stub; runtime shutdown calls
    // the module directly via I_ShutdownSound. Exercise the actual backend.
    DG_music_module.Shutdown();assert(!pthread_join(thread,NULL));DG_music_module.Shutdown();
    assert(render(256,NULL)==0);
    puts("MUSIC_ALL_OK: original WAD songs, volume, pause/resume, loops, track switch, threaded shutdown");
    return 0;
}
