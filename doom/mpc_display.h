/* MPC Sample direct DRM display. Caller must stop ac50 before opening. */
#pragma once
#include <stdint.h>

int mpc_display_open(void);
/* Present a 320x200 XRGB8888 Doom frame as 320x240, correcting its 4:3 aspect. */
int mpc_display_frame(const uint32_t *pixels);
/* Present native 320x240 XRGB8888 pixels without scaling. */
int mpc_display_native_frame(const uint32_t *pixels);
void mpc_display_close(void);
