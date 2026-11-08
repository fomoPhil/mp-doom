// SPDX-License-Identifier: GPL-2.0-or-later
// Included in generated m_menu.c. Text is ours; glyphs come from the loaded WAD.
#include "mpc_status.h"

struct mpc_help_page {
    const char *title;
    const char *rows[10][2];
    unsigned rows_count;
    const char *notes[3];
    unsigned notes_count;
    const char *save_rows[4][2];
    unsigned save_rows_count;
};
static const struct mpc_help_page mpc_help_pages[] = {
#include "mpc_help_data.inc"
};
static int mpc_help_page;
static byte mpc_help_ink[256], mpc_help_colors[5];
static boolean mpc_help_palette_ready;

static boolean MPC_HelpActive(void)
{
    return menuactive && !messageToPrint && !saveStringEnter
        && (currentMenu == &ReadDef1 || currentMenu == &ReadDef2);
}
static void MPC_HelpBegin(void) { mpc_help_page = 0; }
static void MPC_HelpLeave(boolean main_menu)
{
    mpc_status_invalidate();
    if (main_menu) M_SetupNextMenu(&MainDef);
    else M_ClearMenus();
}
static void MPC_HelpNext(void)
{
    if (mpc_help_page < 2) ++mpc_help_page;
    else MPC_HelpLeave(true);
}
static void MPC_HelpStep(int delta)
{
    mpc_help_page += delta;
    if (mpc_help_page < 0) mpc_help_page = 0;
    if (mpc_help_page > 2) mpc_help_page = 2;
}
static boolean MPC_HelpRespond(event_t *ev)
{
    if (!MPC_HelpActive()) return false;
    // Releases must continue through the existing gameplay and encoder handlers.
    if (ev->type == ev_keyup) return false;
    if (ev->type == ev_mouse) {
        MPC_HelpStep(ev->data2 / 128);
        return true;
    }
    if (ev->type != ev_keydown) return false;
    int key = ev->data1;
    if (key == MPC_KEY_ENCODER_PUSH || key == KEY_ENTER) MPC_HelpNext();
    else if (key == key_menu_up || key == key_menu_left) MPC_HelpStep(-1);
    else if (key == key_menu_down || key == key_menu_right) MPC_HelpStep(1);
    else if (key == KEY_BACKSPACE) {
        if (mpc_help_page) --mpc_help_page;
        else MPC_HelpLeave(true);
    } else if (key == KEY_ESCAPE) MPC_HelpLeave(false);
    return true;
}

static void MPC_HelpPalette(void)
{
    if (mpc_help_palette_ready) return;
    const byte *palette = W_CacheLumpName("PLAYPAL", PU_CACHE);
    static const int colors[5][3] = {
        {23,20,22}, {228,184,117}, {237,225,204}, {227,138,114}, {183,172,167}
    };
    for (int i = 0; i < 256; ++i)
        mpc_help_ink[i] = palette[i*3] > 100 || palette[i*3+1] > 100 || palette[i*3+2] > 100;
    for (int color = 0; color < 5; ++color) {
        int best = 0x7fffffff;
        for (int i = 0; i < 256; ++i) {
            int distance = 0;
            for (int channel = 0; channel < 3; ++channel) {
                int delta = palette[i*3+channel] - colors[color][channel];
                distance += delta * delta;
            }
            if (distance < best) { best = distance; mpc_help_colors[color] = i; }
        }
    }
    mpc_help_palette_ready = true;
}

static void MPC_HelpText(const char *text, int x, int y, int color, int scale)
{
    int advance = 0;
    for (; *text; ++text) {
        int ch = *text;
        if (ch == ' ') { advance += 4; continue; }
        patch_t *glyph = hu_font[ch - HU_FONTSTART];
        for (int column = 0; column < SHORT(glyph->width); ++column) {
            const column_t *post = (const column_t *)((const byte *)glyph + LONG(glyph->columnofs[column]));
            while (post->topdelta != 255) {
                const byte *pixels = (const byte *)post + 3;
                for (int row = 0; row < post->length; ++row) {
                    if (!mpc_help_ink[pixels[row]]) continue;
                    int gx = advance + column - SHORT(glyph->leftoffset);
                    int gy = post->topdelta + row - SHORT(glyph->topoffset);
                    for (int py = y + gy*scale/2; py < y + (gy+1)*scale/2; ++py)
                        for (int px = x + gx*scale/2; px < x + (gx+1)*scale/2; ++px)
                            if (px >= 0 && px < SCREENWIDTH && py >= 0 && py < SCREENHEIGHT)
                                I_VideoBuffer[py*SCREENWIDTH+px] = mpc_help_colors[color];
                }
                post = (const column_t *)((const byte *)post + post->length + 4);
            }
        }
        advance += SHORT(glyph->width);
    }
}
static void MPC_HelpRow(const char *const row[2], int y)
{
    MPC_HelpText(row[0],12,y,1,2);
    MPC_HelpText(row[1],132,y,2,2);
}
static void MPC_HelpDraw(void)
{
    MPC_HelpPalette();
    inhelpscreens = true;
    const struct mpc_help_page *page = &mpc_help_pages[mpc_help_page];
    V_DrawFilledBox(0,0,320,200,mpc_help_colors[0]);
    MPC_HelpText("MPC SAMPLE / DOOM",12,10,1,3);
    char number[] = {(char)('1'+mpc_help_page),'/', '3',0};
    MPC_HelpText(number,283,14,4,2);
    MPC_HelpText(page->title,12,31,3,2);
    V_DrawHorizLine(12,43,296,mpc_help_colors[4]);
    for (unsigned i=0; i<page->rows_count; ++i) MPC_HelpRow(page->rows[i],51+i*11);
    if (page->save_rows_count) {
        MPC_HelpText("SAVE NAMES",12,119,3,2);
        for (unsigned i=0; i<page->save_rows_count; ++i) MPC_HelpRow(page->save_rows[i],130+i*11);
    } else {
        if (mpc_help_page == 1)
            MPC_HelpText("MP DOOM / BY PHIL WOOLLEY / MPC SAMPLE PORT",7,128,4,2);
        int y = mpc_help_page == 0 ? 163 : 139;
        for (unsigned i=0; i<page->notes_count; ++i) MPC_HelpText(page->notes[i],12,y+i*11,4,2);
    }
    V_DrawHorizLine(12,175,296,mpc_help_colors[4]);
    MPC_HelpText(mpc_help_page == 2 ? "TURN: PAGE  PLAY/PUSH: DONE" : "TURN: PAGE  PLAY/PUSH: NEXT",12,179,1,2);
    MPC_HelpText("STOP: BACK  SEQ: CLOSE",12,190,4,2);
}
