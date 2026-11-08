// GPL-2.0-or-later. Original WAD music rendered into the existing PCM mixer.
#pragma once
#include <stdint.h>

// Stereo signed 16-bit, 44100 Hz. Serialized internally with song controls.
void mpc_music_render(int16_t *output, unsigned frames);
