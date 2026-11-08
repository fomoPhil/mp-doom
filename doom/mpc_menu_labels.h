// SPDX-License-Identifier: GPL-2.0-or-later
// Use the loaded game font, scaled 3:2, for the longer MPC-specific label.
// No game artwork is embedded in the executable.
static void MPC_DrawKnobLookLabel(int x, int y)
{
    const char *text = "KNOB LOOK SENSITIVITY";
    int advance = 0;
    while (*text)
    {
        int c = *text++;
        if (c == ' ') { advance += 4; continue; }
        patch_t *glyph = hu_font[c - HU_FONTSTART];
        for (int column = 0; column < SHORT(glyph->width); ++column)
        {
            const column_t *post = (const column_t *)((const byte *)glyph + LONG(glyph->columnofs[column]));
            while (post->topdelta != 255)
            {
                const byte *pixels = (const byte *)post + 3;
                for (int row = 0; row < post->length; ++row)
                {
                    int gx = advance + column - SHORT(glyph->leftoffset);
                    int gy = post->topdelta + row - SHORT(glyph->topoffset);
                    for (int py = y + gy * 3 / 2; py < y + (gy + 1) * 3 / 2; ++py)
                        for (int px = x + gx * 3 / 2; px < x + (gx + 1) * 3 / 2; ++px)
                            if (px >= 0 && px < SCREENWIDTH && py >= 0 && py < SCREENHEIGHT)
                                I_VideoBuffer[py * SCREENWIDTH + px] = pixels[row];
                }
                post = (const column_t *)((const byte *)post + post->length + 4);
            }
        }
        advance += SHORT(glyph->width);
    }
}
