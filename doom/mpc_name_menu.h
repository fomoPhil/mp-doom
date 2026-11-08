// SPDX-License-Identifier: GPL-2.0-or-later
// Included only in the generated m_menu.c, after its font and save helpers.
// Uses the engine's existing slot buffer, cancel snapshot and checked save path.
#include "mpc_status.h"
static const char mpc_name_characters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -.";
static int mpc_name_selected;
static char *mpc_name_notice;

static void MPC_NameBegin(void)
{
    mpc_name_selected = 0;
    mpc_name_notice = NULL;
}

static void MPC_NameCenter(char *text, int y)
{
    M_WriteText((SCREENWIDTH - M_StringWidth(text)) / 2, y, text);
}

static void MPC_NameDraw(void)
{
    char prefix[SAVESTRINGSIZE], label[32];
    int offset, count = sizeof(mpc_name_characters) - 1;
    V_DrawFilledBox(0, 0, 320, 200, 5);
    V_DrawPatchDirect(72, 10, W_CacheLumpName(DEH_String("M_SAVEG"), PU_CACHE));
    M_snprintf(label, sizeof(label), "SLOT %d: NAME", saveSlot + 1);
    M_WriteText(18, 33, label);
    M_snprintf(label, sizeof(label), "%d/%d", (int)strlen(savegamestrings[saveSlot]), SAVESTRINGSIZE - 1);
    M_WriteText(302 - M_StringWidth(label), 33, label);
    V_DrawFilledBox(17, 45, 286, 28, 0);
    V_DrawBox(17, 45, 286, 28, 143);
    M_WriteText(24, 54, savegamestrings[saveSlot]);
    memcpy(prefix, savegamestrings[saveSlot], saveCharIndex);
    prefix[saveCharIndex] = '\0';
    V_DrawHorizLine(24 + M_StringWidth(prefix), 66, 8, 54);
    MPC_NameCenter("TURN TO CHOOSE - PUSH TO ENTER", 82);
    for (offset = -3; offset <= 3; ++offset)
    {
        char letter[2] = {mpc_name_characters[(mpc_name_selected + offset + count) % count], 0};
        char *shown = letter[0] == ' ' ? "SPACE" : letter;
        int x = 160 + offset * 37;
        if (offset == 0) V_DrawFilledBox(x - 21, 99, 42, 28, 54);
        M_WriteText(x - M_StringWidth(shown) / 2, 109, shown);
    }
    if (mpc_name_notice) MPC_NameCenter(mpc_name_notice, 138);
    else
    {
        char letter = mpc_name_characters[mpc_name_selected];
        if (letter == ' ') M_StringCopy(label, "PUSH TO ENTER SPACE", sizeof(label));
        else M_snprintf(label, sizeof(label), "PUSH TO ENTER %c", letter);
        MPC_NameCenter(label, 138);
    }
    V_DrawFilledBox(0, 153, 320, 47, 0);
    M_WriteText(18, 162, "ERASE: DELETE");
    M_WriteText(302 - M_StringWidth("PLAY: SAVE"), 162, "PLAY: SAVE");
    M_WriteText(18, 182, "-/+: CURSOR");
    M_WriteText(302 - M_StringWidth("STOP: CANCEL"), 182, "STOP: CANCEL");
}

static void MPC_NamePut(int ch)
{
    char proposed[SAVESTRINGSIZE];
    int length = strlen(savegamestrings[saveSlot]);
    if (saveCharIndex == length && length == SAVESTRINGSIZE - 1)
    {
        mpc_name_notice = "NAME IS FULL";
        return;
    }
    M_StringCopy(proposed, savegamestrings[saveSlot], sizeof(proposed));
    proposed[saveCharIndex] = ch;
    if (saveCharIndex == length) proposed[length + 1] = '\0';
    // Keep the saved description inside the existing Save/Load slot border.
    if (M_StringWidth(proposed) > (SAVESTRINGSIZE - 2) * 8)
    {
        mpc_name_notice = "NAME IS FULL";
        return;
    }
    M_StringCopy(savegamestrings[saveSlot], proposed, SAVESTRINGSIZE);
    ++saveCharIndex;
}

static boolean MPC_NameRespond(event_t *ev)
{
    int key, count = sizeof(mpc_name_characters) - 1;
    if (!saveStringEnter) return false;
    if (ev->type == ev_mouse)
    {
        int delta = ev->data2 / 128;
        mpc_name_selected = (mpc_name_selected + delta % count + count) % count;
        if (delta) mpc_name_notice = NULL;
        return true;
    }
    // Let ordinary key releases reach gameplay, so held movement cannot stick.
    if (ev->type != ev_keydown) return false;
    key = ev->data1;
    mpc_name_notice = NULL;
    switch (key)
    {
        case MPC_KEY_ENCODER_PUSH:
            MPC_NamePut(mpc_name_characters[mpc_name_selected]);
            break;
        case 'y': // PLUS
        case 'n': // MINUS
            if (key == 'y' && savegamestrings[saveSlot][saveCharIndex]) ++saveCharIndex;
            if (key == 'n' && saveCharIndex) --saveCharIndex;
            if (savegamestrings[saveSlot][saveCharIndex])
            {
                char *found = strchr(mpc_name_characters, savegamestrings[saveSlot][saveCharIndex]);
                if (found) mpc_name_selected = found - mpc_name_characters;
            }
            break;
        case KEY_BACKSPACE:
            if (saveCharIndex)
            {
                char *name = savegamestrings[saveSlot];
                memmove(name + saveCharIndex - 1, name + saveCharIndex,
                        strlen(name + saveCharIndex) + 1);
                --saveCharIndex;
            }
            break;
        case KEY_ESCAPE:
            M_StringCopy(savegamestrings[saveSlot], saveOldString, SAVESTRINGSIZE);
            mpc_status_invalidate();
            saveStringEnter = 0;
            break;
        case KEY_ENTER:
        {
            char *name = savegamestrings[saveSlot];
            while (*name == ' ') ++name;
            if (!*name) mpc_name_notice = "ENTER A NAME FIRST";
            else
            {
                mpc_status_invalidate();
                saveStringEnter = 0;
                M_DoSave(saveSlot);
            }
            break;
        }
        default:
        {
            int ch = toupper(vanilla_keyboard_mapping ? key : ev->data2);
            if (ch >= 32 && ch < 127 && strchr(mpc_name_characters, ch)) MPC_NamePut(ch);
            break;
        }
    }
    return true;
}
