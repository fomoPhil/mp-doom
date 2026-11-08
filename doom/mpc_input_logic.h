#pragma once
#include "doomkeys.h"
#include "mpc_surface.h"
#include "mpc_name_keys.h"

struct controls {
    struct { unsigned char down, key; signed char turn; } queue[128];
    unsigned read, write, count;
    unsigned char held[256];
    int turn, quit;
    int volume, volume_changed;
};

// F12 is reserved as an internal STOP token; it is translated before engine input.
struct back_binding { unsigned char emitted; };
static inline unsigned char control_back(struct back_binding *binding, int down,
    unsigned char key, int menu, int main_menu, int editing, int prompt)
{
    if (key != KEY_F12) return key;
    if (down) binding->emitted = (!menu || main_menu || editing || prompt) ? KEY_ESCAPE : KEY_BACKSPACE;
    return binding->emitted;
}

static inline int control_pad_key(int pad)
{
    static const unsigned char keys[16] = {'5','6','7',KEY_FIRE,KEY_STRAFE_L,KEY_DOWNARROW,
        KEY_STRAFE_R,KEY_USE,KEY_LEFTARROW,KEY_UPARROW,KEY_RIGHTARROW,KEY_TAB,'1','2','3','4'};
    return pad >= 0 && pad < 16 ? keys[pad] : 0;
}

static inline void control_queue(struct controls *c, int down, unsigned char key)
{
    if (c->count == 128) { c->quit = 1; return; }
    c->queue[c->write].down = !!down;
    c->queue[c->write].key = key;
    c->queue[c->write].turn = 0;
    c->write = (c->write + 1) % 128;
    ++c->count;
}

static inline int control_pop(struct controls *c, int *down, unsigned char *key)
{
    if (!c->count) return 0;
    *down = c->queue[c->read].down;
    *key = c->queue[c->read].key;
    c->turn = c->queue[c->read].turn;
    c->read = (c->read + 1) % 128;
    --c->count;
    return 1;
}

static inline int control_take_volume(struct controls *c)
{
    if (!c->volume_changed) return -1;
    c->volume_changed = 0;
    return c->volume;
}

static inline void control_message(struct controls *c, const unsigned char *m)
{
    int key = 0, down = 0;
    if (m[0] == 0xB0 && m[1] == 19) {
        c->volume = m[2];
        c->volume_changed = 1;
        return;
    }
    if (m[0] == 0xB0 && m[1] == 100) {
        int delta = m[2] <= 64 ? m[2] : (int)m[2] - 128;
        // Queue turns alongside edges: turn-then-push must not become push-then-turn.
        if (delta) {
            if (c->count == 128) { c->quit = 1; return; }
            control_queue(c, 0, 0);
            c->queue[(c->write + 127) % 128].turn = delta;
        }
        return;
    }
    if (m[0] == 0x99 || m[0] == 0x89) {
        key = control_pad_key(mpc_pad_index(m[1]));
        down = m[0] == 0x99 && m[2] > 0;
    } else if (m[0] == 0x90 || m[0] == 0x80) {
        down = m[0] == 0x90 && m[2] > 0;
        if (m[1] == 81) key = KEY_F12; // STOP: contextual Back, never direct quit
        if (m[1] == 82) key = KEY_ENTER; // PLAY
        if (m[1] == 7) key = KEY_ESCAPE; // SEQ
        if (m[1] == 49) key = KEY_RSHIFT;
        if (m[1] == 54) key = 'y'; // PLUS: confirm Y/N dialogs
        if (m[1] == 55) key = 'n'; // MINUS: decline Y/N dialogs
        if (m[1] == 9) key = KEY_BACKSPACE; // ERASE: parent menu/delete character
        if (m[1] == 111) key = MPC_KEY_ENCODER_PUSH;
    }
    if (key && c->held[key] != down) {
        c->held[key] = down;
        control_queue(c, down, key);
    }
}

static inline int control_power(int type, int code, int value)
{
    return type == 1 && code == 116 && value == 0;
}
