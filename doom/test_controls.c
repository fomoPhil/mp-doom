#include <assert.h>
#include <stdio.h>
#include "mpc_input_logic.h"

int main(void)
{
    struct controls c = {0};
    int down; unsigned char key;
    assert(control_take_volume(&c) == -1);
    const unsigned char mute[] = {0xB0, 19, 0};
    control_message(&c, mute);
    assert(control_take_volume(&c) == 0); // Main Volume fully left must mute.
    assert(control_take_volume(&c) == -1);
    const unsigned char volume[] = {0xB0, 19, 64};
    control_message(&c, volume);
    assert(control_take_volume(&c) == 64);
    const unsigned char maximum[] = {0xB0, 19, 127};
    control_message(&c, maximum);
    assert(control_take_volume(&c) == 127);
    const unsigned char other_channel[] = {0xB1, 19, 0};
    const unsigned char other_knob[] = {0xB0, 16, 0};
    control_message(&c, other_channel);
    control_message(&c, other_knob);
    assert(control_take_volume(&c) == -1 && c.turn == 0 && c.count == 0);
    const int swapped_pads[] = {11, 15}; // Physical pads 12 and 16.
    const unsigned char swapped_keys[] = {KEY_TAB, '4'};
    for (int i = 0; i < 2; ++i) {
        unsigned char message[] = {0x99, mpc_pad_notes[swapped_pads[i]], 127};
        control_message(&c, message);
        assert(control_pop(&c, &down, &key) && down && key == swapped_keys[i]);
        message[0] = 0x89; message[2] = 0;
        control_message(&c, message);
        assert(control_pop(&c, &down, &key) && !down && key == swapped_keys[i]);
    }
    const unsigned char forward[] = {0x99, 47, 100}; // pad10
    const unsigned char release[] = {0x89, 47, 0};
    control_message(&c, forward);
    assert(control_pop(&c, &down, &key) && down && key == KEY_UPARROW);
    assert(!control_pop(&c, &down, &key));
    control_message(&c, forward); // duplicate down should not repeat
    assert(!control_pop(&c, &down, &key));
    control_message(&c, release);
    assert(control_pop(&c, &down, &key) && !down && key == KEY_UPARROW);
    const unsigned char left[] = {0x99, 40, 99}; // pad5 strafe
    const unsigned char right[] = {0x99, 46, 99}; // pad7 strafe
    control_message(&c, left); control_message(&c, right);
    assert(control_pop(&c, &down, &key) && down && key == KEY_STRAFE_L);
    assert(control_pop(&c, &down, &key) && down && key == KEY_STRAFE_R);
    const unsigned char turn_right[] = {0xB0,100,1}, turn_left[] = {0xB0,100,127};
    control_message(&c, turn_right);
    assert(control_pop(&c,&down,&key) && !down && key==0 && c.turn==1);
    control_message(&c, turn_left);
    assert(control_pop(&c,&down,&key) && !down && key==0 && c.turn==-1);
    const unsigned char pressure[] = {0xA9,47,20};
    control_message(&c, pressure); assert(!control_pop(&c,&down,&key));
    const unsigned char fire[] = {0x99,82,127}, fire_release[] = {0x99,82,0};
    control_message(&c,fire); assert(control_pop(&c,&down,&key) && down && key==KEY_FIRE);
    control_message(&c,fire_release); assert(control_pop(&c,&down,&key) && !down && key==KEY_FIRE);
    const unsigned char stop_release[] = {0x80,81,0}, stop[] = {0x90,81,127};
    control_message(&c,stop_release); assert(!c.quit);
    control_message(&c,stop); assert(!c.quit);
    assert(control_pop(&c,&down,&key) && down && key==KEY_F12);
    assert(control_power(1,116,0) && !control_power(1,116,1) && !control_power(1,116,2));
    assert(!control_power(1,115,0));
    c = (struct controls){0};
    control_message(&c,turn_right);
    const unsigned char encoder_push[] = {0x90,111,127}, encoder_up[] = {0x80,111,0};
    control_message(&c,encoder_push);
    assert(control_pop(&c,&down,&key) && key==0 && c.turn==1);
    assert(control_pop(&c,&down,&key) && down && key==0xf0);
    control_message(&c,encoder_push); assert(!control_pop(&c,&down,&key));
    control_message(&c,encoder_up);
    assert(control_pop(&c,&down,&key) && !down && key==0xf0);
    const unsigned char plus[] = {0x90,54,127}, minus[] = {0x90,55,127}, erase[] = {0x90,9,127};
    c = (struct controls){0};
    control_message(&c,plus); assert(control_pop(&c,&down,&key) && down && key=='y');
    control_message(&c,minus); assert(control_pop(&c,&down,&key) && down && key=='n');
    control_message(&c,erase); assert(control_pop(&c,&down,&key) && down && key==KEY_BACKSPACE);
    assert(control_pad_key(8)==KEY_LEFTARROW && control_pad_key(10)==KEY_RIGHTARROW);
    c = (struct controls){0};
    for (int i=0;i<129;i++) control_queue(&c, i & 1, KEY_FIRE);
    assert(c.quit); // overflow exits safely instead of leaving held keys stuck
    puts("DOOM controls press/release/encoder/exit tests passed");
}
