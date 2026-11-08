// SPDX-License-Identifier: GPL-2.0-or-later
// Included in generated m_menu.c; dispatch through the normal menu actions.
static menu_t *mpc_adjust_menu;
static int mpc_adjust_item;

static boolean MPC_SliderActive(void)
{
    if (!menuactive || messageToPrint || saveStringEnter
        || currentMenu != mpc_adjust_menu || itemOn != mpc_adjust_item)
        mpc_adjust_menu = NULL;
    return mpc_adjust_menu != NULL;
}

static boolean MPC_MenuEncoderDraw(void)
{
    if (!MPC_SliderActive()) return false;
    int x = currentMenu->x;
    int y = currentMenu->y + (itemOn + 1) * LINEHEIGHT;
    int width, dot;
    if (currentMenu == &SoundDef) {
        width = 16;
        dot = itemOn == sfx_vol ? sfxVolume : musicVolume;
    } else {
        width = itemOn == scrnsize ? 9 : 10;
        dot = itemOn == scrnsize ? screenSize : mouseSensitivity;
    }
    // Keep the native artwork, highlighting its rail and thumb while active.
    V_DrawHorizLine(x + 1, y + 6, (width + 2) * 8 - 2, 54);
    V_DrawBox(x + 7 + dot * 8, y, 10, 15, 54);
    for (int i = 0; i < 5; ++i)
        V_DrawVertLine(x - 12 + i, y + 2 + i, 9 - 2 * i, 54);
    M_WriteText(x + (width + 2) * 8 + 9, y + 3, "ADJUST");
    return true; // Replace the large label cursor with the small slider cursor.
}

static boolean MPC_MenuEncoderRespond(event_t *ev)
{
    boolean adjusting = MPC_SliderActive();
    if (ev->type == ev_keyup && ev->data1 == MPC_KEY_ENCODER_PUSH)
    {
        // A shot may have begun before a menu or name editor opened. Always
        // release its independent gameplay key, even when the UI owns presses.
        G_Responder(ev);
        return true;
    }
    if (saveStringEnter) return false; // Name editing owns turn and push here.
    if (ev->type == ev_keydown && ev->data1 == MPC_KEY_ENCODER_PUSH)
    {
        if (menuactive || messageToPrint)
        {
            event_t select = {.type = ev_keydown, .data1 = KEY_ENTER, .data2 = KEY_ENTER};
            M_Responder(&select);
            return true;
        }
        return false; // Gameplay owns this second, independently held fire key.
    }
    if (ev->type == ev_keydown && menuactive && !messageToPrint)
    {
        if (ev->data1 == KEY_ENTER
            && (currentMenu == &OptionsDef || currentMenu == &SoundDef)
            && currentMenu->menuitems[itemOn].status == 2)
        {
            mpc_adjust_menu = adjusting ? NULL : currentMenu;
            mpc_adjust_item = itemOn;
            return true; // Enter/finish without changing the value.
        }
        if (adjusting)
        {
            if (ev->data1 == KEY_BACKSPACE) {
                mpc_adjust_menu = NULL;
                return true; // STOP/ERASE finishes; a second press goes back.
            }
            if (ev->data1 == KEY_ESCAPE || ev->data1 == key_menu_up || ev->data1 == key_menu_down)
                mpc_adjust_menu = NULL; // SEQ closes; pads can navigate immediately.
        }
    }
    if (ev->type == ev_mouse && (menuactive || messageToPrint))
    {
        if (messageToPrint) return true; // Rotation cannot answer a dialog.
        int delta = ev->data2 / 128;
        event_t step = {.type = ev_keydown,
            .data1 = adjusting
                ? (delta > 0 ? key_menu_right : key_menu_left)
                : (delta > 0 ? key_menu_down : key_menu_up)};
        int steps = delta < 0 ? -delta : delta;
        for (int i = 0; i < steps; ++i) M_Responder(&step);
        return true;
    }
    return false; // Gameplay still receives relative mouse movement for aiming.
}
