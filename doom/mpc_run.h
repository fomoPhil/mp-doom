// SPDX-License-Identifier: GPL-2.0-or-later
// Included in generated g_game.c. Run mode lasts until toggled or app exit.
static boolean mpc_run_enabled;
static boolean mpc_shift_pressed;
extern boolean message_dontfuckwithme;

static boolean MPC_RunRespond(event_t *ev)
{
    if (ev->data1 != KEY_RSHIFT) return false;
    if (ev->type == ev_keyup) {
        mpc_shift_pressed = false;
        return true;
    }
    if (ev->type != ev_keydown) return false;
    if (!mpc_shift_pressed && gamestate == GS_LEVEL && !menuactive) {
        mpc_run_enabled = !mpc_run_enabled;
        players[consoleplayer].message = mpc_run_enabled ? "RUN ON" : "RUN OFF";
        // Use the engine's existing important-message path, even with messages off.
        message_dontfuckwithme = true;
    }
    mpc_shift_pressed = true;
    return true;
}
